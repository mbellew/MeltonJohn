#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "Patterns.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
//#include <SerialFlash.h>
#include <OctoWS2811.h>
#include <DmxSimple.h>
//#include <DMXSerial.h>
#include <FastLED.h>

#define DebugSerial Serial



#define DEBUG_LOG 0

// IMAGE_SIZE is in config.h

// OUTPUT LIGHT
#define OUTPUT_FASTLED_NEOPIXEL 1
#define OUTPUT_FASTLED_DMX 0
#define OUTPUT_OCTO 0
#define OUTPUT_DEBUG 0
#define OUTPUT_MYDMX 0

#define NEOPIXEL_PIN 2
#define DMX_TX_PIN 1

// INPUT SOUND
#define USE_I2S 0
#define USE_ADC 1
#define ADC_MIC_PIN A5
#define ADC_MIC_VCC_PIN 23
#define ADC_MIC_GND_PIN 22




#ifdef SERIAL_PORT_HARDWARE_OPEN6
  #define TEENSY40
  HardwareSerial &SERIAL_PORT_DMX  = Serial1;
#else
  HardwareSerial &SERIAL_PORT_DMX  = Serial1;
#endif




#if DEBUG_LOG
#define LOG_print(x) DebugSerial.print(x)
//#define LOG_println() SerialDebugSerial.println()
#define LOG_println(x) DebugSerial.println(x)
#else
#define LOG_print(x)
#define LOG_println(x)
#endif



typedef uint32_t color_t;
typedef struct realcolor
{
    float r, g, b;
} RealColor;

// 4 bit colors
#define v(i)((uint8_t)(i==0?0:i*16-1))

// 8 bit colors
#define color(_r, _g, _b) (((color_t)(_r))<<16 | ((color_t)(_g)) << 8 | (color_t)(_b))

#define max(a, b) ((a)>(b)?(a):(b))
#define min(a, b) ((a)<(b)?(a):(b)) 


void LOG_format(int n)
{
    if (n < 10)
        LOG_print("    ");
    else if (n < 100)
        LOG_print("   ");
    else if (n < 1000)
        LOG_print("  ");
    else
        LOG_print(" ");
    LOG_print(n);
}


#if USE_I2S
AudioInputI2S adc;                        // AudioBoard
AudioControlSGTL5000 audioShield;
#endif
#if USE_ADC
AudioInputAnalog adc(ADC_MIC_PIN);       // built-in ADC
#endif
AudioAnalyzeFFT1024 fft;
AudioConnection patchCord(adc, fft);

struct SoundFFT
{
    
    float volume_level = 20;
    float att[3] = {20, 20, 20};

    SoundFFT()
    {
    }

    void begin()
    {
        AudioMemory(12);
#if USE_I2S
        audioShield.enable();
        audioShield.inputSelect(AUDIO_INPUT_LINEIN);
#endif
        fft.windowFunction(AudioWindowHanning1024);
    }

    /** return three numbers between 0.0 and 1.0 **/
    boolean next(Spectrum &ret)
    {
        if (!fft.available())
          return false;
          
        float sums[] = {0, 0, 0};
        sums[0] = fft.read(0, 1);
        sums[1] = fft.read(2, 8);
        sums[2] = fft.read(9, 16);
       
        //LOG_print("count "); LOG_format(count); LOG_println();
        //LOG_print("raw   "); LOG_format((int)(sums[0]*1000.0)); LOG_format((int)(sums[1]*1000.0)); LOG_format((int)(sums[2]*1000.0)); LOG_println();

        float level = max(0.01, (sums[0] + sums[1] + sums[2]) / 3.0);
        volume_level = (volume_level * 29.0 + level) / 30.0;
        //LOG_print("vol   "); LOG_format(volume_level*1000.0); LOG_println();

        // auto-level
        const float SCALE = 1.5;
        sums[0] *= SCALE / volume_level;
        sums[1] *= SCALE / volume_level;
        sums[2] *= SCALE / volume_level;

        // LOG_print("level "); LOG_format((int)(sums[0]*1000.0)); LOG_format((int)(sums[1]*1000.0)); LOG_format((int)(sums[2]*1000.0)); LOG_println();

        att[0] = (att[0] * 5 + sums[0]) / 6;
        att[1] = (att[1] * 5 + sums[1]) / 6;
        att[2] = (att[2] * 5 + sums[2]) / 6;

        //Spectrum ret;
        ret.bass = sums[0];
        ret.mid = sums[1];
        ret.treb = sums[2];
        ret.bass_att = att[0];
        ret.mid_att = att[1];
        ret.treb_att = att[2];
        ret.vol = (sums[0] + sums[1] + sums[2]);

        if (1 == 1)
        {
            LOG_print("ret   ");
            LOG_format((int) (ret.bass * 1000.0));
            LOG_format((int) (ret.mid * 1000.0));
            LOG_format((int) (ret.treb * 1000.0));
            LOG_println();
        }
        return true;
    }
};


/** MAIN **/

class _DMXWriter
{
public:
    virtual void begin() = 0;
    virtual void write(CRGB data[], size_t count) = 0;
    virtual void loop() = 0;
};


#if OUTPUT_OCTO
DMAMEM int octo_displayMemory[IMAGE_SIZE * 6];
int octo_drawingMemory[IMAGE_SIZE * 6];
OctoWS2811 octo_leds(IMAGE_SIZE, octo_displayMemory, octo_drawingMemory, WS2811_GRB | WS2811_800kHz);
class RenderOcto : _DMXWriter
{
    OctoWS2811 octo_leds;

public:
    RenderOcto() : leds(IMAGE_SIZE, octo_displayMemory, octo_drawingMemory, WS2811_GRB | WS2811_800kHz)
    {
    }

    void begin() override
    {
        leds.begin();
        leds.show();
    }

    void write(CRGB data[], size_t count) override
    {
        size_t count = min(count, IMAGE_SIZE);
        for (size_t i = 0; i < count; i++)
            leds.setPixel(i, data[i].r, data[i].g, data[i].b);
        leds.show();
    }

    void loop() override
    {
    }
};
#endif


#if OUTPUT_MYDMX
class RenderMyDMX : public _DMXWriter
{
    // extra buffer for full dmx frame
    static uint8_t tx_buffer[512*3];

    HardwareSerial &serial;

public:
    RenderMyDMX(HardwareSerial &serial_) :
            serial(serial_)
    {
#ifdef TEENSY40
//        serial.addStorageForWrite(RenderDMX::tx_buffer, sizeof(RenderDMX::tx_buffer));
#endif
    }

    void begin() override
    {
    }

    void write(CRGB data[], size_t count) override
    {
        serial.flush();
        delayMicroseconds(44);
        serial.begin(100000, SERIAL_8E1);  // write out a long 0
        serial.write(0);
        serial.flush();
        delayMicroseconds(44);
        serial.begin(250000, SERIAL_8N2);
        serial.write((uint8_t)0);

        serial.write((uint8_t *)data, count*3);

        for (size_t i=size ; i<128 ; i++)
            serial.write((uint8_t)0);
    }

    void loop() override
    {}
};
#endif


class RenderDebug : public _DMXWriter
{
public:
    void begin()
    {
    }

    void write(CRGB rgb[], size_t count)
    {
        for (size_t i = 0; i < count; i++)
        {
            CRGB c = rgb[i];
            unsigned long ul = ((unsigned long)c.r << 16) + ((unsigned long)c.g << 8) + (unsigned long)c.b;
            ul |= 0x80000000;
            DebugSerial.print(ul, 16);
            DebugSerial.print(", ");
        }
        DebugSerial.println();
    }

    void loop(){}
};



class _RenderFastLED : public _DMXWriter
{
protected:
    CRGB leds[IMAGE_SIZE];

public:
    void write(CRGB data[], size_t count) override
    {
        memcpy((uint8_t *)leds, (uint8_t *)data, count*sizeof(CRGB));
        FastLED.show();
    }

    void loop() override
    {}
};
class RenderFastLEDDMX : public _RenderFastLED
{
public:
    void begin() override
    {
        FastLED.addLeds<DMXSIMPLE,DMX_TX_PIN,RGB>(leds, IMAGE_SIZE);
    }
};
class RenderFastLEDNeoPixel : public _RenderFastLED
{
public:
    void begin() override
    {
        FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(leds, IMAGE_SIZE);
    }
};



class RenderReorder : public _DMXWriter
{
  _DMXWriter &delegate;
public:
  RenderReorder(_DMXWriter &d) : delegate(d)
  {
  }

  void begin() override
  {
    delegate.begin();
  }

  void loop() override
  {
    delegate.loop();
  }

  void write(CRGB data[], size_t count) override
  {
        CRGB to[count];
        for (size_t i=0 ; i<count/2 ; i++)
        {
           to[IMAGE_SIZE-1-i] = data[(i*2)];
           to[i] = data[i*2+1];
        }
        delegate.write(to,count);
  }
};


void mapToDisplay(float maxBrightness, float vibrance, float gamma, float ledData[], CRGB rgbData[], size_t size)
{
    if (vibrance != 0.0)
    {
        for (size_t i=0 ; i<size ; i+=3)
        {
            float r = ledData[i], g = ledData[i+1], b = ledData[i+2];
            float mx = (float)fmax(fmax(r,g),b);
            float avg = (r + g + b) / 3.0f;
            float adjust = (1-(mx - avg)) * 2 * -1 * vibrance;
            //float adjust = -1 * vibrance;
            // r += (mx - r) * adjust;
            ledData[i]   = r * (1 - adjust) + adjust * mx;
            ledData[i+1] = g * (1 - adjust) + adjust * mx;
            ledData[i+2] = b * (1 - adjust) + adjust * mx;
        }
    }
    size_t count = size/3;
    for (size_t i=0 ; i<count ; i++)
    {
        rgbData[i].r = (int)round(pow(ledData[i*3+0], gamma) * maxBrightness * 255);
        rgbData[i].g = (int)round(pow(ledData[i*3+1], gamma) * maxBrightness * 255);
        rgbData[i].b = (int)round(pow(ledData[i*3+2], gamma) * maxBrightness * 255);
    }
}


class SoundFFT sound;
#if OUTPUT_FASTLED_DMX
class RenderFastLEDDMX outputFastLED;
#endif
#if OUTPUT_FASTLED_NEOPIXEL
class RenderFastLEDNeoPixel raw;
class RenderReorder outputFastLED(raw);
#endif
class RenderDebug outputDebug;
Renderer *renderPattern = createRenderer();

void testPattern()
{
    uint8_t C = 0x40;
    CRGB rgbBuffer[IMAGE_SIZE];

    unsigned long int i=1;
    while (0)
    {
        rgbBuffer[(i-1)%IMAGE_SIZE] = CRGB(0,0,0);
        rgbBuffer[(i)%IMAGE_SIZE] = CRGB(255,255,255);
        outputFastLED.write(rgbBuffer,IMAGE_SIZE);
        i++;
        delay(200);
    }

    for (size_t rep = 0; rep < 3; rep++)
    {
        for (size_t i=0; i < IMAGE_SIZE; )
        {
            if (i<IMAGE_SIZE)
            {
                rgbBuffer[i].r = C;
                rgbBuffer[i].g = 0x00;
                rgbBuffer[i].b = 0x00;
            }
            i++;
            if (i<IMAGE_SIZE)
            {
                rgbBuffer[i].r = 0x00;
                rgbBuffer[i].g = C;
                rgbBuffer[i].b = 0x00;
            }
            i++;
            if (i<IMAGE_SIZE)
            {
                rgbBuffer[i].r = 0x00;
                rgbBuffer[i].g = 0x00;
                rgbBuffer[i].b = C;
            }
            i++;
        }
        outputFastLED.write(rgbBuffer,IMAGE_SIZE);
        delay(3000);

        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = C;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = 0x00;
        }
        outputFastLED.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = C;
            rgbBuffer[i].b = 0x00;
        }
        outputFastLED.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = C;
        }
        outputFastLED.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
    }
}

void setup_()
{
#if USE_ADC
    // Power for microphone
    pinMode(ADC_MIC_GND_PIN, OUTPUT);
    digitalWrite(ADC_MIC_GND_PIN, LOW);
    pinMode(ADC_MIC_VCC_PIN, OUTPUT);
    digitalWrite(ADC_MIC_VCC_PIN, HIGH);
#endif
#if USE_I2S
    // analog volume pot
    pinMode(A1, INPUT);
#endif
    DebugSerial.begin(115200);
    for (int i=0;i<100 && !DebugSerial;i++) delay(1);
    delay(1000);

    sound.begin();

    outputFastLED.begin();

    if (1)
        testPattern();

    LOG_println("exit setup()");
};



void loop_fft()
{
    Spectrum spectrum;

    //if (fft.available())
    if (sound.next(spectrum))
    {
//        LOG_print("FFT: ");
//        for (int i=0; i<40; i++)
//        {
//            float n = fft.read(i);
//            if (n >= 0.01)
//            {
//                LOG_print(n);
//                LOG_print(" ");
//            }
//            else
//            {
//                LOG_print("  -  "); // don't print "0.00"
//            }
//        }
//        LOG_println();
    }
}


unsigned long int loop_frame = 0;
float loop_brightness = 0.5f;

void loop_()
{
    Spectrum spectrum;
    if (!sound.next(spectrum))
      return;

    loop_frame++;
    
    LOG_println(""); LOG_print("frame"); LOG_println(loop_frame);
    float f32values[IMAGE_SIZE*3];
    CRGB rgbValues[IMAGE_SIZE];

    LOG_println("renderFrame");
    DebugSerial.flush();
    delay(1);
    renderPattern->renderFrame(millis() / 1000.0, &spectrum, f32values, IMAGE_SIZE*3);

    // LOG_println("mapToDisplay");
    mapToDisplay(loop_brightness, 0.0, 2.5, f32values, rgbValues, IMAGE_SIZE*3);
 
    // LOG_println("outputFastLED.write");
#if OUTPUT_FASTLED_DMX || OUTPUT_FASTLED_NEOPIXEL
    outputFastLED.write(rgbValues, IMAGE_SIZE);
#endif
#if OUTPUT_DEBUG
    outputDebug.write(rgbValues, IMAGE_SIZE);
#endif

#if USE_I2S
      int volPot = analogRead(A1);
      loop_brightness += 0.1 * (loop_brightness - (volPot/1023.0)); // smooth to reduce noise
      // LOG_print(volPot);
#endif

}
