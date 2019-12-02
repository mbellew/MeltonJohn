#include "MidiMix.h"

#define IMAGE_SIZE 20
#define IMAGE_SCALE ((double)IMAGE_SIZE)
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



extern void renderFrame(float time, const Spectrum *spectrum, float buffer[], size_t size);
extern void renderFrame(float time, const Spectrum *spectrum, MidiMix *board, float buffer[], size_t size);

extern void rgb2hsl(const double *rgb, double *hsl);
extern void hsl2rgb(const double *hsl, double *rgb);
