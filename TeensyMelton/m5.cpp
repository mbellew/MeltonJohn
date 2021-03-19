#include "config.h"
#ifdef PLATFORM_M5STACK

#include <M5StickCPlus.h>
#undef WHITE
#undef BLACK
#undef RED
#undef GREEN
#undef BLUE
#include <driver/i2s.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "Patterns.h"
#if OUTPUT_FASTLED_DMX
#include "DmxSimple.h"
#endif
#include <FastLED.h>
#include "PCM.hpp"
#include "BeatDetect.hpp"

#define DebugSerial Serial

#if DEBUG_LOG
#define LOG_print(x) DebugSerial.print(x)
//#define LOG_println() SerialDebugSerial.println()
#define LOG_println(x) DebugSerial.println(x)
#else
#define LOG_print(x)
#define LOG_println(x)
#endif

// I2S microphone configuration
#define PIN_CLK  0
#define PIN_DATA 34
#define GAIN_FACTOR 3

inline uint16_t TFT_COLOR(Color c)
{
    return ((((uint16_t)(32*c.r()))&0x1f) << 11) |
           ((((uint16_t)(64*c.g()))&0x3f) << 5) |
           ((((uint16_t)(32*c.b()))&0x3f) << 0);
}
inline uint16_t TFT_CRGB(CRGB c)
{
    return ((c.r>>3) << 11) |
           ((c.g>>2) << 5) |
           ((c.g>>3) << 0);
}


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


struct SoundFFT
{
    float volume_level = 20;
    float att[3] = {20, 20, 20};

    PCM pcm;
    BeatDetect beatDetect;
    int samples = 0;

    SoundFFT() : beatDetect(&pcm)
    {
    }

    void begin()
    {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
            .sample_rate =  44100,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
            .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
            .communication_format = I2S_COMM_FORMAT_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 2,
            .dma_buf_len = 128,
        };

        i2s_pin_config_t pin_config;
        pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
        pin_config.ws_io_num    = PIN_CLK;
        pin_config.data_out_num = I2S_PIN_NO_CHANGE;
        pin_config.data_in_num  = PIN_DATA;

        i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        i2s_set_pin(I2S_NUM_0, &pin_config);
        i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    }

    void loop()
    {
        size_t bytesread;
        int16_t buf[128];
        do
        {
            i2s_read(I2S_NUM_0, (char *) buf, sizeof(buf), &bytesread, 0);
            if (bytesread)
            {
                pcm.addPCM16_1ch((short *) buf, bytesread / 2);
                samples += bytesread / 2;
            }
        } while (bytesread > 0);
    }

    bool available()
    {
        if (samples < 1024)
            loop();
        if (samples < 1024)
            return false;
        samples -= 1024;
        return true;
    }

    /** return three numbers between 0.0 and 1.0 **/
    boolean next(Spectrum &spectrum)
    {
        if (!available())
            return false;

        beatDetect.detectFromSamples();
        // shim between projectM class and shared Patterns code
        spectrum.bass = beatDetect.bass;
        spectrum.bass_att = beatDetect.bass_att;
        spectrum.mid = beatDetect.mid;
        spectrum.mid_att = beatDetect.mid_att;
        spectrum.treb = beatDetect.treb;
        spectrum.treb_att = beatDetect.treb_att;
        spectrum.vol = beatDetect.vol;
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


#if OUTPUT_MYDMX
class RenderMyDMX : public _DMXWriter
{
    HardwareSerial &serial;

public:
    RenderMyDMX(HardwareSerial &serial_) :
            serial(serial_)
    {
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

        for (size_t i=count*3 ; i<IMAGE_SIZE*3 ; i++)
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
#if OUTPUT_FASTLED_DMX
class RenderFastLEDDMX : public _RenderFastLED
{
public:
    void begin() override
    {
        FastLED.addLeds<DMXSIMPLE,DMX_TX_PIN,RGB>(leds, IMAGE_SIZE);
    }
};
#endif
#if OUTPUT_FASTLED_NEOPIXEL
class RenderFastLEDNeoPixel : public _RenderFastLED
{
public:
    void begin() override
    {
        FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(leds, IMAGE_SIZE);
    }
};
#endif


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


class RenderLCD : public _DMXWriter
{
public:
    void begin() override {}
    void loop() override{}

    void write(CRGB data[], size_t cont) override
    {
        size_t width = 360/IMAGE_SIZE;
        for (int i=0 ; i<IMAGE_SIZE ; i++)
        {
            uint16_t color = TFT_CRGB(data[i]);
            M5.Lcd.fillRect(0,i*width,240,width,color);
        }
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
#if OUTPUT_MYDMX
class RenderMyDMX outputDmx(Serial1);
class RenderMyDMX &output=outputDmx;
#endif
#if OUTPUT_TEENSYDMX
class RenderTeensyDMX outputTeensyDmx(Serial1);
class RenderTeensyDMX &output=outputTeensyDmx;
#endif
#if OUTPUT_FASTLED_DMX
class RenderFastLEDDMX outputFastLED;
class RenderFastLEDDMX &output=outputFastLED;
#endif
#if OUTPUT_FASTLED_NEOPIXEL
class RenderFastLEDNeoPixel outputFastLED;
class RenderFastLEDNeoPixel &output=outputFastLED;
//class RenderReorder outputReorder(outputFastLED);
//class RenderReorder &output(outputReorder);
#endif
#if OUTPUT_M5LCD
class RenderLCD outputLCD;
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
        output.write(rgbBuffer,IMAGE_SIZE);
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
        output.write(rgbBuffer,IMAGE_SIZE);
        delay(3000);

        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = C;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = 0x00;
        }
        output.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = C;
            rgbBuffer[i].b = 0x00;
        }
        output.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            rgbBuffer[i].r = 0x00;
            rgbBuffer[i].g = 0x00;
            rgbBuffer[i].b = C;
        }
        output.write(rgbBuffer,IMAGE_SIZE);
        delay(1000);
    }
}


void setup_()
{
    if (NEOPIXEL_PIN != G36)
    {
        pinMode(G36, INPUT);
        digitalWrite(G36, LOW);
    }
    if (NEOPIXEL_PIN != G0)
    {
        pinMode(G0, INPUT);
        digitalWrite(G0, LOW);
    }
    if (NEOPIXEL_PIN != G26)
    {
        pinMode(G26, INPUT);
        digitalWrite(G26, LOW);
    }
    M5.begin();
    for (int i=0;i<100 && !DebugSerial;i++) delay(10);

    M5.Lcd.fillScreen(TFT_BLACK);

    sound.begin();

    output.begin();
    if (0) testPattern();

    LOG_println("exit setup()");
};



void loop_fft()
{
    static Spectrum spectrum;

    if (sound.next(spectrum))
    {
        LOG_print(spectrum.bass);
        LOG_print(" " );
        LOG_print(spectrum.mid);
        LOG_print(" " );
        LOG_print(spectrum.treb);
        LOG_println();

//        float fft[40];
//        sound.pcm.getSpectrum(fft, CHANNEL_0, 40, 0.0);
//        LOG_print("FFT: ");
//        for (int i=0; i<40; i++)
//        {
//            float n = fft[i];
//            if (n >= 0.001)
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



void loop_()
{
    static unsigned long int loop_frame = 0;
    static float loop_brightness = 1.0f;
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
    mapToDisplay(loop_brightness, 0.0, 2.5, f32values, rgbValues, IMAGE_SIZE*3);
 
    // LOG_println("outputFastLED.write");
    output.write(rgbValues, IMAGE_SIZE);

#if OUTPUT_M5LCD
    outputLCD.write(rgbValues, IMAGE_SIZE);
#endif

#if OUTPUT_DEBUG
    outputDebug.write(rgbValues, IMAGE_SIZE);
#endif
}



int32_t random(void)
{
    static uint64_t seed = millis() * micros();
    seed = seed * 2147483647 + 16807;
    return (seed >> 16u) % (RAND_MAX + 1u);
}


#endif //PLATFORM_M5