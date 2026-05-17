#include "config.h"

#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>
#if MIDI_MIXER 
#include "MidiMix.h"
#endif

#define IMAGE_SCALE ((float)IMAGE_SIZE)
#define OB_LEFT 0
#define OB_RIGHT (IMAGE_SIZE-1)
#define IB_LEFT 1
#define IB_RIGHT (IMAGE_SIZE-2)
#define LEFTMOST_PIXEL 0
#define RIGHTMOST_PIXEL (IMAGE_SIZE-1)
#define RED_CHANNEL 0
#define BLUE_CHANNEL 1
#define GREEN_CHANNEL 2


/**
 * Audio spectrum snapshot delivered to Renderer::renderFrame() each frame.
 *
 * All values are dimensionless ratios: instant band energy divided by a
 * rolling average over the last ~80 frames (~2.7 s at 30 fps).  A value of
 * 1.0 means current energy equals the recent average; 2.0 means twice the
 * average.  Values are capped at 100 but rarely exceed 3–5 in practice.
 *
 * The @c *_att ("attenuated") variants are exponentially smoothed (α = 0.4)
 * versions of the raw values, useful as slow-moving envelope followers.
 */
struct Spectrum
{
    float bass;      ///< Low-frequency energy ratio  (roughly 0–500 Hz)
    float mid;       ///< Mid-frequency energy ratio  (roughly 500–4000 Hz)
    float treb;      ///< High-frequency energy ratio (roughly 4000 Hz–Nyquist)
    float bass_att;  ///< Smoothed bass
    float mid_att;   ///< Smoothed mid
    float treb_att;  ///< Smoothed treb
    float vol;       ///< Overall loudness ratio (bin-count-normalized mean of the three bands)
};


/**
 * Abstract interface for the LED pattern renderer.
 *
 * Each call to renderFrame() consumes one Spectrum snapshot and writes
 * normalized RGB floats [0, 1] into @p buffer (3 × IMAGE_SIZE floats,
 * interleaved R G B per pixel).  The implementation handles pattern
 * selection, beat-driven transitions, and per-frame animation.
 */
struct Renderer
{
    /// Render one frame into @p buffer (3 × IMAGE_SIZE floats, RGB interleaved).
    virtual void renderFrame(float time, const Spectrum *spectrum, float buffer[], size_t size) = 0;
    /// Name of the currently active pattern, or "" before the first frame.
    virtual const char* getPatternName() const = 0;
};

extern Renderer *createRenderer();

#endif
