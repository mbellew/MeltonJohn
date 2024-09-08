//
// Created by matthew on 12/13/20.
//

#ifndef MELTONJOHN_CONFIG_H
#define MELTONJOHN_CONFIG_H

// APPLICATION -- pick one
#define MELTONJOHN 1
//#define BICYCLEPOLE 1
//#define LIGHTBUCKET 1

// PLATFORM -- pick one
#ifdef STM32_CORE_VERSION
#define PLATFORM_DAISY 1
#else
  #ifdef ESP32
  #define PLATFORM_M5STACK 1
  #else
  #define PLATFORM_TEENSY 1
  #endif
#endif

#define OUTPUT_DEBUG 0
#define OUTPUT_ADAFRUIT_NEOPIXELS 0
#define OUTPUT_FASTLED_NEOPIXEL 0
#define OUTPUT_FASTLED_DMX 0
#define OUTPUT_MYDMX 0


#ifdef PLATFORM_M5STACK
    #define NEOPIXEL_PIN G26
    #define OUTPUT_M5LCD 0
    #define OUTPUT_SPECTRUM 1
    #define OUTPUT_NEOPIXELBUS 0
//    #define FASTLED_RMT_MAX_CHANNELS 1
//    #define FASTLED_RMT_BUILTIN_DRIVER 1
#else
    #define DMX_TX_PIN 1
    #define NEOPIXEL_PIN 2
    #define OUTPUT_OCTO 0
    #define OUTPUT_TEENSYDMX 0
    // pick audi source
    // -- audio board
    #define USE_I2S 0
    // -- ADC microphone
    #define USE_ADC 0
    #define ADC_MIC_PIN A5
    #define ADC_MIC_VCC_PIN 23
    #define ADC_MIC_GND_PIN 22
#endif


#ifdef MELTONJOHN

    #define IMAGE_SIZE 20

#ifdef TEENSY_PLATFORM
    /** MeltonJohn Teensy4 with an MSGEQ7 spectrum analyzer sub-board **/
    // INPUT SOUND
    //#undef USE_I2S
    //#define USE_I2S 1
    #define USE_MSGEQ7 1
    #undef OUTPUT_TEENSYDMX
    #define OUTPUT_TEENSYDMX 1
#else
    /* MeltonJohn Daisy board with built-in audio handler **/
    #undef OUTPUT_MYDMX
    #define OUTPUT_MYDMX 1
#endif

#endif


/** BicyclePole uses M5StickC ESP32-PICO Mini **/
#ifdef BICYCLEPOLE
    #define IMAGE_SIZE 50

    // OUTPUT LIGHT
    //#define OUTPUT_ADAFRUIT_NEOPIXELS 1
    //#define OUTPUT_FASTLED_NEOPIXEL 1
    #undef OUTPUT_NEOPIXELBUS
    #define OUTPUT_NEOPIXELBUS 1

    // INPUT SOUND
    #undef USE_ADC
    #define USE_ADC 1
#endif


/** LightBucket uses Teensy 3.2 with an analog microphone **/
#ifdef LIGHTBUCKET
    #define IMAGE_SIZE 50

    // OUTPUT LIGHT
    #undef OUTPUT_FASTLED_NEOPIXEL
    #define OUTPUT_FASTLED_NEOPIXEL 1

    // INPUT SOUND
    #undef USE_ADC
    #define USE_ADC 1
#endif


#define DESKTOP 0
#define MIDI_MIXER 0
#define DEBUG_LOG 1



#if DEBUG_LOG

extern void _print(const char *s);
extern void _println(const char *s);
extern void _print(float f);
extern void _println(float f);

#else

#define _print(x)
#define _println(x)

#endif // !DEBUG_LOG

//extern int random(void);



// fastled is not supported so fixup
#include "pixeltypes.h"




#endif //MELTONJOHN_CONFIG_H
