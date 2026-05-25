/**
 * projectM -- Milkdrop-esque visualisation SDK
 * Copyright (C)2003-2007 projectM Team
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
 * $Id$
 *
 * Beat detection class. Takes decompressed sound buffers and returns
 * various characteristics
 *
 * $Log$
 *
 */

#ifndef _BEAT_DETECT_H
#define _BEAT_DETECT_H

#include "PCM.hpp"
#include <algorithm>
#include <cmath>


// this is the size of the buffer used to determine avg levels of the input audio
// the actual time represented in the history depends on FPS
#define BEAT_HISTORY_LENGTH 80

/**
 * Spectrum analyser: converts raw FFT bins into per-band energy ratios.
 *
 * Each call to detectFromSamples() pulls the latest FFT data from a PCM
 * object, sums energy in three frequency bands (bass / mid / treb), and
 * expresses each band as @c instant/history — the ratio of the current
 * frame's energy to an 80-frame rolling average.  This adaptive
 * normalisation means the output is largely independent of playback volume.
 *
 * Output fields are populated on the class directly (bass, mid, treb, vol
 * and their @c *_att smoothed variants) and should be copied into a Spectrum
 * struct before being passed to the renderer.
 *
 * @note  This class measures energy levels, not rhythmic events.  Beat
 *        timing and tempo estimation are handled by the BeatTracker layer
 *        in Renderer.cpp.
 */
class BeatDetect
{
	public:
        float beatSensitivity;

        float treb;      ///< High-frequency energy ratio this frame
        float mid;       ///< Mid-frequency energy ratio this frame
        float bass;      ///< Low-frequency energy ratio this frame
        float vol_old;   ///< Overall loudness ratio from the previous frame

        float treb_att;  ///< Exponentially smoothed treb (α = 0.4)
        float mid_att;   ///< Exponentially smoothed mid  (α = 0.4)
        float bass_att;  ///< Exponentially smoothed bass (α = 0.4)
        float vol;       ///< Overall loudness ratio this frame
        float vol_att;   ///< Exponentially smoothed vol  (α = 0.4)

		PCM *pcm;

		explicit BeatDetect(PCM *pcm, float sampleRate = 44100.0f);
		~BeatDetect();
		void reset();
        /// Pull the latest FFT data from @c pcm and update all public fields.
		void detectFromSamples();
		void getBeatVals( float samplerate, unsigned fft_length, float *vdataL, float *vdataR );

        // getPCMScale() was added to address https://github.com/projectM-visualizer/projectm/issues/161
        // Returning 1.0 results in using the raw PCM data, which can make the presets look pretty unresponsive
        // if the application volume is low.
		float getPCMScale()
        {
		    return beatSensitivity;
        }

	private:
        float sampleRate;
		int beat_buffer_pos;
        float bass_buffer[BEAT_HISTORY_LENGTH];
		float bass_history;
        float bass_instant;

		float mid_buffer[BEAT_HISTORY_LENGTH];
        float mid_history;
		float mid_instant;

		float treb_buffer[BEAT_HISTORY_LENGTH];
		float treb_history;
		float treb_instant;

        float vol_buffer[BEAT_HISTORY_LENGTH];
        float vol_history;
        float vol_instant;
};

#endif /** !_BEAT_DETECT_H */
