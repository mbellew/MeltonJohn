// DaisyMelton — native libDaisy port.
//
// Reads stereo audio from the Daisy Patch's WM8731 codec, runs a small
// FFT to derive bass/mid/treble band energies, hands those to the
// shared Renderer, and shoves the resulting RGB frame out the MIDI
// TRS jack as DMX-512 (via an external MAX3485 driver).  The built-in
// 128×64 OLED shows a three-band VU meter.
//
// Replaces the prior Arduino sketch (DaisyMelton.ino + daisy.cpp).

#include "daisy_patch.h"
#include "per/uart.h"
#include "sys/system.h"

#include "config.h"
#include "Patterns.h"
#include "Renderer.h"
#include "SpectrumAnalyzer.h"
#include "beat_data.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace daisy;


// ─── Globals ──────────────────────────────────────────────────────────────────

static DaisyPatch  patch;
static UartHandler dmx;


// ─── Audio capture ────────────────────────────────────────────────────────────
// AudioCallback (DMA interrupt) writes into a circular buffer; the main loop
// snapshots the most recent FFT_SIZE samples and runs the spectrum analyzer.
// CIRC_LEN must be a power of 2 so wrap-around uses & instead of %.

static constexpr uint32_t CIRC_LEN = FFT_SIZE * 2;
static constexpr uint32_t HOP_SIZE = FFT_SIZE;       // no overlap → ~31 fps @ 8 kHz

static float             circBuf[2][CIRC_LEN];
static volatile uint32_t g_writeIdx = 0;
static float             g_raw_vol  = 0.0f;          // pre-normalization energy for VU

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    uint32_t base = g_writeIdx;
    for (size_t i = 0; i < size; i++)
    {
        uint32_t idx    = (base + i) & (CIRC_LEN - 1);
        circBuf[0][idx] = in[0][i];
        circBuf[1][idx] = in[1][i];
        out[0][i] = out[1][i] = 0.0f;   // silence — visualiser only
    }
    g_writeIdx = base + (uint32_t)size;
}


// ─── Spectrum analyzer ────────────────────────────────────────────────────────
// The actual FFT + per-band aggregation lives in src/SpectrumAnalyzer so the
// desktop simulator can use the same code; here we just snapshot FFT_SIZE
// samples per channel from the circular buffer once per HOP_SIZE and hand
// them off.

static SpectrumAnalyzer *analyzer      = nullptr;
static Renderer         *renderPattern = createRenderer();
static uint32_t          lastFFTIdx    = 0;

static bool nextSpectrum(Spectrum &s)
{
    uint32_t current = g_writeIdx;
    if ((current - lastFFTIdx) < HOP_SIZE)
        return false;
    lastFFTIdx = current;

    static float chL[FFT_SIZE], chR[FFT_SIZE];
    for (uint32_t i = 0; i < FFT_SIZE; i++)
    {
        uint32_t idx = (uint32_t)(current - FFT_SIZE + i) & (CIRC_LEN - 1);
        chL[i] = circBuf[0][idx];
        chR[i] = circBuf[1][idx];
    }

    analyzer->analyze(chL, chR, s);
    g_raw_vol = analyzer->getRawVol();
    return true;
}


// ─── Silence detection / background-music fallback ────────────────────────────
// When the live audio input goes quiet for ~2 s the visualiser would otherwise
// freeze on the last spectrum the analyzer saw.  Instead we feed the renderer
// pre-recorded band magnitudes from beat_data.cpp so the lights keep moving.
// Mirrors the same logic in Daisy/src/main.cpp (the Linux build), adapted to
// threshold against g_raw_vol (the pre-normalization energy) — the Daisy
// analyzer auto-levels spectrum.vol so it cannot be used as a silence signal.

static constexpr float SILENCE_THRESHOLD      = 0.05f;
static constexpr float SILENCE_HYSTERESIS     = 0.1f;
static constexpr int   SILENCE_TRIGGER_FRAMES = 120;   // ~2 s at the ~62 fps HOP rate

// Returns true if we substituted background-music data for `spectrum`.
static bool applyBackgroundIfSilent(Spectrum &spectrum)
{
    static int      silenceFrames = 0;
    static unsigned bgIndex       = 0;
    static float    bgBassAtt     = 1.0f, bgMidAtt = 1.0f, bgTrebAtt = 1.0f;

    bool  inSilence = silenceFrames > SILENCE_TRIGGER_FRAMES;
    float threshold = SILENCE_THRESHOLD * (inSilence ? 1.0f + SILENCE_HYSTERESIS
                                                     : 1.0f - SILENCE_HYSTERESIS);
    if (g_raw_vol < threshold) {
        if (silenceFrames <= SILENCE_TRIGGER_FRAMES)
            silenceFrames++;
    } else {
        silenceFrames = 0;
    }

    if (silenceFrames <= SILENCE_TRIGGER_FRAMES || backgroundMusicSize == 0)
        return false;

    const float *bg = backgroundMusic[bgIndex % backgroundMusicSize];
    bgIndex++;
    bgBassAtt = 0.6f * bgBassAtt + 0.4f * bg[0];
    bgMidAtt  = 0.6f * bgMidAtt  + 0.4f * bg[1];
    bgTrebAtt = 0.6f * bgTrebAtt + 0.4f * bg[2];
    spectrum.bass     = bg[0];
    spectrum.mid      = bg[1];
    spectrum.treb     = bg[2];
    spectrum.vol      = bg[3];
    spectrum.bass_att = bgBassAtt;
    spectrum.mid_att  = bgMidAtt;
    spectrum.treb_att = bgTrebAtt;
    return true;
}


// ─── DMX output ───────────────────────────────────────────────────────────────
// DMX-512 frame = BREAK (>88 µs low) + MAB (>8 µs high) + start code (0x00) +
// 1..512 channel bytes at 250 kbaud 8N2.  We reproduce the BREAK using the
// Arduino-port trick: re-init the UART at 100 kbaud 8E1 and transmit a zero,
// which holds the line low for ~100 µs, then re-init at 250 kbaud 8N2 for the
// real frame.

static UartHandler::Config makeDmxBreakConfig()
{
    UartHandler::Config c;
    c.periph        = UartHandler::Config::Peripheral::USART_1;
    c.mode          = UartHandler::Config::Mode::TX;
    c.pin_config.tx = seed::D13;          // MIDI TRS TX on the Patch
    c.pin_config.rx = seed::D14;
    c.baudrate      = 100000;
    c.stopbits      = UartHandler::Config::StopBits::BITS_1;
    c.parity        = UartHandler::Config::Parity::EVEN;
    c.wordlength    = UartHandler::Config::WordLength::BITS_8;
    return c;
}

static UartHandler::Config makeDmxFrameConfig()
{
    UartHandler::Config c = makeDmxBreakConfig();
    c.baudrate = 250000;
    c.stopbits = UartHandler::Config::StopBits::BITS_2;
    c.parity   = UartHandler::Config::Parity::NONE;
    return c;
}

static void dmxWrite(const CRGB *data, size_t count)
{
    uint8_t zero = 0;

    // BREAK + MAB: a zero byte at 100 kHz 8E1 ≈ 100 µs low + 10 µs high.
    dmx.Init(makeDmxBreakConfig());
    dmx.BlockingTransmit(&zero, 1);

    // Frame: start code + channel data, padded to IMAGE_SIZE*3 if short.
    dmx.Init(makeDmxFrameConfig());
    dmx.BlockingTransmit(&zero, 1);

    uint8_t buf[IMAGE_SIZE * 3];
    size_t  bytes = count * 3;
    if (bytes > sizeof(buf))
        bytes = sizeof(buf);
    memcpy(buf, data, bytes);
    if (bytes < sizeof(buf))
        memset(buf + bytes, 0, sizeof(buf) - bytes);
    dmx.BlockingTransmit(buf, sizeof(buf));
}


// ─── Float → 8-bit RGB conversion (formerly mapToDisplay in DaisyMelton.ino) ──

#define VIBRANCE       0.0f
#define GAMMA          2.6f
#define MAX_BRIGHTNESS 1.0f

static inline uint8_t clampByte(float v)
{
    int i = (int)roundf(v);
    if (i < 0)   return 0;
    if (i > 255) return 255;
    return (uint8_t)i;
}

static void mapToDisplay(const float ledData[], CRGB rgbData[], size_t size)
{
    size_t count = size / 3;
    for (size_t i = 0; i < count; i++)
    {
        float r = ledData[i * 3 + 0];
        float g = ledData[i * 3 + 1];
        float b = ledData[i * 3 + 2];

        if (VIBRANCE != 0.0f)
        {
            float mx     = fmaxf(fmaxf(r, g), b);
            float avg    = (r + g + b) / 3.0f;
            float adjust = (1.0f - (mx - avg)) * 2.0f * -1.0f * VIBRANCE;
            r = r * (1.0f - adjust) + adjust * mx;
            g = g * (1.0f - adjust) + adjust * mx;
            b = b * (1.0f - adjust) + adjust * mx;
        }

        const float scale = MAX_BRIGHTNESS * 255.0f;
        if (GAMMA != 1.0f)
        {
            rgbData[i].r = clampByte(powf(r, GAMMA) * scale);
            rgbData[i].g = clampByte(powf(g, GAMMA) * scale);
            rgbData[i].b = clampByte(powf(b, GAMMA) * scale);
        }
        else
        {
            rgbData[i].r = clampByte(r * scale);
            rgbData[i].g = clampByte(g * scale);
            rgbData[i].b = clampByte(b * scale);
        }
    }
}


// ─── Non-blocking USB-CDC logger ──────────────────────────────────────────────
// The libDaisy Logger::PrintLine switches to a *blocking* TransmitSync once
// the host has connected, so the main loop wedges as soon as the host
// disconnects (e.g. you close arduino-cli monitor and the OLED freezes).
// We bypass that by writing straight to UsbHandle::TransmitInternal, which
// returns an error if the CDC TX endpoint is busy and we just drop the
// message — fire and forget.

static void usbLogf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    buf[n++] = '\r';
    buf[n++] = '\n';
    (void)patch.seed.usb_handle.TransmitInternal((uint8_t *)buf, (size_t)n);
}


// ─── Hooks expected by shared code ────────────────────────────────────────────
// config.h forward-declares these for DEBUG_LOG paths.

#if DEBUG_LOG
void _print(const char *s)   { usbLogf("%s", s); }
void _println(const char *s) { usbLogf("%s", s); }
void _print(float f)         { usbLogf("%f", (double)f); }
void _println(float f)       { usbLogf("%f", (double)f); }
#endif


// ─── Entry point ──────────────────────────────────────────────────────────────

static void updatePatchDisplay(const Spectrum &spectrum,
                               bool             isInternal,
                               const char      *patternName);

int main(void)
{
    patch.Init();
    patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_8KHZ);

    // Effective frame rate is one analyze() call per HOP_SIZE new audio
    // samples — passed into the analyzer so its EMA time-constants are
    // converted to the right per-frame alphas.
    float fps = (float)DAISY_SAMPLE_RATE_HZ / (float)HOP_SIZE;
    analyzer  = new SpectrumAnalyzer(DAISY_SAMPLE_RATE_HZ, FFT_SIZE, fps);
    patch.StartAudio(AudioCallback);

    // USB CDC logger — appears as a serial device when the Daisy is plugged in.
    patch.seed.StartLog(false);

    // DMX UART is (re-)configured on every frame in dmxWrite(); we just need an
    // initial Init() so the GPIO alternate-function gets wired up.
    dmx.Init(makeDmxFrameConfig());

    Spectrum spectrum;
    float    f32values[IMAGE_SIZE * 3];
    CRGB     rgbValues[IMAGE_SIZE];

    // Per-second timing telemetry (logged via USB CDC).  Tracks avg & peak
    // microseconds spent in (a) audio analysis + pattern render + DMX TX,
    // and (b) OLED draw + SPI update.
    uint32_t audio_us_acc  = 0, audio_us_peak  = 0;
    uint32_t disp_us_acc   = 0, disp_us_peak   = 0;
    uint32_t frame_count   = 0;
    uint32_t last_log_ms   = System::GetNow();

    while (true)
    {
        if (!nextSpectrum(spectrum))
            continue;

        // ── (a) audio / pattern / DMX block ──────────────────────────────────
        uint32_t t0 = System::GetUs();

        bool isInternal = applyBackgroundIfSilent(spectrum);
        static bool wasInternal = false;
        if (isInternal != wasInternal) {
            usbLogf("source=%s", isInternal ? "internal" : "external");
            wasInternal = isInternal;
        }

        float t = (float)System::GetNow() / 1000.0f;
        renderPattern->renderFrame(t, &spectrum, f32values, IMAGE_SIZE * 3);
        mapToDisplay(f32values, rgbValues, IMAGE_SIZE * 3);
        dmxWrite(rgbValues, IMAGE_SIZE);

        uint32_t t1 = System::GetUs();

        // ── (b) display block ────────────────────────────────────────────────
        updatePatchDisplay(spectrum, isInternal, renderPattern->getPatternName());

        uint32_t t2 = System::GetUs();

        // ── timing telemetry ─────────────────────────────────────────────────
        uint32_t audio_us = t1 - t0;
        uint32_t disp_us  = t2 - t1;
        audio_us_acc += audio_us;
        disp_us_acc  += disp_us;
        if (audio_us > audio_us_peak) audio_us_peak = audio_us;
        if (disp_us  > disp_us_peak)  disp_us_peak  = disp_us;
        frame_count++;

        uint32_t now = System::GetNow();
        if (false && now - last_log_ms >= 1000 && frame_count > 0)
        {
            usbLogf(
                "fps=%lu  audio %luus avg / %luus peak   disp %luus avg / %luus peak",
                (unsigned long)frame_count,
                (unsigned long)(audio_us_acc / frame_count),
                (unsigned long)audio_us_peak,
                (unsigned long)(disp_us_acc / frame_count),
                (unsigned long)disp_us_peak);

            audio_us_acc = audio_us_peak = 0;
            disp_us_acc  = disp_us_peak  = 0;
            frame_count  = 0;
            last_log_ms  = now;
        }
    }
}


// ─── OLED VU meter ────────────────────────────────────────────────────────────
// Layout (128 × 64, all coords inclusive):
//   y=0..7   header: "*" above the vertical bar when on the internal/silence
//            feed, then the current pattern name above the horizontal bars
//   y=9..63  vertical raw-input-level bar (6 px wide) on the left
//   y=9..63  three horizontal per-band bars (bass / mid / treb) on the right
// vu_smooth / vu_peak are file-static so the fast-attack / slow-release
// envelope persists across frames without living in main()'s locals.

static void updatePatchDisplay(const Spectrum &spectrum,
                               bool             isInternal,
                               const char      *patternName)
{
    static float vu_smooth = 0.0f;
    static float vu_peak   = 0.01f;

    // Fast attack, slow release, normalised to a peak that decays over ~10 s
    // so the user sees relative input level.
    vu_smooth = (g_raw_vol > vu_smooth)
                    ? g_raw_vol * 0.4f  + vu_smooth * 0.6f
                    : g_raw_vol * 0.02f + vu_smooth * 0.98f;
    vu_peak   = fmaxf(vu_peak * 0.999f, vu_smooth);

    // Geometry (after the 9 px header strip — vertical bar is 55 px tall).
    constexpr int meter_top    = 9;
    constexpr int meter_bottom = 63;
    constexpr int meter_h      = meter_bottom - meter_top + 1;   // 55
    int vu_h = (int)(vu_smooth / vu_peak * (float)meter_h);

    const char *labels[3] = { "B", "M", "H" };
    float       vals[3]   = { spectrum.bass, spectrum.mid, spectrum.treb };

    patch.display.Fill(false);

    // Header row: silence-source indicator above the vertical bar, pattern
    // name above the horizontal bars.  Font_6x8 is 8 px tall.
    patch.display.SetCursor(0, 0);
    patch.display.WriteString(isInternal ? "*" : " ", Font_6x8, true);
    patch.display.SetCursor(7, 0);
    patch.display.WriteString(patternName ? patternName : "", Font_6x8, true);

    // Vertical raw-level bar (6 px wide, fills from bottom).
    patch.display.DrawRect(0, meter_top, 5, meter_bottom, true, false);
    if (vu_h > 0)
        patch.display.DrawRect(0, meter_bottom - vu_h + 1, 5, meter_bottom, true, true);
    // Target marker at ~75 % of the bar height.
    int marker_y = meter_bottom - (meter_h * 3 / 4);
    patch.display.DrawLine(0, marker_y, 5, marker_y, true);

    // Horizontal band bars to the right of the vertical bar.  Three 16 px
    // bars with 2 px gaps fits in the 55 px window (16 + 2 + 16 + 2 + 16 = 52).
    for (int i = 0; i < 3; i++)
    {
        int y     = meter_top + i * 18;
        int bar_w = (int)(fminf(vals[i] / 4.0f, 1.0f) * 110.0f);
        patch.display.SetCursor(7, y + 4);
        patch.display.WriteString(labels[i], Font_6x8, true);
        if (bar_w > 0)
            patch.display.DrawRect(13, y, 13 + bar_w - 1, y + 15, true, true);
    }

    patch.display.Update();
}
