#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>
#if MIDI_MIXER 
#include "MidiMix.h"
#endif

#define IMAGE_SIZE 20
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


struct Spectrum
{
    float bass;
    float mid;
    float treb;
    float bass_att;
    float mid_att;
    float treb_att;
    float vol;
};


struct Renderer
{
    virtual void renderFrame(float time, const Spectrum *spectrum, float buffer[], size_t size) = 0;
};

extern Renderer *createRenderer();

#endif
