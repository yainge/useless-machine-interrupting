#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

const int SOUND_PIN = 5;
const int LED_PIN = A0;
const int BUZZER_PIN = 6;
const int MP3_RX_PIN = 3;
const int MP3_TX_PIN = 2;
const bool MP3_ENABLED = false;

SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);
DFRobotDFPlayerMini mp3Player;
bool previousSoundState = false;

void setup()
{
    pinMode(SOUND_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    Serial.begin(9600);

    if (MP3_ENABLED)
    {
        mp3Serial.begin(9600);

        if (mp3Player.begin(mp3Serial))
        {
            mp3Player.volume(20);
            Serial.println("MP3 player ready");
        }
        else
        {
            Serial.println("MP3 player not detected");
        }
    }
}

void loop()
{
    int soundDetected = digitalRead(SOUND_PIN);
    bool soundState = soundDetected == HIGH;

    if (soundState)
    {
        digitalWrite(LED_PIN, HIGH);
        tone(BUZZER_PIN, 1000);

        if (!previousSoundState)
        {
            if (MP3_ENABLED)
            {
                mp3Player.play(1);
            }
        }
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
        noTone(BUZZER_PIN);
    }

    previousSoundState = soundState;
    delay(10);
}