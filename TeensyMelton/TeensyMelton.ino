#include "TeensyMelton.h"
#include "FastLED.h"
#include "Adafruit_NeoPixel.h"

extern void loop_fft();

#ifdef PLATFORM_M5STACK
extern void setup_m5();
extern void loop_m5();

void setup()
{
#ifdef ESP32
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("ESP32");
#endif

  setup_m5();
}
void loop()
{
  loop_m5();
}


#endif

#ifdef PLATFORM_TEENSY
extern void setup_teensy();
extern void loop_teensy();

void setup()
{
  setup_teensy();
}
void loop()
{
  loop_teensy();
}
#endif





#if OUTPUT_MYDMX
class RenderMyDMX : public OutputLED
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

    void write(const CRGB data[], size_t count) override
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

RenderMyDmx _renderMyDmx(Serial1);
RenderMyDmx *output = &_renderMyDmx;
#endif


#if OUTPUT_DEBUG
class RenderDebug : public OutputLED
{
public:
    void begin()
    {
    }

    void write(const CRGB rgb[], size_t count)
    {
        count = count>20 ? 20 : count;
        for (size_t i = 0; i < count; i++)
        {
            CRGB c = rgb[i];
            unsigned long ul = ((unsigned long)c.r << 16) + ((unsigned long)c.g << 8) + (unsigned long)c.b;
            ul |= 0x80000000;
            Serial.print(ul, 16);
            Serial.print(", ");
        }
        Serial.println();
    }

    void loop(){}
};
RenderDebug _outputDebug;
OutputLED *outputDebug = &_outputDebug;
#endif


#if OUTPUT_ADAFRUIT_NEOPIXELS
Adafruit_NeoPixel adafruit_neopixels(IMAGE_SIZE, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

class RenderAdafruitNeoPixel : public OutputLED
{
public:
    RenderAdafruitNeoPixel()
    {}

    void begin() override
    {
        adafruit_neopixels.begin();
    }

    void write(const CRGB data[], size_t count) override
    {
        for (size_t i=0 ; i<count ; i++)
            adafruit_neopixels.setPixelColor(i, data[i].r, data[i].g, data[i].b);
        adafruit_neopixels.show();
    }

    void loop() override
    {}
};
RenderAdafruitNeoPixel _outputAdafruit;
OutputLED *output = &_outputAdafruit;
#endif

class _RenderFastLED : public OutputLED
{
protected:
    CRGB leds[IMAGE_SIZE];

public:
    void write(const CRGB data[], size_t count) override
    {
        memcpy(leds, data, count*sizeof(CRGB));
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
RenderFastLEDDMX _outputFastLED;
OutputLED *output = &_outputFastLED;
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
RenderFastLEDNeoPixel _outputFastLED;
OutputLED *output = &_outputFastLED;
#endif




void mapToDisplay(const float ledData[], CRGB rgbData[], const size_t size)
{
    size_t count = size / 3;
    for (size_t i = 0; i < count; i++)
    {
        float r = ledData[i], g = ledData[i+1], b = ledData[i+2];
        if (VIBRANCE != 0.0)
        {
            float mx = (float) fmax(fmax(r, g), b);
            float avg = (r + g + b) / 3.0f;
            float adjust = (1 - (mx - avg)) * 2 * -1 * VIBRANCE;
            //float adjust = -1 * vibrance;
            // r += (mx - r) * adjust;
            r = r * (1 - adjust) + adjust * mx;
            g = g * (1 - adjust) + adjust * mx;
            b = b * (1 - adjust) + adjust * mx;
        }
        if (GAMMA != 1.0)
        {

//            rgbData[i].r = constrain(roundf(pow(ledData[i * 3 + 0], GAMMA) * maxBrightness * 255), 0.0f, 255.0f);
//            rgbData[i].g = constrain(roundf(pow(ledData[i * 3 + 1], GAMMA) * maxBrightness * 255), 0.0f, 255.0f);
//            rgbData[i].b = constrain(roundf(pow(ledData[i * 3 + 2], GAMMA) * maxBrightness * 255), 0.0f, 255.0f);
            // Adafruit_Neopixel::gamma8 corresponds to GAMMA=2.6
            rgbData[i].r = Adafruit_NeoPixel::gamma8(constrain((int)roundf(r * 255), 0, 255)) * MAX_BRIGHTNESS;
            rgbData[i].g = Adafruit_NeoPixel::gamma8(constrain((int)roundf(g * 255), 0, 255)) * MAX_BRIGHTNESS;
            rgbData[i].b = Adafruit_NeoPixel::gamma8(constrain((int)roundf(b * 255), 0, 255)) * MAX_BRIGHTNESS;
        }
        else
        {
            size_t count = size / 3;
            const float scale = MAX_BRIGHTNESS * 255;
            for (size_t i = 0; i < count; i++)
            {
                rgbData[i].r = constrain((int) roundf(r * scale), 0, 255);
                rgbData[i].g = constrain((int) roundf(g * scale), 0, 255);
                rgbData[i].b = constrain((int) roundf(b * scale), 0, 255);
            }
        }
    }
}



#if DEBUG_LOG

void _print(const char *s)
{
    Serial.print(s);
}
void _println(const char *s)
{
    Serial.println(s);
}
void _print(float f)
{
    Serial.print(f);
}
void _println(float f)
{
    Serial.println(f);
}

#endif

void _format(int n)
{
    if (n < 10)
        _print("    ");
    else if (n < 100)
        _print("   ");
    else if (n < 1000)
        _print("  ");
    else
        _print(" ");
    _print(n);
}





#if 0
#include "DMXSerial.h"
#include "DmxSimple.h"
#include "FastLED.h"
#define IMAGE_SIZE 50
#define NEOPIXEL_PIN 2
#define DMX_TX_PIN 1



class RenderMyDMX
{
//    // extra buffer for full dmx frame
//    static uint8_t tx_buffer[512*3];

    HardwareSerial &serial;

public:
    RenderMyDMX(HardwareSerial &serial_) :
            serial(serial_)
    {
#ifdef TEENSY40
        //        serial.addStorageForWrite(RenderDMX::tx_buffer, sizeof(RenderDMX::tx_buffer));
#endif
    }

    void begin()
    {
    }

    void write(CRGB data[], size_t count)
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

        for (size_t i=count*3 ; i<512 ; i++)
            serial.write((uint8_t)0);
    }

    void loop()
    {}
};


class RenderFastLED
{
    CRGB leds[IMAGE_SIZE];

public:
    void begin() 
    {
      //FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(leds, IMAGE_SIZE);
      //FastLED.addLeds<DMXSIMPLE,DMX_TX_PIN,RGB>(leds, IMAGE_SIZE);
    }

    void write(CRGB data[], size_t count)
    {
        for (size_t i=0 ; i<count/2 ; i++)
        {
           leds[IMAGE_SIZE-1-i] = data[i*2];
           leds[i] = data[i*2+1];
        }
        FastLED.show();
    }

    void loop() 
    {}
};

RenderFastLED render;
//RenderMyDMX render(Serial1);

void setup_2()
{
    Serial.begin(250000);
    while( !Serial );

    pinMode(22, OUTPUT);
    digitalWrite(2, LOW);
    pinMode(23, OUTPUT);
    digitalWrite(23, HIGH);

    Serial.begin(250000);
    pinMode(A5,INPUT);

    render.begin();

    pinMode(13,OUTPUT);
}

int blink = 0;
void loop_2()
{
    Serial.println(".");
    CRGB rgb[IMAGE_SIZE];
    float t = millis()/500.0;
    
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
      rgb[i].r = 63 * sin(t*1.1 + i*(2*3.1415/IMAGE_SIZE));
      rgb[i].g = 63 * sin(t*1.5 + i*(2*3.1415/IMAGE_SIZE));
      rgb[i].b = 63 * sin(t*2.1 + i*(2*3.1415/IMAGE_SIZE));
    }

    Serial.println("+");
    Serial.flush();
    render.write(rgb,IMAGE_SIZE);
    Serial.println("-");
    Serial.flush();

//    Serial.println(analogRead(A5));   // causes hang???
//    Serial.flush();
    delay(25);

    digitalWrite(13,(blink = !blink));
    Serial.println("blink");Serial.flush();
}
#endif
