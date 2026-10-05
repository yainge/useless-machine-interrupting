#include <Arduino.h>

const int SOUND_PIN = A2;
const int BUZZER_PIN = 5;
const int LED_PIN = 7;
const int SOUND_THRESHOLD = 300;

void setup()
{
    pinMode(SOUND_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(9600);
}

void loop()
{
    int soundLevel = analogRead(SOUND_PIN);

    Serial.println(soundLevel);

    if (soundLevel > SOUND_THRESHOLD)
    {
        digitalWrite(LED_PIN, HIGH);
        tone(BUZZER_PIN, 4000);
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
        noTone(BUZZER_PIN);
    }

    delay(10);
}