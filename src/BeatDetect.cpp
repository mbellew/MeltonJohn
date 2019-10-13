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
 * Takes sound data from wherever and returns beat detection values
 * Uses statistical Energy-Based methods. Very simple
 *
 * Some stuff was taken from Frederic Patin's beat-detection article,
 * you'll find it online
 */

#include <stdlib.h>
#include <stdio.h>
#include <cmath>
#include "BeatDetect.hpp"


#include "beats.data"


BeatDetect::BeatDetect(PCM *pcm)
{
    int x, y;

    this->pcm = pcm;

    this->vol_instant = 0;
    this->vol_history = 0;

    for (y = 0; y < 80; y++)
    {
        this->vol_buffer[y] = 0;
    }

    this->beat_buffer_pos = 0;

    for (x = 0; x < 32; x++)
    {
        this->beat_instant[x] = 0;
        this->beat_history[x] = 0;
        this->beat_val[x] = 1.0;
        this->beat_att[x] = 1.0;
        this->beat_variance[x] = 0;
        for (y = 0; y < 80; y++)
        {
            this->beat_buffer[x][y] = 0;
        }
    }
    this->bg_fadein = 0.0;
    this->bg_pos = 0;
    this->bg_history = 0.1;

    // output variables
    this->treb = 0;
    this->mid = 0;
    this->bass = 0;
    this->vol_old = 0;
    this->beat_sensitivity = 10.00;
    this->treb_att = 0;
    this->mid_att = 0;
    this->bass_att = 0;
    this->vol = 0;
}


BeatDetect::~BeatDetect() = default;


void BeatDetect::detectFromSamples()
{
    float vol_prev = vol;

    // these strictly output variables make sure we aren't using them to compute new values
    this->vol_old = 0;
    this->vol = 0;
    this->treb = 0;
    this->bass = 0;
    this->mid = 0;
    this->treb_att = 0;
    this->mid_att = 0;
    this->bass_att = 0;

    getBeatVals(pcm->pcmdataL, pcm->pcmdataR);
    this->vol_old = vol_prev;
}


void BeatDetect::getBeatVals(float *vdataL, float *vdataR)
{
    int linear = 0;
    int x, y;
    float temp2 = 0;

    vol_instant = 0;
    for (x = 0; x < 16; x++)
    {
        beat_instant[x] = 0;
        for (y = linear * 2; y < (linear + 8 + x) * 2; y++)
        {
            beat_instant[x] += ((vdataL[y] * vdataL[y]) + (vdataR[y] * vdataR[y])) * (1.0 / (8 + x));
            vol_instant += ((vdataL[y] * vdataL[y]) + (vdataR[y] * vdataR[y])) * (1.0 / 512.0);
        }
        linear = y / 2;
        beat_history[x] -= (beat_buffer[x][beat_buffer_pos]) * .0125;
        beat_buffer[x][beat_buffer_pos] = beat_instant[x];
        beat_history[x] += (beat_instant[x]) * .0125;

        beat_val[x] = (beat_instant[x]) / (beat_history[x]);

        beat_att[x] += (beat_instant[x]) / (beat_history[x]);
    }

    vol_history -= (vol_buffer[beat_buffer_pos]) * .0125;
    vol_buffer[beat_buffer_pos] = vol_instant;
    vol_history += (vol_instant) * .0125;

    bass = (beat_instant[0]) / (1.5f * beat_history[0]);

    temp2 = 0;
    mid = 0;
    for (x = 1; x < 10; x++)
    {
        mid += (beat_instant[x]);
        temp2 += (beat_history[x]);
    }
    mid = mid / (1.5f * temp2);

    temp2 = 0;
    treb = 0;
    for (x = 10; x < 16; x++)
    {
        treb += (beat_instant[x]);
        temp2 += (beat_history[x]);
    }
    treb = treb / (1.5f * temp2);

    vol = vol_instant / (1.5f * fmax(0.0001,vol_history));

    if (std::isnan(treb))
    {
        treb = 0.0;
    }
    if (std::isnan(mid))
    {
        mid = 0.0;
    }
    if (std::isnan(bass))
    {
        bass = 0.0;
    }
    if (std::isnan(vol))
    {
        vol = 0.0;
    }

    // if volume is very low, the add in background
    if (vol_history < 0.00001)
        bg_fadein = fmin(1,bg_fadein + 0.01);
    else if (vol_history > 0.00002)
        bg_fadein = fmax(0.0,bg_fadein - 0.05);
    bg_pos = (bg_pos+1) % backgroundMusicSize;
    float bg_bass = backgroundMusic[bg_pos][0];
    float bg_mid  = backgroundMusic[bg_pos][1];
    float bg_treb = backgroundMusic[bg_pos][2];
    float bg_vol  = bg_bass + bg_mid + bg_treb;
    float bg_history = (bg_history * 0.99) + 0.1;

    vol  += bg_vol / (1.5 * bg_history);

    bass += bg_fadein * bg_bass;
    mid  += bg_fadein * bg_mid;
    treb += bg_fadein * bg_treb;

    treb_att = .6f * treb_att + .4f * treb;
    mid_att  = .6f * mid_att + .4f * mid;
    bass_att = .6f * bass_att + .4f * bass;

    if (bass_att > 100)
        bass_att = 100;
    if (bass > 100)
        bass = 100;
    if (mid_att > 100)
        mid_att = 100;
    if (mid > 100)
        mid = 100;
    if (treb_att > 100)
        treb_att = 100;
    if (treb > 100)
        treb = 100;
    if (vol > 100)
        vol = 100;

    beat_buffer_pos++;
    if (beat_buffer_pos > 79)
        beat_buffer_pos = 0;
}
