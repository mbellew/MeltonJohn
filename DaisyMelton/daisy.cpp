#include "DaisyMelton.h"

#include <AudioClass.h>
#include <CpuLoadMeter.h>
#include <DaisyDSP.h>
#include <DaisyDuino.h>
#include <PersistentStorage.h>
#include <hal_conf_extra.h>
#include <U8g2lib.h>


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

using namespace daisy;
using namespace daisysp;

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "Patterns.h"
//#include <Audio.h>
//#include <Wire.h>
//#include <SPI.h>
//#include <SD.h>
//#include <SerialFlash.h>
#if OUTPUT_FASTLED_DMX
#include "DmxSimple.h"
#endif
//#include <FastLED.h>

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


#if USE_I2S || USE_ADC
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
#endif


class SpectrumAnalyzer
{
  public:
      virtual bool next(Spectrum &spectrum) = 0;
};


#if USE_MSGEQ7
#error Not expected for Daisy
#include "MD_MSGEQ7.h"

class SoundFFT : Spectrum Analyzer
{
    const int eqVDD = 19;
    const int eqRESET = 18;
    const int eqSTROBE = 17;
    const int eqOUTL = 16;
    const int eqOUTR = 15;
    const int eqGND = 14;
    const unsigned frameInterval = 1000/44;

    MD_MSGEQ7 MSGEQ7;

    unsigned long frame_count = 0;
    unsigned long nextFrameStartTime = 0;

    float h_levels[MAX_BAND]        = {1000,1000,1000,1000,1000,1000,1000};
    float h_inv_levels[MAX_BAND]    = {1.0/1000,1.0/1000,1.0/1000,1.0/1000,1.0/1000,1.0/1000,1.0/1000};  // running sum of inv levels
    float h_inv_minlevels[MAX_BAND] = {1.0/400,1.0/500,1.0/600,1.0/700,1.0/600,1.0/500,1.0/500};    // running sum (< levels[i])
    float h_vol_level = 1000;
    float bass_att = 0;
    float mid_att = 0;
    float treb_att = 0;


public:
    SoundFFT() : MSGEQ7(eqRESET, eqSTROBE, eqOUTL)
    {
    }

    void begin()
    {
        analogReadResolution(13);
        pinMode(eqGND,OUTPUT);
        digitalWrite(eqGND,0);
        pinMode(eqVDD,OUTPUT);
        digitalWrite(eqVDD,1);
        delay(1);
        MSGEQ7.begin();
        nextFrameStartTime = millis() + frameInterval;
    };



        double count_ = 0;
        double sum_ = 0;
        double min_ = 10000;
        double max_ = -10000;

    uint16_t minimums[MAX_BAND] = {10000,10000,10000,10000,10000,10000,10000};

    bool next(Spectrum &spectrum) 
    {
        float raw_level[MAX_BAND];

        if (millis() < nextFrameStartTime)
            return false;
        nextFrameStartTime += frameInterval;

        MSGEQ7.read();


        for (size_t i=0; i<MAX_BAND; i++)
        {
        frame_count++;
        uint16_t l = MSGEQ7.get(i);
        //LOG_format(l);
        minimums[i] = min(minimums[i],l);
        float level = fmaxf((float)(l-minimums[i]), 1.0f);
        float inv_level = 1.0/level;

        if (-1==6)
        {
            count_ += 1.0;
            sum_ += level;
            min_ = fminf(min_,level);
            max_ = fmaxf(max_,level);
            //LOG_format(sum_/count_); LOG_format(min_); LOG_format(max_);
            LOG_format((int)level);
            LOG_println();
        }

        h_levels[i]     = h_levels[i] * 0.99 + level * 0.01;
        h_inv_levels[i] = h_inv_levels[i] * 0.99 + inv_level * 0.01;
        if (inv_level > h_inv_levels[i])
        {
            if (inv_level > h_inv_minlevels[i] || frame_count < 100)
            h_inv_minlevels[i] = h_inv_minlevels[i] * 0.99 + inv_level * 0.01;
            else
            h_inv_minlevels[i] = h_inv_minlevels[i] * 0.999 + inv_level * 0.001;
        }
        raw_level[i] = level;
        }

        float avg_level[MAX_BAND];
        float curr_level[MAX_BAND];
        float v = 0.0;

        // skip band 7
        for (size_t i=0; i<6; i++)
        {
        float min_level = 1.0/h_inv_minlevels[i];
        avg_level[i]  = fmaxf(h_levels[i] - min_level, 20);
        curr_level[i] = fmaxf(raw_level[i] - min_level, 0);
        v += curr_level[i];
        }
        v /= 6.0;
        if (frame_count < 100)
        h_vol_level = h_vol_level * 0.99 + v * 0.01;
        else
        h_vol_level = h_vol_level * 0.995 + v * 0.005;

        float avg_v = fmaxf(h_vol_level,20);
        float b     = (curr_level[0] + curr_level[1]) / 2;
        float avg_b = ( avg_level[0] +  avg_level[1]) / 2;
        float m     = (curr_level[2] + curr_level[3]) / 2;
        float avg_m = ( avg_level[2] +  avg_level[3]) / 2;
        float t     = (curr_level[4] + curr_level[5]) / 2;
        float avg_t = ( avg_level[4] +  avg_level[5]) / 2;

        const float formula[3] = {1.0, 1.0, 0.0};
        spectrum.bass = (formula[0]*b) / (formula[1]*avg_v + formula[2]*avg_b);
        spectrum.mid  = (formula[0]*m) / (formula[1]*avg_v + formula[2]*avg_m);
        spectrum.treb = (formula[0]*t) / (formula[1]*avg_v + formula[2]*avg_t);
        spectrum.bass_att = bass_att = (bass_att * 5 + spectrum.bass) / 6;
        spectrum.mid_att  = mid_att  = ( mid_att * 5 + spectrum.mid) / 6;
        spectrum.treb_att = treb_att = (treb_att * 5 + spectrum.treb) / 6;
        spectrum.vol = v / avg_v;

        LOG_format((int)(spectrum.bass*1000));
        LOG_format((int)(spectrum.mid*1000));
        LOG_format((int)(spectrum.treb*1000));
        LOG_println();

        return  true;
    }
};
#endif // USE_MSGEQ7

/** MAIN **/

#if OUTPUT_OCTO
#include <OctoWS2811.h>
DMAMEM int octo_displayMemory[IMAGE_SIZE * 6];
int octo_drawingMemory[IMAGE_SIZE * 6];
OctoWS2811 octo_leds(IMAGE_SIZE, octo_displayMemory, octo_drawingMemory, WS2811_GRB | WS2811_800kHz);
class RenderOcto : OutputLED
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

    void write(const CRGB data[], unsigned count) override
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
class RenderTeensyDMX : public OutputLED
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

    void write(const CRGB data[], unsigned count) override
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


SpectrumAnalyzer *sound = NULL;
#if OUTPUT_TEENSYDMX
class RenderTeensyDMX outputTeensyDmx(Serial1);
class OutputLED *output=&outputTeensyDmx;
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




DaisyHardware hw;

// the magic incantation
U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI
    oled(U8G2_R0, /* clock=*/8, /* data=*/10, /* cs=*/7, /* dc=*/9);




void setup_daisy()
{
    hw = DAISY.init(DAISY_FIELD, AUDIO_SR_48K);
    float sample_rate = DAISY.AudioSampleRate();

    oled.setFont(u8g2_font_inb16_mf);
    oled.setFontDirection(0);
    oled.setFontMode(1);
    oled.begin();

    // DAISY.StartAudio(AudioCallback);
    oled.clearBuffer();
    oled.drawStr(10, 10, "melton");
};



void loop_fft()
{
    static Spectrum spectrum;

    if (NULL == sound)
      return;

    //if (fft.available())
    if (sound->next(spectrum))
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



void loop_daisy()
{
//     static unsigned long int loop_frame = 0;
//     static Spectrum spectrum;

//     if (!sound.next(spectrum))
//       return;

//     loop_frame++;
    
// //    LOG_println(""); LOG_print("frame"); LOG_println(loop_frame);
//     float f32values[IMAGE_SIZE*3];
//     CRGB rgbValues[IMAGE_SIZE];

// //    LOG_println("renderFrame");
//     DebugSerial.flush();
//     delay(1);
//     renderPattern->renderFrame(millis() / 1000.0, &spectrum, f32values, IMAGE_SIZE*3);

//     // LOG_println("mapToDisplay");
//     mapToDisplay(f32values, rgbValues, IMAGE_SIZE*3);
 
//     // LOG_println("outputFastLED.write");
//     output->write(rgbValues, IMAGE_SIZE);
// #if OUTPUT_DEBUG
//     outputDebug->write(rgbValues, IMAGE_SIZE);
// #endif

// #if USE_I2S
// //      int volPot = analogRead(A1);
// //      loop_brightness = 0.5;// += 0.1 * (loop_brightness - (volPot/1023.0)); // smooth to reduce noise
// #endif
}
