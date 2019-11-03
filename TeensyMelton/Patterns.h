//#include <OctoWS2811.h>

#define IMAGE_SIZE 20

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

void renderFrame(float time, Spectrum *spectrum, uint8_t buffer[], size_t size);
