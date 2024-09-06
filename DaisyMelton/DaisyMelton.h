//
// Created by matthew on 3/21/21.
//

#ifndef TEENSYMELTON_TEENSYMELTON_H
#define TEENSYMELTON_TEENSYMELTON_H
#include "config.h"



#define VIBRANCE 0.0f
#define GAMMA 2.6f
#define MAX_BRIGHTNESS 1.0f
struct CRGB;
extern void mapToDisplay(const float ledData[], CRGB rgbData[], const unsigned size);


class OutputLED
{
public:
    virtual void begin() = 0;
    virtual void write(const CRGB data[], unsigned count) = 0;
    virtual void loop()
    { };
};

extern OutputLED *output;
#ifdef OUTPUT_DEBUG
extern OutputLED *outputDebug;
#endif

#endif //TEENSYMELTON_TEENSYMELTON_H
