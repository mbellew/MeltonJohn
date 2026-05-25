// Platform implementation for Electro-Smith Daisy Patch.
// Audio is captured via the built-in WM8731 codec and processed with
// CMSIS-DSP FFT (arm_rfft_fast_f32) to produce bass/mid/treb spectrum
// data.  DMX output uses Serial1 (the MIDI TRS UART) at 250 kbaud via
// an external MAX3485 RS-485 driver — see README for the circuit.

#include "DaisyMelton.h"
#include <DaisyDuino.h>
#include <U8g2lib.h>
#include <math.h>
#include <stdlib.h>
#include "Patterns.h"
#include "fftsg.hpp"

using namespace daisy;
using namespace daisysp;

#if DEBUG_LOG
#define LOG_print(x)   Serial.print(x)
#define LOG_println(x) Serial.println(x)
#else
#define LOG_print(x)
#define LOG_println(x)
#endif


// ─── Abstract spectrum analyzer interface ─────────────────────────────────────

class SpectrumAnalyzer
{
public:
    virtual bool next(Spectrum &spectrum) = 0;
};


// ─── Circular audio buffer ────────────────────────────────────────────────────
// AudioCallback (ISR) writes samples here; the main loop reads snapshots.
// CIRC_LEN must be a power of 2 so index wrapping uses & instead of %.

static const uint32_t CIRC_LEN = FFT_SIZE * 2;
static const uint32_t HOP_SIZE = FFT_SIZE / 2;   // 50% overlap → ~62 fps @ 8 kHz

static float circBuf[2][CIRC_LEN];        // [0=L / 1=R][sample index]
static volatile uint32_t g_writeIdx = 0;  // monotonically increasing sample count

static void AudioCallback(float **in, float **out, size_t size)
{
    uint32_t base = g_writeIdx;
    for (size_t i = 0; i < size; i++)
    {
        uint32_t idx      = (base + i) & (CIRC_LEN - 1);
        circBuf[0][idx]   = in[0][i];
        circBuf[1][idx]   = in[1][i];
    }
    g_writeIdx = base + (uint32_t)size;

    // This sketch produces no audio output.
    for (size_t i = 0; i < size; i++)
        out[0][i] = out[1][i] = 0.0f;
}


// ─── DaisySpectrumAnalyzer ────────────────────────────────────────────────────
// Runs two independent FFTs (one per channel) so stereo phase differences
// cannot cause frequency cancellation.  Bin magnitudes are summed after
// the FFT, which is always additive regardless of phase.

class DaisySpectrumAnalyzer : public SpectrumAnalyzer
{
    float    hannWindow[FFT_SIZE];
    float    fftWork[FFT_SIZE];     // scratch for one channel at a time

    // fftsg work arrays — ip[0]=0 triggers initialization on first rdft call
    int      fftIp[20]         = {};
    float    fftW[FFT_SIZE / 2] = {};

    // Auto-level state — mirrors the Teensy SoundFFT approach.
    float    vol_level = 20.0f;
    float    att[3]    = {20.0f, 20.0f, 20.0f};  // bass / mid / treb

    uint32_t lastFFTIdx = 0;

    static float bandAvg(const float *mag, int lo, int hi)
    {
        float sum = 0.0f;
        for (int i = lo; i <= hi; i++)
            sum += mag[i];
        return sum / (float)(hi - lo + 1);
    }

public:
    void begin()
    {
        for (int i = 0; i < FFT_SIZE; i++)
            hannWindow[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (FFT_SIZE - 1)));
    }

    bool next(Spectrum &s) override
    {
        uint32_t current = g_writeIdx;
        if ((current - lastFFTIdx) < HOP_SIZE)
            return false;
        lastFFTIdx = current;

        // Accumulate magnitudes from both channels — phase-agnostic.
        float mag[FFT_SIZE / 2] = {};

        for (int ch = 0; ch < 2; ch++)
        {
            // Window samples into scratch buffer.
            for (int i = 0; i < FFT_SIZE; i++)
            {
                uint32_t idx = (uint32_t)(current - FFT_SIZE + i) & (CIRC_LEN - 1);
                fftWork[i] = circBuf[ch][idx] * hannWindow[i];
            }

            // Forward real DFT (in-place).
            // Output: fftWork[2*k]=R[k], fftWork[2*k+1]=I[k] for k>=1.
            rdft(FFT_SIZE, 1, fftWork, fftIp, fftW);

            // Accumulate magnitudes for bins 1..FFT_SIZE/2-1 (skip DC).
            for (int k = 1; k < FFT_SIZE / 2; k++)
            {
                float re = fftWork[2 * k];
                float im = fftWork[2 * k + 1];
                mag[k] += sqrtf(re * re + im * im);
            }
        }

        float bass = bandAvg(mag, BASS_BIN_LO, BASS_BIN_HI);
        float mid  = bandAvg(mag, MID_BIN_LO,  MID_BIN_HI);
        float treb = bandAvg(mag, TREB_BIN_LO, TREB_BIN_HI);

        // Auto-level: normalise to running average so the visualiser responds
        // equally to quiet and loud sources.
        float level = fmaxf((bass + mid + treb) / 3.0f, 0.01f);
        vol_level   = (vol_level * 29.0f + level) / 30.0f;
        float inv   = 1.5f / vol_level;
        bass *= inv;
        mid  *= inv;
        treb *= inv;

        // Slow-following attenuated values (used by pattern renderer).
        att[0] = (att[0] * 5.0f + bass) / 6.0f;
        att[1] = (att[1] * 5.0f + mid)  / 6.0f;
        att[2] = (att[2] * 5.0f + treb) / 6.0f;

        s.bass     = bass;
        s.mid      = mid;
        s.treb     = treb;
        s.bass_att = att[0];
        s.mid_att  = att[1];
        s.treb_att = att[2];
        s.vol      = bass + mid + treb;

        return true;
    }
};


// ─── Globals ──────────────────────────────────────────────────────────────────

static DaisyHardware         hw;
static DaisySpectrumAnalyzer daisySound;

SpectrumAnalyzer *sound         = &daisySound;
Renderer         *renderPattern = createRenderer();

// Pins confirmed from DaisyDuino Patch/Oled example.
U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI
    oled(U8G2_R0, /*clk*/8, /*data*/10, /*cs*/7, /*dc*/9, /*reset*/30);


// ─── Entry points (called from DaisyMelton.ino) ───────────────────────────────

void setup_daisy()
{
    // TODO: confirm the correct enum for your hardware:
    //   DAISY_PATCH      — original Daisy Patch (most likely)
    //   DAISY_PATCH_SM   — Daisy Patch Submodule
    //   DAISY_PATCH_INIT — Patch.Init()
    // AUDIO_SR_8K matches DAISY_SAMPLE_RATE_HZ=8000 defined in config.h.
    hw = DAISY.init(DAISY_PATCH, AUDIO_SR_8K);

    daisySound.begin();
    DAISY.StartAudio(AudioCallback);

    // Initialise DMX output (RenderMyDMX on Serial1, defined in DaisyMelton.ino).
    output->begin();

    Serial.begin(115200);

    oled.setFont(u8g2_font_6x10_tf);
    oled.setFontMode(1);
    oled.begin();
}


void loop_daisy()
{
    static Spectrum spectrum;
    static float    f32values[IMAGE_SIZE * 3];
    static CRGB     rgbValues[IMAGE_SIZE];

    if (!sound->next(spectrum))
        return;

    renderPattern->renderFrame(millis() / 1000.0f, &spectrum, f32values, IMAGE_SIZE * 3);
    mapToDisplay(f32values, rgbValues, IMAGE_SIZE * 3);
    output->write(rgbValues, IMAGE_SIZE);

    LOG_print("vol: ");  LOG_println(spectrum.vol);

    // VU meter: vol = bass + mid + treb, auto-leveled to ~4.5 average.
    // Map [0, 9] → full bar width so average signal sits at ~50%.
    int bar_w = (int)(fminf(spectrum.vol / 9.0f, 1.0f) * 128.0f);
    oled.clearBuffer();
    oled.drawStr(0, 10, "VU");
    oled.drawFrame(0, 18, 128, 28);
    if (bar_w > 0)
        oled.drawBox(0, 18, bar_w, 28);
    oled.sendBuffer();
}


// Called from DaisyMelton.ino — unused in the Daisy Patch path.
void loop_fft() {}
