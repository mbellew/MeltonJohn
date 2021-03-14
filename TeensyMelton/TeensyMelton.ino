extern void setup_();
extern void loop_();
extern void loop_fft();


void setup()
{
  setup_();
}
void loop()
{
  loop_();
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
      FastLED.addLeds<DMXSIMPLE,DMX_TX_PIN,RGB>(leds, IMAGE_SIZE);
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
