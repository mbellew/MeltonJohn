#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "Patterns.h"
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
//#include <OctoWS2811.h>


#ifdef SERIAL_PORT_HARDWARE_OPEN6
  #define TEENSY40
  HardwareSerial &SERIAL_PORT_DMX  = Serial1;
#else
  HardwareSerial &SERIAL_PORT_DMX  = Serial1;
#endif

#if 1
#define LOG_print(x) Serial.print(x)
#define LOG_println(x) Serial.println(x)
#else
#define LOG_print(x)
#define LOG_println(x)
#endif

const int MIC_ANALOG_IN = A5;
const int BATT_ANALOG_IN = A3;


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

void P(int);


//void rotate_wheel(unsigned long int deg, const byte (&in)[12], byte (&out)[12]);
//void generate_wheel(unsigned long int deg, uint8_t mx, byte (&out)[12]);
//void generate_wheel60(unsigned long int deg, uint8_t mx, byte (&out)[60]);
float _sin[360];


void P(int n)
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


    
    AudioInputI2S adc;
    AudioAnalyzeFFT1024 fft;
    AudioConnection patchCord(adc, 0, fft, 0);
    AudioControlSGTL5000 audioShield;

struct SoundFFT
{
    
    float volume_level = 20;
    float att[3] = {20, 20, 20};

    SoundFFT() // :  adc(), fft(), patchCord(adc, 0, fft, 0), audioShield()
    {
    }

    void begin()
    {
        AudioMemory(12);
        audioShield.enable();
        audioShield.inputSelect(AUDIO_INPUT_LINEIN);
        fft.windowFunction(AudioWindowHanning1024);
    }

    /** return three numbers between 0.0 and 1.0 **/
    boolean next(Spectrum &ret)
    {
        int count = 0;
        float sums[] = {0, 0, 0};
        if (!fft.available())
          return false;
          
        sums[0] += fft.read(0, 1);
        sums[1] += fft.read(2, 8);
        sums[2] += fft.read(9, 16);
        count++;
       
if (0)
{
  for (int i=0; i<40; i++) {
float n = fft.read(i);
if (n >= 0.01) {
Serial.print(n);
Serial.print(" ");
} else {
Serial.print("  -  "); // don't print "0.00"
}
}
Serial.println();
}

        sums[0] /= count;
        sums[1] /= count;
        sums[2] /= count;
        //LOG_print("count "); P(count); LOG_println();
        //LOG_print("raw   "); P((int)(sums[0]*1000.0)); P((int)(sums[1]*1000.0)); P((int)(sums[2]*1000.0)); LOG_println();

        float level = max(0.01, (sums[0] + sums[1] + sums[2]) / 3.0);
        volume_level = (volume_level * 29.0 + level) / 30.0;
        //LOG_print("vol   "); P(volume_level*1000.0); LOG_println();

        // subtract overtones???
//        sums[2] -= sums[1] / 4;
//        sums[1] -= sums[0] / 4;

        // auto-level
        const float SCALE = 1.5;
        sums[0] *= SCALE / volume_level;
        sums[1] *= SCALE / volume_level;
        sums[2] *= SCALE / volume_level;

        LOG_print("level "); P((int)(sums[0]*1000.0)); P((int)(sums[1]*1000.0)); P((int)(sums[2]*1000.0)); LOG_println();

/*
    // favorite mapping function goes here
    Spectrum scaled = {
      atan((ratio.lo-1)/3)  / (M_PI/2),
      atan((ratio.med-1)/3) / (M_PI/2),
      atan((ratio.hi-1)/3)  / (M_PI/2)
    };
*/
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

        if (1 == 0)
        {
            LOG_print("ret   ");
            P((int) (ret.bass * 1000.0));
            P((int) (ret.mid * 1000.0));
            P((int) (ret.treb * 1000.0));
            LOG_println();
        }
        return true;
    }
};


/** MAIN **/

class Renderer
{
public:
    virtual void begin() = 0;
    virtual void write(uint8_t data[], size_t size) = 0;
};


#if 0
class RenderOcto : Renderer
{
    static DMAMEM int displayMemory[512 * 6];
    static int drawingMemory[512 * 6];
    size_t ledCount = 512;
    OctoWS2811 leds;

public:
    RenderOcto(size_t ledCount_) :
            ledCount(min(512, ledCount_)),
            leds(min(512, ledCount_), displayMemory, drawingMemory, WS2811_GRB | WS2811_800kHz)
    {
    }

    void begin()
    {
        leds.begin();
        leds.show();
    }

    void write(uint8_t data[], size_t size)
    {
        size_t count = min(size, 3 * ledCount) / 3;
        for (size_t i = 0; i < count; i++)
            leds.setPixel(i, data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2]);
        leds.show();
    }
};
#endif


class RenderDMX : Renderer
{
    // extra buffer for full dmx frame
    static uint8_t tx_buffer[512*3];

    HardwareSerial &serial;

public:
    RenderDMX(HardwareSerial &serial_) :
            serial(serial_)
    {
#ifdef TEENSY40
//        serial.addStorageForWrite(RenderDMX::tx_buffer, sizeof(RenderDMX::tx_buffer));
#endif
    }

    void begin()
    {
    }

    void write(byte data[], size_t size)
    {
        serial.flush();
        delayMicroseconds(44);
        serial.begin(100000, SERIAL_8E1);  // write out a long 0
        serial.write(0);
        serial.flush();
        delayMicroseconds(44);
        serial.write(0);
        serial.begin(250000, SERIAL_8N2);
        for (size_t i = 0; i < size; i++)
            serial.write(data[i]);
    }
};

class RenderDebug : Renderer
{
public:
    void begin()
    {
    }

    void write(uint8_t data[], size_t size)
    {
        size_t count = size / 3;
        for (size_t i = 0; i < count; i++)
        {
            unsigned long ul = ((unsigned long)data[i * 3 + 0] << 16) + ((unsigned long)data[i * 3 + 1] << 8) + (unsigned long)data[i * 3 + 2];
            ul |= 0x80000000;
            Serial.print(ul, 16);
            Serial.print(", ");
        }
        Serial.println();
    }
};


class SoundFFT sound;
//class RenderOcto renderer(IMAGE_SIZE);
class RenderDMX renderer(SERIAL_PORT_DMX);
class RenderDebug rendererDebug;



void setup_()
{
//    // Power for microphone
//    pinMode(22, OUTPUT);
//    digitalWrite(22, LOW);
//    pinMode(23, OUTPUT);
//    digitalWrite(23, HIGH);

    SERIAL_PORT_MONITOR.begin(250000);
    renderer.begin();
    sound.begin();
    
    uint8_t buffer[3*IMAGE_SIZE];
    if (1)
    for (size_t rep = 0; rep < 2; rep++)
    {
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            buffer[i*3+0] = 0x3f;
            buffer[i*3+1] = 0x00;
            buffer[i*3+2] = 0x00;
        }
        renderer.write(buffer,3*IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            buffer[i*3+0] = 0x00;
            buffer[i*3+1] = 0x3f;
            buffer[i*3+2] = 0x00;
        }
        renderer.write(buffer,3*IMAGE_SIZE);
        delay(1000);
        for (size_t i=0; i < IMAGE_SIZE; i++)
        {
            buffer[i*3+0] = 0x00;
            buffer[i*3+1] = 0x00;
            buffer[i*3+2] = 0x3f;
        }
        renderer.write(buffer,3*IMAGE_SIZE);
        delay(1000);
    }
};


unsigned long int frame = 0;


void loop_1() {
  float n;
  int i;
    Spectrum spectrum;

  if (sound.next(spectrum)) {
    // each time new FFT data is available
    // print it all to the Arduino Serial Monitor

    if (0)
    {
    Serial.print("FFT: ");
    for (i=0; i<40; i++) {
      n = fft.read(i);
      if (n >= 0.01) {
        Serial.print(n);
        Serial.print(" ");
      } else {
        Serial.print("  -  "); // don't print "0.00"
      }
    }
    Serial.println();
  }
  }
}

void loop_()
{
    //Serial.println("loop");
    //P(frame); LOG_println();
    // read sound sensor and delay
    // if we're using 1024 samples, that's about 43FPS, so shouldn't need to wait
    Spectrum spectrum;
    if (sound.next(spectrum))
    {
      frame++;
      uint8_t buffer[IMAGE_SIZE*3];
      renderFrame(millis() / 1000.0, &spectrum, buffer, sizeof(buffer));
      renderer.write(buffer, sizeof(buffer));
      //rendererDebug.write(buffer, sizeof(buffer));
    }
}
