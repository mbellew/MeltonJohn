//
// Created by matthew on 12/13/20.
//

#ifndef MELTONJOHN_CONFIG_H
#define MELTONJOHN_CONFIG_H


#define MELTONJOHN 1
//#define BICYCLEPOLE 1


#define OUTPUT_FASTLED_NEOPIXEL 0
#define OUTPUT_FASTLED_DMX 0
#define OUTPUT_OCTO 0
#define OUTPUT_DEBUG 1
#define OUTPUT_MYDMX 0

#define USE_I2S 0
#define USE_ADC 0



#ifdef MELTONJOHN
    #define IMAGE_SIZE 50

    // OUTPUT LIGHT
//    #undef OUTPUT_FASTLED_DMX
//    #define OUTPUT_FASTLED_DMX 1
    #undef OUTPUT_MYDMX
    #define OUTPUT_MYDMX 1
    #define DMX_TX_PIN 1

    // INPUT SOUND
    #undef USE_I2S
    #define USE_I2S 1
#endif


#ifdef BICYCLEPOLE
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
