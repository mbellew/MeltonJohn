#include <DaisyDuino.h>

DaisyHardware hw;

void setup() {
    hw = DAISY.init(DAISY_PATCH, AUDIO_SR_48K);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(9600);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("on");
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("off");
    delay(500);
}
