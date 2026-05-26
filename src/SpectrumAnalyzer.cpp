/**
 * projectM -- Milkdrop-esque visualisation SDK
 * Copyright (C)2003-2004 projectM Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE.txt' included within this release
 *
 */
/**
 * FFT-based audio analyzer.  Direct descendant of the earlier projectM
 * BeatDetect class — same statistical energy-based approach, rewritten
 * for embedded use and shared between the Daisy firmware and the
 * Linux/macOS simulator.
 *
 * Background reading: Frederic Patin's beat-detection article.
 */

#include "SpectrumAnalyzer.h"
#include "fftsg.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>


// EMA time constants (seconds).  Picked to match the previous
// DaisySpectrumAnalyzer at ~31 fps (vol: 30-frame, att: ~5.5-frame).
// At other frame rates the analyzer scales α to preserve the same
// "follow speed" in wall-clock time.
static constexpr float VOL_TIME_CONSTANT_SEC = 1.0f;
static constexpr float ATT_TIME_CONSTANT_SEC = 0.18f;


static unsigned hzToBin(float hz, unsigned sample_rate, unsigned fft_size)
{
    int b = (int)(hz * (float)fft_size / (float)sample_rate + 0.5f);
    if (b < 1) b = 1;                                       // skip DC
    if ((unsigned)b >= fft_size / 2) b = fft_size / 2 - 1;  // clamp to Nyquist
    return (unsigned)b;
}


SpectrumAnalyzer::SpectrumAnalyzer(unsigned sample_rate, unsigned fft_size, float frame_rate_hz)
  : fft_size_(fft_size),
    vol_level_(20.0f),
    raw_vol_(0.01f)
{
    bass_lo_ = hzToBin(BASS_HZ_LO, sample_rate, fft_size);
    bass_hi_ = hzToBin(BASS_HZ_HI, sample_rate, fft_size);
    mid_lo_  = hzToBin(MID_HZ_LO,  sample_rate, fft_size);
    mid_hi_  = hzToBin(MID_HZ_HI,  sample_rate, fft_size);
    treb_lo_ = hzToBin(TREB_HZ_LO, sample_rate, fft_size);
    treb_hi_ = hzToBin(TREB_HZ_HI, sample_rate, fft_size);

    float frame_interval = 1.0f / frame_rate_hz;
    vol_alpha_ = frame_interval / (VOL_TIME_CONSTANT_SEC + frame_interval);
    att_alpha_ = frame_interval / (ATT_TIME_CONSTANT_SEC + frame_interval);

    hann_     = (float *)malloc(fft_size * sizeof(float));
    fft_work_ = (float *)malloc(fft_size * sizeof(float));
    mag_      = (float *)malloc((fft_size / 2) * sizeof(float));
    fft_w_    = (float *)malloc((fft_size / 2) * sizeof(float));

    for (unsigned i = 0; i < fft_size; i++)
        hann_[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(fft_size - 1)));
    memset(fft_ip_, 0, sizeof(fft_ip_));
    memset(fft_w_,  0, (fft_size / 2) * sizeof(float));

    att_[0] = att_[1] = att_[2] = 20.0f;
}


SpectrumAnalyzer::~SpectrumAnalyzer()
{
    free(hann_);
    free(fft_work_);
    free(mag_);
    free(fft_w_);
}


static float bandAvg(const float *mag, unsigned lo, unsigned hi)
{
    float sum = 0.0f;
    for (unsigned i = lo; i <= hi; i++)
        sum += mag[i];
    return sum / (float)(hi - lo + 1);
}


void SpectrumAnalyzer::analyze(const float *chL, const float *chR, Spectrum &out)
{
    const unsigned half = fft_size_ / 2;
    memset(mag_, 0, half * sizeof(float));

    const float *channels[2] = { chL, chR };
    for (int ch = 0; ch < 2; ch++)
    {
        for (unsigned i = 0; i < fft_size_; i++)
            fft_work_[i] = channels[ch][i] * hann_[i];

        rdft((int)fft_size_, 1, fft_work_, fft_ip_, fft_w_);

        // Bin 0 is DC and bin N/2 is Nyquist (packed into fft_work_[1] by
        // rdft); we skip both since they don't carry musical content.
        for (unsigned k = 1; k < half; k++)
        {
            float re = fft_work_[2 * k];
            float im = fft_work_[2 * k + 1];
            mag_[k] += sqrtf(re * re + im * im);
        }
    }

    float bass_raw = bandAvg(mag_, bass_lo_, bass_hi_);
    float mid_raw  = bandAvg(mag_, mid_lo_,  mid_hi_);
    float treb_raw = bandAvg(mag_, treb_lo_, treb_hi_);

    float bass = bass_raw * BAND_SCALE_BASS;
    float mid  = mid_raw  * BAND_SCALE_MID;
    float treb = treb_raw * BAND_SCALE_TREB;

    // raw_vol_ stays on the un-scaled mean so silence detection and raw
    // VU bars don't need to be re-tuned for the per-band boosts.
    raw_vol_ = fmaxf((bass_raw + mid_raw + treb_raw) / 3.0f, 0.01f);

    float level = fmaxf((bass + mid + treb) / 3.0f, 0.01f);
    vol_level_ = (1.0f - vol_alpha_) * vol_level_ + vol_alpha_ * level;
    float inv  = 1.5f / vol_level_;
    bass *= inv;
    mid  *= inv;
    treb *= inv;

    att_[0] = (1.0f - att_alpha_) * att_[0] + att_alpha_ * bass;
    att_[1] = (1.0f - att_alpha_) * att_[1] + att_alpha_ * mid;
    att_[2] = (1.0f - att_alpha_) * att_[2] + att_alpha_ * treb;

    out.bass     = bass;
    out.mid      = mid;
    out.treb     = treb;
    out.bass_att = att_[0];
    out.mid_att  = att_[1];
    out.treb_att = att_[2];
    out.vol      = bass + mid + treb;
}
