#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Arduino pin wiring:
// Sound sensor DO -> D5
// LED             -> A0
// Buzzer          -> D6
// MP3 board RX    <- D3 (Arduino TX)
// MP3 board TX    -> D2 (Arduino RX)
// Pot middle pin   -> A2 (currently disabled)
const int SOUND_PIN = 5;
// const int POT_PIN = A2;
const unsigned long SOUND_THRESHOLD_MS = 100;
const int LED_PIN = A0;
const int BUZZER_PIN = 6;
const int MP3_RX_PIN = 3;
const int MP3_TX_PIN = 2;
const bool MP3_ENABLED = false;

SoftwareSerial mp3Serial(MP3_RX_PIN, MP3_TX_PIN);
DFRobotDFPlayerMini mp3Player;
bool previousSoundState = false;
unsigned long soundStartedAt = 0;

void setup()
{
    pinMode(SOUND_PIN, INPUT);
    // pinMode(POT_PIN, INPUT);
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
    bool rawSoundState = soundDetected == HIGH;
    bool soundState = false;

    if (rawSoundState)
    {
        if (soundStartedAt == 0)
        {
            soundStartedAt = millis();
        }

        soundState = millis() - soundStartedAt >= SOUND_THRESHOLD_MS;
    }
    else
    {
        soundStartedAt = 0;
    }

    Serial.print("Sound: ");
    Serial.println(soundDetected);

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