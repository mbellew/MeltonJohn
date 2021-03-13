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











#if 1

#include "FastLED.h"
#define IMAGE_SIZE 50
#define NEOPIXEL_PIN 2

class RenderFastLED
{
    CRGB leds[IMAGE_SIZE];

public:
    void begin() 
    {
      FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(leds, IMAGE_SIZE);
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
