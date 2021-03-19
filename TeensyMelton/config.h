//
// Created by matthew on 12/13/20.
//

#ifndef MELTONJOHN_CONFIG_H
#define MELTONJOHN_CONFIG_H

// APPLICATION -- pick one
//#define MELTONJOHN 1
#define BICYCLEPOLE 1
//#define LIGHTBUCKET 1

// PLATFORM -- pick one
//#define PLATFORM_TEENSY 1
#define PLATFORM_M5STACK 1




#define OUTPUT_DEBUG 0
#define OUTPUT_FASTLED_NEOPIXEL 0
#define OUTPUT_FASTLED_DMX 0
#define OUTPUT_OCTO 0
#define OUTPUT_MYDMX 0
#define OUTPUT_TEENSYDMX 0
#define OUTPUT_M5LCD 0

#define USE_I2S 0
#define USE_ADC 0


/** MeltonJohn uses a Teensy4 with an AudioBoard **/
#ifdef MELTONJOHN

    #define IMAGE_SIZE 50

    #undef PLATFORM_M5STACK
    #undef PLATFORM_TEENSY
    #define PLATFORM_TEENSY 1

    // OUTPUT LIGHT
    #undef OUTPUT_TEENSYDMX
    #define OUTPUT_TEENSYDMX 1
    #define DMX_TX_PIN 1

    // INPUT SOUND
    #undef USE_I2S
    #define USE_I2S 1
#endif


/** BicyclePole uses M5StickC ESP32-PICO Mini **/
#ifdef BICYCLEPOLE
    #define IMAGE_SIZE 50

    // OUTPUT LIGHT
    #undef OUTPUT_FASTLED_NEOPIXEL
    #define OUTPUT_FASTLED_NEOPIXEL 1

    #undef OUTPUT_M5LCD
    #define OUTPUT_M5LCD 1

#ifdef PLATFORM_M5STACK
    #define NEOPIXEL_PIN G0
#else
    #define NEOPIXEL_PIN 2
#endif

    // INPUT SOUND
    #undef USE_ADC
    #define USE_ADC 1
    #define ADC_MIC_PIN A5
    #define ADC_MIC_VCC_PIN 23
    #define ADC_MIC_GND_PIN 22
#endif


/** LightBucket uses Teensy 3.2 with an analog microphone **/
#ifdef LIGHTBUCKET
    #define IMAGE_SIZE 50

    // OUTPUT LIGHT
    #undef OUTPUT_FASTLED_NEOPIXEL
    #define OUTPUT_FASTLED_NEOPIXEL 1

    #define NEOPIXEL_PIN 2

    // INPUT SOUND
    #undef USE_ADC
    #define USE_ADC 1
    #define ADC_MIC_PIN A5
    #define ADC_MIC_VCC_PIN 23
    #define ADC_MIC_GND_PIN 22
#endif


#define DESKTOP 0
#define MIDI_MIXER 0
#define DEBUG_LOG 1



#endif //MELTONJOHN_CONFIG_H
