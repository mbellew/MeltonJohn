#include "TeensyMelton.h"
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
#include <FastLED.h>                // provides CRGB
#include <NeoPixelBus.h>
#include "PCM.hpp"
#include "BeatDetect.hpp"


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
            .dma_buf_count = 4,
            .dma_buf_len = 256,
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

    /* read all available data w/o waiting */
    void loop()
    {
        size_t bytesread;
        static int16_t buf[256];
        static float f[256];
        do
        {
            i2s_read(I2S_NUM_0, (char *) buf, sizeof(buf), &bytesread, 0);
            if (bytesread)
            {
                size_t count = bytesread/2;
                for (int i=0 ; i<count ; i++)
                    f[i] = buf[i] / 16384.0f;
                pcm.addPCMfloat(f, count);
                samples += bytesread / 2;
            }
        } while (bytesread == sizeof(buf));
    }

    /* check if 1024 new samples are available (since last time available() returned true) */
    bool available()
    {
        if (samples < 1024)
            loop();
        if (samples < 1024)
            return false;
        samples -= 1024;
        return true;
    }

    /* return updated bass/mid/treb if new samples are available */
    bool next(Spectrum &spectrum)
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


#if OUTPUT_NEOPIXELBUS
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(IMAGE_SIZE, NEOPIXEL_PIN);
class RenderNeoPixelBus : public OutputLED
{
    CRGB leds[IMAGE_SIZE];
public:
    void begin() override
    {
        strip.Begin();
        strip.Show();
    }

    void write(const CRGB data[], size_t count) override
    {
        count = count > IMAGE_SIZE ? IMAGE_SIZE : count;
        uint8_t *pixels = strip.Pixels();
        memcpy(pixels,data,count*3);
        strip.Dirty();
        strip.Show();
    }
};
#endif


class RenderLCD : public OutputLED
{
    const size_t displayW = 185, displayH=240;
public:
    void begin() override {}
    void loop() override{}

    void write(const CRGB data[], size_t cont) override
    {
        size_t h = displayH/IMAGE_SIZE;
        for (int i=0 ; i<IMAGE_SIZE ; i++)
        {
            uint16_t color = TFT_CRGB(data[i]);
            M5.Lcd.fillRect(0,i*h,displayW,h,color);
        }
    }
};


#ifdef OUTPUT_SPECTRUM
#define drawPixel(a, b, c) M5.Lcd.setAddrWindow(a, b, a, b); M5.Lcd.pushColor(c)
extern SoundFFT sound;

class RenderSpectrum : public OutputLED
{
    const size_t displayW = 185, displayH=240;
public:
    void begin() override {}
    void loop() override{}

    void write(const CRGB data[], size_t cont) override
    {
        float spectrum[displayH];
        sound.pcm._getSpectrum(spectrum, CHANNEL_0, displayH, 0.9);
        M5.Lcd.fillScreen(TFT_BLACK);
        int ypos=0;
        for (int i=0 ; i<displayH && ypos < 240 ; i++)
        {
            int h = 1;
            unsigned color = TFT_LIGHTGREY;
            if (i < 4)
            {
                h = 4;
                color = TFT_BLUE;
            }
            else if (i<23)
            {
                h = 2;
                color = TFT_YELLOW;
            }
            int x = (int)(spectrum[i] * (displayW / 40.0f));
            for (int y=ypos ; y<ypos+h ; y++)
            {
                M5.Lcd.drawFastHLine(0, y, x, color);
                drawPixel(x, y, TFT_RED);
            }
            ypos += h;
        }
    }
};
#endif


struct RTCWrapper
{
    static float rtcTime;
    static unsigned long millisAtTime;
    static bool forceReset;

    static void setup()
    {
        RTC_TimeTypeDef rtcTemp1, rtcNow;
        M5.Rtc.GetTime(&rtcTemp1);
        do
        {
            M5.Rtc.GetTime(&rtcNow);
        } while (rtcTemp1.Seconds == rtcNow.Seconds);
        millisAtTime = millis();
        rtcTime = (rtcNow.Hours*60*60) + (rtcNow.Minutes*60) + rtcNow.Seconds;
    }

    static float time() // seconds since midnight
    {
        unsigned long currentMillis = millis();
        unsigned long elapsed = currentMillis<millisAtTime ? currentMillis : currentMillis-millisAtTime;
        return rtcTime + (elapsed/1000.0f);
    }

    // mostly noop, occassionally start polling the RTC say 1/minute
    static void loop()
    {
        static bool polling = false;
        static RTC_TimeTypeDef rtcPollingStart = {0};

        if (polling)
        {
            RTC_TimeTypeDef rtcNow;
            M5.Rtc.GetTime(&rtcNow);
            if (rtcNow.Seconds != rtcPollingStart.Seconds)
            {
                 millisAtTime = millis();
                 rtcTime = (rtcNow.Hours*60*60) + (rtcNow.Minutes*60) + rtcNow.Seconds;
                 polling = false;
                 forceReset = false;
            }
        }
        else
        {
            unsigned long m = millis();
            if (forceReset || m<millisAtTime || m-millisAtTime > 60*1000)
            {
                polling = true;
                M5.Rtc.GetTime(&rtcPollingStart);
            }
        }
    }

    /* if we have interrupts hooked up in some useful way... no access to CLKOUT unfortunately
    static void updateOnInterrupt()
    {
        RTC_TimeTypeDef rtcTemp;
        M5.Rtc.GetTime(&rtcTemp);
        unsigned long m = millis();
        Serial.println(m);
        float f = (rtcTemp.Hours*60*60) + (rtcTemp.Minutes*60) + rtcTemp.Seconds;
        if (f != rtcTime)
        {
            rtcTime = f;
            millisAtTime = m;
            Serial.print("rtcTime:  "); Serial.println(f);
            Serial.print("millis(): "); Serial.println(m);
        }
    }*/
};

float RTCWrapper::rtcTime=0;
unsigned long RTCWrapper::millisAtTime=0;
bool RTCWrapper::forceReset=false;



struct TimeParser
{
  Stream &input;

  TimeParser(Stream &s) : input(s)
  {
  }

  void set_time(char *sz)
  {
    if (8 != strlen(sz))
      return;
    if (sz[2] != ':' || sz[5] != ':')
      return;
_println(sz);
    sz[2] = 0;
    sz[5] = 0;
    RTC_TimeTypeDef TimeStruct;
    TimeStruct.Hours   = atoi(sz+0);;
    TimeStruct.Minutes = atoi(sz+3);
    TimeStruct.Seconds = atoi(sz+6);
    M5.Rtc.SetTime(&TimeStruct);
    RTCWrapper::forceReset = true;
  }

  void loop()
  {
    static char serial_buffer[10];
    static size_t serial_len=0;
    static size_t serial_lastchar = '\n';

    while (input.available())
    {
      uint8_t ch = input.read();
      if (ch == '\n')
      {
        if (serial_len == 8)
        {
          serial_buffer[serial_len++] = 0;
          set_time(serial_buffer);
        }
      }
      else if (ch == ':')
      {
        if (serial_len==2 || serial_len==5)
        {
          serial_buffer[serial_len++] = ch;
          serial_buffer[serial_len] = 0;
        }
        else ch = 0;
      }
      else if (ch >= '0' && ch <= '9')
      {
          if ((serial_len==0 && serial_lastchar=='\n') || (0<serial_len && serial_len<8))
          {
            serial_buffer[serial_len++] = ch;
            serial_buffer[serial_len] = 0;
          }
          else ch = 0;
      }
      if (ch == '\n' || ch == 0)
        serial_len = 0;
      serial_lastchar = ch;
    }
  }
};



class SoundFFT sound;
#if OUTPUT_M5LCD
class RenderLCD outputLCD;
#endif
#if OUTPUT_SPECTRUM
RenderSpectrum outputSpectrum;
#endif
#if OUTPUT_NEOPIXELBUS
RenderNeoPixelBus outputNeoPixelBus;
OutputLED *output = &outputNeoPixelBus;
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


extern void display_task(void *pv);

void setup_m5()
{
    // disable GPIO 25
    pinMode(GPIO_NUM_25,INPUT);
    gpio_pulldown_dis(GPIO_NUM_25);
    gpio_pullup_dis(GPIO_NUM_25);

    M5.begin();
    
    if (NEOPIXEL_PIN != G36)
    {
        pinMode(G36, INPUT);
        digitalWrite(G36, HIGH);
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
    for (int i=0;i<100 && !Serial;i++) delay(10);
    pinMode(G36,INPUT);
    digitalWrite(G36,HIGH);
    Serial1.begin(2400, SERIAL_8N1, G36, -1, false);  // baud, config, rx, tx, invert

    M5.Lcd.fillScreen(TFT_BLACK);

    sound.begin();
    output->begin();
    if (0) testPattern();

    RTCWrapper::setup();
    _println("exit setup()");

    xTaskCreate(display_task, "display", 8*1024, NULL, 1, NULL);
};

TimeParser timeParser(Serial1);


void loop_m5()
{
    timeParser.loop();
    RTCWrapper::loop();
    M5.update();
    vTaskDelay(1 / portTICK_PERIOD_MS);
}


void loop_fft()
{
    static Spectrum spectrum;

    if (sound.next(spectrum))
    {
        _print(spectrum.bass);
        _print(" " );
        _print(spectrum.mid);
        _print(" " );
        _print(spectrum.treb);
        _println("");

        float fft[40];
        sound.pcm.getSpectrum(fft, CHANNEL_0, 40, 0.0);
        _print("FFT: ");
        for (int i=0; i<40; i++)
        {
            float n = fft[i];
            if (n >= 0.001)
            {
                _print(n);
                _print(" ");
            }
            else
            {
                _print("  -  "); // don't print "0.00"
            }
        }
        _println("");
    }
}


void display_loop()
{
    static unsigned long int loop_frame = 0;
    static Spectrum spectrum;

    if (sound.next(spectrum))
        return;
    loop_frame++;

    //_println(""); _print("frame"); _println(loop_frame);
    float f32values[IMAGE_SIZE*3];
    CRGB rgbValues[IMAGE_SIZE];

    //renderPattern->renderFrame(millis() / 1000.0, &spectrum, f32values, IMAGE_SIZE*3);
    float time = RTCWrapper::time();
//    char buf[20];
//    snprintf(buf, 20, "%f", time);
//    _println(buf);
    renderPattern->renderFrame(time, &spectrum, f32values, IMAGE_SIZE*3);

    // _println("mapToDisplay");
    mapToDisplay(f32values, rgbValues, IMAGE_SIZE*3);

    // _println("outputFastLED.write");
    output->write(rgbValues, IMAGE_SIZE);

#if OUTPUT_M5LCD
    outputLCD.write(rgbValues, IMAGE_SIZE);
#endif
#if OUTPUT_SPECTRUM
    outputSpectrum.write(rgbValues, IMAGE_SIZE);
#endif
#if OUTPUT_DEBUG
    outputDebug->write(rgbValues, IMAGE_SIZE);
#endif
}


void display_task(void *pv)
{
    for (;;)
    {
        display_loop();
        delay(1);
    }
}


int32_t random(void)
{
    static uint64_t seed = millis() * micros();
    seed = seed * 2147483647 + 16807;
    return (seed >> 16u) % (RAND_MAX + 1u);
}


#endif //PLATFORM_M5
