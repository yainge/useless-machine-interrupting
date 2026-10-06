#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
const int PIN_MIC = A2;  // Grove sound sensor (analog out)
const int PIN_LED = 4;   // LED (D4)
const int PIN_DF_RX = 2; // Arduino RX ← DFPlayer TX
const int PIN_DF_TX = 3; // Arduino TX → DFPlayer RX

// ── Tune these in the Fab Lab ─────────────────────────────────────────────────
const int THRESHOLD = 100;
// Peak-to-peak level that counts as talking (0–1023).
// Watch "level:" in serial monitor while quiet – background is usually 5–25.
// Raise if room noise triggers it; lower if it misses speech.

const unsigned long TRIGGER_HOLD_MS = 50;
// Sound must stay above THRESHOLD for this many ms before reacting.
// Raise to ignore claps/door slams; lower if speech is slow to trigger.

const unsigned long RELEASE_HOLD_MS = 600;
// Must be quiet for this many ms before stopping.
// Raise if it cuts out between words; lower if it lingers too long.

// ── DFPlayer ──────────────────────────────────────────────────────────────────
const int DF_TRACK_COUNT = 6; // number of .mp3 files in /mp3/ on the SD card
// Volume is controlled by the potentiometer on A0 (fully clockwise = max)

// ── Measurement ───────────────────────────────────────────────────────────────
const unsigned int SAMPLE_WIN_MS = 30; // peak-to-peak window (ms)

// ── Feedback gap: DFPlayer (timed, no BUSY pin) ───────────────────────────────
// Play → stop → settle → sample mic → re-trigger if still loud.
// Gap ≈ DF_SETTLE_MS + SAMPLE_WIN_MS ≈ 80 ms (short stutter per cycle).
const unsigned long DF_BURST_MS = 3000; // play time before each mic check (ms) – lower = stops sooner, more stutter; raise = smoother, slower to stop

// ── Debug (Serial Plotter compatible) ─────────────────────────────────────────
const bool DEBUG = true; // set false to silence serial output

// ─────────────────────────────────────────────────────────────────────────────

SoftwareSerial dfSerial(PIN_DF_RX, PIN_DF_TX);
DFRobotDFPlayerMini dfPlayer;
bool dfAvailable = false;

enum State : uint8_t
{
    IDLE,
    ARMED,
    REACTING
};
State state = IDLE;

unsigned long armedStart = 0;
unsigned long quietStart = 0;
unsigned long dfPlayStart = 0;
bool dfPlaying = false;
int lastVol = 0;

// ─────────────────────────────────────────────────────────────────────────────

int peakToPeak()
{
    unsigned long t = millis();
    int hi = 0, lo = 1023;
    while (millis() - t < SAMPLE_WIN_MS)
    {
        int v = analogRead(PIN_MIC);
        if (v > hi)
            hi = v;
        if (v < lo)
            lo = v;
    }
    return hi - lo;
}

void playRandom()
{
    lastVol = map(analogRead(A0), 0, 1023, 0, 30);
    dfPlayer.volume(lastVol);
    int track = (int)random(1, DF_TRACK_COUNT + 1);
    if (DEBUG)
    {
        Serial.print(F("track:"));
        Serial.println(track);
    }
    dfPlayer.playMp3Folder(track);
}

void dbg(int level, int st)
{
    if (!DEBUG)
        return;
    Serial.print(F("level:"));
    Serial.print(level);
    Serial.print(F(" threshold:"));
    Serial.print(THRESHOLD);
    Serial.print(F(" vol:"));
    Serial.print(lastVol);
    Serial.print(F(" state:"));
    Serial.println(st);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup()
{
    pinMode(PIN_LED, OUTPUT);
    Serial.begin(9600);
    randomSeed(analogRead(A2));

    dfSerial.begin(9600);
    delay(1000);
    if (dfPlayer.begin(dfSerial))
    {
        lastVol = map(analogRead(A0), 0, 1023, 0, 30);
        dfPlayer.volume(lastVol);
        dfAvailable = true;
        Serial.println(F("DFPlayer OK"));
    }
    else
    {
        Serial.println(F("DFPlayer not found"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void loop()
{
    int level = peakToPeak();

    // ── IDLE ─────────────────────────────────────────────────────────────────
    if (state == IDLE)
    {
        if (level > THRESHOLD)
        {
            state = ARMED;
            armedStart = millis();
        }
        dbg(level, 0);
        return;
    }

    // ── ARMED – confirm sound is sustained, not a clap ────────────────────────
    if (state == ARMED)
    {
        if (level <= THRESHOLD)
        {
            state = IDLE;
            dbg(level, 1);
            return;
        }
        if (millis() - armedStart >= TRIGGER_HOLD_MS)
        {
            state = REACTING;
            quietStart = 0;
            dfPlaying = false;
            Serial.println(F("Triggered"));
        }
        dbg(level, 1);
        return;
    }

    // ── REACTING ─────────────────────────────────────────────────────────────
    if (state == REACTING)
    {
        if (dfAvailable)
        {
            if (dfPlaying && millis() - dfPlayStart < DF_BURST_MS)
            {
                digitalWrite(PIN_LED, HIGH);
                dbg(level, 2);
                return;
            }
            if (dfPlaying)
            {
                dfPlayer.stop();
                digitalWrite(PIN_LED, LOW);
                dfPlaying = false;
                level = peakToPeak();
            }
            if (level > THRESHOLD)
            {
                quietStart = 0;
                playRandom();
                dfPlayStart = millis();
                dfPlaying = true;
                digitalWrite(PIN_LED, HIGH);
            }
            else
            {
                if (quietStart == 0)
                    quietStart = millis();
                if (millis() - quietStart >= RELEASE_HOLD_MS)
                {
                    dfPlayer.stop();
                    digitalWrite(PIN_LED, LOW);
                    state = IDLE;
                    Serial.println(F("Released"));
                }
            }
        }
        dbg(level, 2);
        return;
    }
}
