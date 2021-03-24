#include "TeensyMelton.h"
#ifdef PLATFORM_TEENSY

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "Patterns.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
//#include <SerialFlash.h>
#if OUTPUT_FASTLED_DMX
#include "DmxSimple.h"
#endif
#include <FastLED.h>

#define DebugSerial Serial

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

//#define max(a, b) ((a)>(b)?(a):(b))
//#define min(a, b) ((a)<(b)?(a):(b))


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
            _print("ret   ");
            _format((int) (ret.bass * 1000.0));
            _format((int) (ret.mid * 1000.0));
            _format((int) (ret.treb * 1000.0));
            _println();
        }
        return true;
    }
};


/** MAIN **/

#if OUTPUT_OCTO
#include <OctoWS2811.h>
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


#if OUTPUT_TEENSYDMX
#include <TeensyDMX.h>
class RenderTeensyDMX : public _DMXWriter
{
    ::qindesign::teensydmx::Sender dmxTx;

public:
    RenderTeensyDMX(HardwareSerial &serial_) : dmxTx(serial_)
    {
    }

    void begin() override
    {
        dmxTx.begin();
        dmxTx.setPacketSize(IMAGE_SIZE*3);
        dmxTx.resume();
    }

    void write(CRGB data[], size_t count) override
    {
        dmxTx.set(0,(uint8_t*)data,count*3);
    }

    void loop() override
    {}
};
#endif


class RenderReorder : public OutputLED
{
  OutputLED &delegate;
public:
  RenderReorder(OutputLED &d) : delegate(d)
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

  void write(const CRGB data[], size_t count) override
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


class SoundFFT sound;
#if OUTPUT_TEENSYDMX
class RenderTeensyDMX outputTeensyDmx(Serial1);
class RenderTeensyDMX &output=outputTeensyDmx;
#endif
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
        output->write(rgbBuffer,IMAGE_SIZE);
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
        output->write(rgbBuffer,IMAGE_SIZE);
        delay(3000);

        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = C;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = 0x00;
        }
        output->write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = C;
            rgbBuffer[i].b = 0x00;
        }
        output->write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = C;
        }
        output->write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
    }
}

void setup_teensy()
{
#if USE_ADC
    // on octo shield this give us
    // 1 23 22 19 18
    // G  V  G AD  V
    pinMode(1, OUTPUT);
    digitalWrite(ADC_MIC_GND_PIN, LOW);

    pinMode(ADC_MIC_VCC_PIN, OUTPUT);
    digitalWrite(ADC_MIC_VCC_PIN, HIGH);

    pinMode(ADC_MIC_GND_PIN, OUTPUT);
    digitalWrite(ADC_MIC_GND_PIN, LOW);

    // 19 is controlled by AudioInputAnalog

    pinMode(18, OUTPUT);
    digitalWrite(18, HIGH);
#endif
#if USE_I2S
    // analog volume pot
    pinMode(A1, INPUT);
#endif
    DebugSerial.begin(115200);
    for (int i=0;i<100 && !DebugSerial;i++) delay(1);
    delay(1000);

    sound.begin();

    output->begin();

    if (0) testPattern();

    _println("exit setup()");
};



void loop_fft()
{
    static Spectrum spectrum;

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



void loop_teensy()
{
    static unsigned long int loop_frame = 0;
    static Spectrum spectrum;

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
    mapToDisplay(f32values, rgbValues, IMAGE_SIZE*3);
 
    // LOG_println("outputFastLED.write");
    output->write(rgbValues, IMAGE_SIZE);
#if OUTPUT_DEBUG
    outputDebug->write(rgbValues, IMAGE_SIZE);
#endif

#if USE_I2S
//      int volPot = analogRead(A1);
//      loop_brightness = 0.5;// += 0.1 * (loop_brightness - (volPot/1023.0)); // smooth to reduce noise
#endif
}


#endif // PLATFORM_TEENSY