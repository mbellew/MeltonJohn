#include "BeatDetect.hpp"

#define IMAGE_SIZE 20
#define IMAGE_SCALE ((double)IMAGE_SIZE)
#define OB_LEFT 0
#define OB_RIGHT (IMAGE_SIZE-1)
#define IB_LEFT 1
#define IB_RIGHT (IMAGE_SIZE-2)
#define LEFTMOST_M_PIXEL 0
#define RIGHTMOST_M_PIXEL (IMAGE_SIZE-1)
#define RED_CHANNEL 0
#define BLUE_CHANNEL 1
#define GREEN_CHANNEL 2

extern void renderFrame(BeatDetect *beatDetect, double current_time, unsigned char ledData[]);
