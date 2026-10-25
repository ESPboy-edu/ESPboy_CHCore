#pragma GCC optimize("Os", "no-ipa-sra")
#include <Arduino.h>
#include <Ticker.h>
#include "Audio.h"
#include "CHGfx.h"
#include "lib/ESPboyInit.h"

extern ESPboyInit myESPboy;

namespace audio {

static const Effect *table = nullptr;

// --- Sequencer state (shared with the 1 kHz interrupt) ----------------------
static volatile const Step *fxSteps = nullptr;
static volatile uint8_t fxN = 0, fxI = 0, fxPrio = 0;
static volatile uint16_t fxPitch = 256;          // Q8: sfx()'s transposition
static volatile uint16_t fxT = 0;
static Step oneStep;                            // blip() and note()

static const uint8_t *score = nullptr;           // a Playtune score ...
static const Melody *mel = nullptr;             // ... or a melody
static volatile const uint8_t *scorePos = nullptr;
static volatile uint8_t melI = 0;
static volatile uint16_t melT = 0;
static volatile uint16_t scoreWait = 0;
static volatile bool scoreLoops = true;          // honour the score's restart (0xE0)
static volatile uint8_t notes[4];               // MIDI note per channel, 0 = off
static uint8_t arpT = 0, arpCh = 0, leadHold = 0;
static const uint8_t ARP_MS = 6;                // arpeggio: ms per note
static const uint8_t LEAD_HOLD_MS = 20;

static bool started = false;
static volatile bool soft, glide;               // the effect's flags
static uint8_t musicMode = ARPEGGIO;
static uint16_t lastHz = 0;
static uint8_t ledPattern = 0;
static uint16_t ledT = 0;

Ticker audioTicker;

// Play tone using ESPboy library
static void tone(uint16_t hz, bool smooth) {
    (void)smooth; // ESP8266 tone() transitions smoothly by default if called again
    if (hz == lastHz) return;
    lastHz = hz;
    if (!hz) {
        myESPboy.noPlayTone();
        return;
    }
    // We ignore duty cycle (soft) for ESPboy's piezo as it has fixed volume, 
    // but the pitch works perfectly.
    myESPboy.playTone(hz); 
}

// MIDI note -> Hz: the top octave (C8..B8), shifted down.
static uint16_t noteHz(uint8_t n) {
    static const uint16_t TOP[12] = {4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902};
    int sh = 9 - n / 12;
    return sh >= 0 ? (uint16_t)(TOP[n % 12] >> sh) : 0;
}

static void scoreTick() {
    if (!scorePos) return;
    if (scoreWait) { scoreWait--; return; }
    for (int guard = 0; guard < 16; guard++) {
        uint8_t b = *scorePos;
        if (b < 0x80) {
            scoreWait = (uint16_t)((b << 8) | scorePos[1]);
            scorePos += 2;
            if (scoreWait) { scoreWait--; return; }
            continue;
        }
        uint8_t cmd = b & 0xF0, ch = b & 3;
        if (cmd == 0x90) { notes[ch] = scorePos[1]; scorePos += 2; }
        else if (cmd == 0x80) { notes[ch] = 0; scorePos += 1; }
        else if (b == 0xE0 && scoreLoops) { scorePos = score; }
        else { scorePos = nullptr; for (auto &n : notes) n = 0; return; }
    }
}

// A melody plays through channel 0, a note at a time.
static void melodyTick() {
    if (!scorePos) return;
    uint8_t note = mel->notes[melI * 2];
    uint16_t len = (uint16_t)(mel->notes[melI * 2 + 1] * mel->unitMs);
    notes[0] = note && melT + mel->gapMs < len ? note : 0;
    if (++melT >= len) {
        melT = 0;
        if (++melI >= mel->count) {
            melI = 0;
            if (!scoreLoops) { scorePos = nullptr; notes[0] = 0; }
        }
    }
}

// The music note to sound this millisecond.
static uint16_t musicHz() {
    if (musicMode == LEAD) {
        if (notes[0]) { leadHold = LEAD_HOLD_MS; return noteHz(notes[0]); }
        if (leadHold) { leadHold--; return 0; }
        for (uint8_t k = 1; k < 4; k++) if (notes[k]) return noteHz(notes[k]);
        return 0;
    }
    if (++arpT >= ARP_MS || !notes[arpCh]) {
        arpT = 0;
        for (uint8_t k = 0; k < 4; k++) {
            arpCh = (arpCh + 1) & 3;
            if (notes[arpCh]) break;
        }
    }
    return notes[arpCh] ? noteHz(notes[arpCh]) : 0;
}

static uint16_t scorePlayer(bool sound)  { scoreTick();  return sound ? musicHz() : 0; }
static uint16_t melodyPlayer(bool sound) {
    melodyTick();
    return sound && notes[0] ? noteHz(notes[0]) : 0;
}
static uint16_t (*player)(bool sound) = nullptr;

// Ticker-based 1kHz interrupt for ESP8266
static void audioTick() {
    if (!started) return;
    const Step *s = (const Step *)fxSteps;
    uint16_t m = musicMode && player ? player(!s) : 0;
    if (s) {
        const Step &st = s[fxI];
        uint16_t hz = (uint16_t)(st.hz * 20), ms = (uint16_t)(st.ms * 2);
        bool sweep = hz && st.endHz;
        if (sweep) hz = (uint16_t)(hz + ((int32_t)st.endHz * 20 - hz) * fxT / ms);
        hz = (uint16_t)((uint32_t)hz * fxPitch >> 8);             // 256: as written
        bool smooth = glide && sweep && fxT;
        if (++fxT >= ms) {
            fxT = 0;
            if (++fxI >= fxN) { fxSteps = nullptr; fxPrio = 0; }
        }
        tone(hz, smooth);
    } else {
        soft = false;
        tone(m, true);
    }
}

void begin(const Effect *effects, uint8_t count, bool o) {
    table = effects; (void)count;
    setOn(o);
}

void setOn(bool o) {
    if (!o || !table) {
        started = false;
        lastHz = 1; tone(0, false);
        audioTicker.detach();
        return;
    }
    if (!started) {
        audioTicker.attach_ms(1, audioTick); // Start 1kHz timer
    }
    started = true;
}

bool on() { return started; }

static bool busy(uint8_t prio) { return !started || (fxSteps && prio < fxPrio); }

static void start(const Step *st, uint8_t n, uint8_t flags, uint16_t pitch) {
    noInterrupts();
    fxSteps = st; fxN = n; fxI = 0; fxT = 0; fxPrio = flags & 15; fxPitch = pitch;
    soft = (flags & SOFT) != 0; glide = (flags & GLIDE) != 0;
    lastHz = 1;                              // restart the tone
    interrupts();
}

static void play(uint8_t id, uint16_t pitch) {
    if (!started) return;
    const Effect &e = table[id];
    if (!busy(e.flags & 15)) start(e.steps, e.n, e.flags, pitch);
}

void sfx(uint8_t id) { play(id, 256); }

void sfx(uint8_t id, uint8_t n) {
    static const uint16_t SEMI[13] = {256, 271, 287, 304, 323, 342, 362, 384, 406, 431, 456, 483, 512};
    play(id, SEMI[n > 12 ? 12 : n]);
}

static void one(uint16_t hz, uint16_t ms, uint8_t flags, uint8_t over) {
    if (busy(over)) return;
    noInterrupts();
    oneStep.hz = (uint8_t)((hz + 10) / 20); oneStep.endHz = 0; oneStep.ms = (uint8_t)((ms + 1) / 2);
    interrupts();
    start(&oneStep, 1, flags, 256);
}

void blip(uint16_t hz, uint16_t ms, bool s) { one(hz, ms, s ? SOFT : 0, 1); }
void note(uint16_t hz, uint16_t ms, uint8_t priority) { one(hz, ms, priority & 15, priority & 15); }
bool playing() { return fxSteps != nullptr; }

void setMusic(uint8_t mode) {
    musicMode = mode;
    if (!mode) for (auto &x : notes) x = 0;
}

void music(const uint8_t *s, bool loop) {
    noInterrupts();
    score = s; mel = nullptr; scorePos = s; scoreWait = 0; scoreLoops = loop;
    for (auto &x : notes) x = 0;
    leadHold = 0;
    player = scorePlayer;
    interrupts();
}

void melody(const Melody &m, bool loop) {
    noInterrupts();
    score = nullptr; mel = &m; scorePos = m.notes; melI = 0; melT = 0; scoreLoops = loop;
    for (auto &x : notes) x = 0;
    leadHold = 0;
    player = melodyPlayer;
    interrupts();
}

void loopMusic(bool o) { scoreLoops = o; }
bool musicPlaying() { return started && musicMode && scorePos; }

void stopMusic() {
    noInterrupts();
    scorePos = nullptr;
    for (auto &x : notes) x = 0;
    interrupts();
}

void led(Led p) { ledPattern = p; ledT = 0; }

void update() {
    if (!ledPattern) return;
    ledT++;
    bool lit = false;
    switch (ledPattern) {
        case LED_BLINK:  lit = ledT < 12; if (ledT > 12) { ledPattern = 0; myESPboy.myLED.setRGB(0,0,0); } break;
        case LED_TRIPLE: lit = (ledT % 16) < 8; if (ledT > 48) { ledPattern = 0; myESPboy.myLED.setRGB(0,0,0); } break;
        case LED_PARTY:  lit = (ledT % 8) < 4; if (ledT > 240) { ledPattern = 0; myESPboy.myLED.setRGB(0,0,0); } break;
    }
    
    // ESPboy smart LED instead of simple GPIO pin
    if (lit) {
        if (ledPattern == LED_PARTY) {
            myESPboy.myLED.setRGB(random(50, 255), random(50, 255), random(50, 255));
        } else {
            myESPboy.myLED.setRGB(150, 100, 0); // Gold flash
        }
    } else {
        myESPboy.myLED.setRGB(0, 0, 0);
    }
}

}  // namespace audio