#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
const int PIN_MIC    = A2;  // Grove sound sensor (analog out)
const int PIN_BUZZER = 5;   // Buzzer  (D5)
const int PIN_LED    = 4;   // LED     (D4)
const int PIN_DF_RX  = 2;   // Arduino RX ← DFPlayer TX
const int PIN_DF_TX  = 3;   // Arduino TX → DFPlayer RX

// ── Tune these in the Fab Lab ─────────────────────────────────────────────────
const int THRESHOLD = 60;
// Peak-to-peak level that counts as talking (0–1023).
// Open Serial Monitor at 9600 and watch "level:" while quiet – background is
// usually 5–25. Set THRESHOLD above that. Raise if room noise triggers it;
// lower if it misses speech.

const unsigned long TRIGGER_HOLD_MS = 120;
// Sound must stay above THRESHOLD for this many ms before reacting.
// Raise to ignore claps/door slams; lower if speech is slow to trigger.

const unsigned long RELEASE_HOLD_MS = 600;
// Must be quiet for this many ms before stopping.
// Raise if it cuts out between words; lower if it lingers too long.

// ── DFPlayer ──────────────────────────────────────────────────────────────────
const uint8_t DF_VOLUME      = 20;  // speaker volume, 0–30
const int     DF_TRACK_COUNT = 2;   // number of .mp3 files in /mp3/ on the SD card

// ── Measurement ───────────────────────────────────────────────────────────────
const unsigned int SAMPLE_WIN_MS = 30;  // peak-to-peak window (ms); keep ≥ 2 cycles of lowest expected frequency

// ── Feedback gap: DFPlayer (timed, no BUSY pin) ───────────────────────────────
// Play → stop → settle → sample mic → re-trigger if still loud.
// Gap ≈ DF_SETTLE_MS + SAMPLE_WIN_MS = ~80 ms (short audible stutter per cycle).
const unsigned long DF_BURST_MS  = 300;  // play time before each mic check (ms)
const unsigned long DF_SETTLE_MS = 50;   // wait after stop before sampling (ms)

// ── Feedback gap: Buzzer ──────────────────────────────────────────────────────
const unsigned long BUZZ_BURST_MS = 150;   // tone on time (ms)
const unsigned long BUZZ_GAP_MS   = 25;    // silent gap before sampling (ms)
const int           BUZZ_FREQ     = 4000;  // Hz

// ── Debug (Serial Plotter compatible) ─────────────────────────────────────────
const bool DEBUG = true;  // set false to silence serial output

// ─────────────────────────────────────────────────────────────────────────────

SoftwareSerial dfSerial(PIN_DF_RX, PIN_DF_TX);
DFRobotDFPlayerMini dfPlayer;
bool dfAvailable = false;

enum State : uint8_t { IDLE, ARMED, REACTING };
State state = IDLE;

unsigned long armedStart  = 0;
unsigned long quietStart  = 0;
unsigned long dfPlayStart = 0;
bool          dfPlaying   = false;

// ─────────────────────────────────────────────────────────────────────────────

int peakToPeak() {
    unsigned long t = millis();
    int hi = 0, lo = 1023;
    while (millis() - t < SAMPLE_WIN_MS) {
        int v = analogRead(PIN_MIC);
        if (v > hi) hi = v;
        if (v < lo) lo = v;
    }
    return hi - lo;
}

void playRandom() {
    dfPlayer.playMp3Folder((int)random(1, DF_TRACK_COUNT + 1));
}

void dbg(int level, int st) {
    if (!DEBUG) return;
    Serial.print(F("level:"));     Serial.print(level);
    Serial.print(F(" threshold:")); Serial.print(THRESHOLD);
    Serial.print(F(" state:"));    Serial.println(st);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    Serial.begin(9600);
    randomSeed(analogRead(A2));  // mic at startup gives variable noise → track randomness

    dfSerial.begin(9600);
    delay(1000);  // let the DFPlayer module finish booting
    if (dfPlayer.begin(dfSerial)) {
        dfPlayer.volume(DF_VOLUME);
        dfAvailable = true;
        Serial.println(F("DFPlayer OK"));
    } else {
        Serial.println(F("DFPlayer not found - buzzer only"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    int level = peakToPeak();  // 30 ms clean sample (output is off at this point)

    // ── IDLE ─────────────────────────────────────────────────────────────────
    if (state == IDLE) {
        if (level > THRESHOLD) {
            state      = ARMED;
            armedStart = millis();
        }
        dbg(level, 0);
        return;
    }

    // ── ARMED – confirm sound is sustained, not a clap ────────────────────────
    if (state == ARMED) {
        if (level <= THRESHOLD) {
            state = IDLE;  // transient noise, ignore
            dbg(level, 1);
            return;
        }
        if (millis() - armedStart >= TRIGGER_HOLD_MS) {
            state      = REACTING;
            quietStart = 0;
            dfPlaying  = false;
            Serial.println(F("Triggered"));
        }
        dbg(level, 1);
        return;
    }

    // ── REACTING ─────────────────────────────────────────────────────────────
    if (state == REACTING) {

        if (dfAvailable) {
            // ── DFPlayer burst / gap cycle ────────────────────────────────────
            if (dfPlaying && millis() - dfPlayStart < DF_BURST_MS) {
                // Still inside play window – leave output on, skip mic decision
                digitalWrite(PIN_LED, HIGH);
                dbg(level, 2);
                return;
            }
            if (dfPlaying) {
                // Burst elapsed – stop, settle, take a clean mic sample
                dfPlayer.stop();
                digitalWrite(PIN_LED, LOW);
                dfPlaying = false;
                delay(DF_SETTLE_MS);
                level = peakToPeak();
            }
            // Output is off – decide whether to play again or release
            if (level > THRESHOLD) {
                quietStart  = 0;
                playRandom();
                dfPlayStart = millis();
                dfPlaying   = true;
                digitalWrite(PIN_LED, HIGH);
            } else {
                if (quietStart == 0) quietStart = millis();
                if (millis() - quietStart >= RELEASE_HOLD_MS) {
                    dfPlayer.stop();
                    digitalWrite(PIN_LED, LOW);
                    state = IDLE;
                    Serial.println(F("Released"));
                }
            }

        } else {
            // ── Buzzer fallback ───────────────────────────────────────────────
            // level was sampled at the top of the loop while buzzer was off – clean read
            if (level > THRESHOLD) {
                quietStart = 0;
                tone(PIN_BUZZER, BUZZ_FREQ);
                digitalWrite(PIN_LED, HIGH);
                delay(BUZZ_BURST_MS);   // buzz
                noTone(PIN_BUZZER);
                digitalWrite(PIN_LED, LOW);
                delay(BUZZ_GAP_MS);     // settle; next loop's peakToPeak is clean
            } else {
                if (quietStart == 0) quietStart = millis();
                if (millis() - quietStart >= RELEASE_HOLD_MS) {
                    noTone(PIN_BUZZER);
                    state = IDLE;
                    Serial.println(F("Released"));
                }
            }
        }

        dbg(level, 2);
        return;
    }
}
