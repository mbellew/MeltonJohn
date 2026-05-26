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

#ifndef SPECTRUM_ANALYZER_H
#define SPECTRUM_ANALYZER_H

#include <stddef.h>
#include "Renderer.h"   // Spectrum

/**
 * Shared FFT-based audio analyzer used by the Daisy firmware and the
 * Linux/macOS desktop simulator.  Replaces the older `BeatDetect` class
 * for FFT platforms; the Teensy build does not use this class (it reads
 * pre-computed band magnitudes from an MSGEQ7 chip).
 *
 * The analyzer is configured per-platform with the audio sample rate and
 * FFT size; the band boundaries are expressed in Hz (not bin indices),
 * and the EMA smoothing constants are expressed in seconds, so the same
 * code produces equivalent behavior at e.g. 8 kHz / FFT 256 / 31 fps and
 * 44.1 kHz / FFT 1024 / 30 fps.
 *
 * Pipeline (per analyze() call):
 *   1. Hann-window each channel.
 *   2. Real DFT (Ooura `rdft`) on each channel.
 *   3. Per-bin magnitudes summed across the two channels (phase-safe).
 *   4. Per-band average magnitude (band Hz boundaries resolved to bins at
 *      construction time).
 *   5. Per-band fixed gain compensation (BAND_SCALE_*).
 *   6. Global auto-leveler: divide all bands by a running average of
 *      (bass + mid + treb) / 3 so the visualiser responds equally to
 *      quiet and loud sources.
 *   7. Per-band slow envelope follower for the `*_att` outputs.
 */
class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer(unsigned sample_rate, unsigned fft_size, float frame_rate_hz);
    ~SpectrumAnalyzer();

    // Consume one frame of stereo audio (fft_size samples per channel)
    // and write the band-energy spectrum into `out`.
    void analyze(const float *chL, const float *chR, Spectrum &out);

    // Pre-scale, pre-normalization mean band magnitude.  Useful for raw
    // input-level VU bars and silence detection — its scale is not
    // affected by the per-band boosts or the auto-leveler.
    float getRawVol() const { return raw_vol_; }

    // Band boundaries (Hz).  Same across platforms; bin indices are
    // derived at construction from the sample rate and FFT size.
    static constexpr float BASS_HZ_LO = 30.0f;
    static constexpr float BASS_HZ_HI = 250.0f;
    static constexpr float MID_HZ_LO  = 280.0f;
    static constexpr float MID_HZ_HI  = 660.0f;
    static constexpr float TREB_HZ_LO = 690.0f;
    static constexpr float TREB_HZ_HI = 2500.0f;

    // Per-band gain compensation applied after per-bin averaging.  Each
    // band averages over a different number of FFT bins (bass spans fewer
    // bins than treble), and typical music has a ~3 dB/octave spectral
    // rolloff, so the per-bin average for treble is intrinsically much
    // smaller.  These factors boost mid/treb so a "typical" mix reads
    // balanced; a bass-heavy or treble-heavy track still looks different.
    static constexpr float BAND_SCALE_BASS = 1.0f;
    static constexpr float BAND_SCALE_MID  = 2.5f;
    static constexpr float BAND_SCALE_TREB = 6.0f;

private:
    unsigned fft_size_;

    unsigned bass_lo_, bass_hi_;
    unsigned mid_lo_,  mid_hi_;
    unsigned treb_lo_, treb_hi_;

    // EMA coefficients derived from time constants and the supplied
    // frame_rate_hz, so behavior is platform-independent.
    float vol_alpha_;   // applied to the global auto-leveler
    float att_alpha_;   // applied to the per-band envelope follower

    // FFT scratch (heap-allocated so fft_size_ can vary across platforms).
    float *hann_;
    float *fft_work_;
    float *mag_;
    int    fft_ip_[20];
    float *fft_w_;

    // State
    float vol_level_;
    float att_[3];
    float raw_vol_;
};

#endif // SPECTRUM_ANALYZER_H
