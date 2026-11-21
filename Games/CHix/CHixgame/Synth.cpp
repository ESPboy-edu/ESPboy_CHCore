// CHix's sound (Synth.h): a small software synth mixed in the sample interrupt and sent to the piezo as PWM.
#include "Synth.h"
#include "Music.h"

#ifdef ESP8266
#include <Arduino.h>
#include <sigma_delta.h>
#define SYN_DEVICE 1
#else
#define SYN_DEVICE 0
#define IRAM_ATTR
#endif

namespace {

#ifndef SYN_GAIN
#define SYN_GAIN 8                                     // 1.375 x: louder, with the soft knee squashing the peaks
#endif
constexpr uint16_t ENV_DIV = SYN_RATE / 122;            // samples between envelope ticks (about 8 ms)

struct Voice { uint16_t ph, mph, inc, minc; uint8_t idx, eff, vol; bool gate; };
Voice v[3];

struct Drum { uint16_t ph, inc, lfsr; uint8_t vol, kind, decay; };
Drum dr = { 0, 0, 0xACE1, 0, 0, 0 };
volatile uint8_t kickFlash = 0;                          

// Структура SFX хранится в RAM для предотвращения обращений к PROGMEM из ISR
struct Sfx { 
    audio::Step st[10]; 
    uint8_t n, i, ticks, prio, amp; 
    bool soft; 
    uint16_t inc, ph; 
    int16_t dinc; 
};
Sfx sf = {};

// Структура для кэширования трека целиком
struct RamSong {
    uint8_t lead[256];
    uint8_t bass[256];
    uint8_t drums[256];
    Inst inst[3];
    uint8_t stepMs, sweep, delay, level;
};

// --- RAM БУФЕРЫ ДЛЯ БЕЗОПАСНОСТИ ESP8266 ---
// Хранение всех аудио-данных в RAM спасает от перезагрузок чипа (Exception),
// когда прерывание таймера совпадает с работой SPI экрана.
static RamSong ramSongs[9];
static int8_t ramSINE[256];
static uint16_t ramNOTE_INC16[12];
static RamSong *curRam = nullptr;
// -------------------------------------------

uint8_t sweepNow = 0;
int8_t curId = -1, pendingId = -2;                       
uint8_t fade = 255;                                      
uint8_t level = 255, mlvl = 255;                         
uint8_t pass = 0;                                        
int8_t fadeDir = 0;                                      
uint8_t pos = 0;                                        
uint16_t stepCnt = 0, stepLen = 0, envCnt = 0;


IRAM_ATTR inline uint16_t noteInc(uint8_t n) {
    const int o = n / 12 - 5;                           
    const uint32_t b = ramNOTE_INC16[n % 12];
    return (uint16_t)(o >= 0 ? (b << o) >> 4 : b >> (4 - o));
}

IRAM_ATTR void noteOn(int i, uint8_t n) {
    Voice &c = v[i];
    const Inst &in = curRam->inst[i];
    c.inc = noteInc(n);
    c.minc = (uint16_t)(c.inc * in.ratio);
    c.mph = 0;
    c.idx = in.idx0;
    c.eff = in.idx0;
    c.vol = in.amp;
    c.gate = true;
}

IRAM_ATTR void drumOn(uint8_t bits) {
    if (bits & 1) { kickFlash = 3; dr.kind = 0; dr.inc = 1310; dr.vol = 175; dr.decay = 14; }
    else if (bits & 2) { dr.kind = 1; dr.inc = 1556; dr.vol = 150; dr.decay = 18; }
    else if (bits & 8) { dr.kind = 2; dr.vol = 80; dr.decay = 7; }
    else if (bits & 4) { dr.kind = 2; dr.vol = 84; dr.decay = 26; }
}

// ДОБАВЛЕН КРИТИЧЕСКИ ВАЖНЫЙ IRAM_ATTR
IRAM_ATTR void startSong(int song) {
    for (int i = 0; i < 3; i++) { v[i].vol = 0; v[i].gate = false; }
    dr.vol = 0; kickFlash = 0; sweepNow = 0;
    if (song < 0 || song >= 9) { curRam = nullptr; curId = -1; return; }
    
    curRam = &ramSongs[song];
    curId = (int8_t)song;
    
    stepLen = (uint16_t)(curRam->stepMs * SYN_RATE / 1000);
    stepCnt = 1;
    pos = 0;
    pass = 0;
    level = curRam->level;
}

IRAM_ATTR void nextStep() {
    const uint8_t *s[2] = { curRam->lead, curRam->bass };
    for (int i = 0; i < 3; i++) {
        const uint8_t b = i == 2 ? 
            curRam->lead[(uint8_t)(pos - curRam->delay - pass)] : 
            s[i][pos];
            
        if (b == 0) v[i].gate = false;
        else if (b > 1) noteOn(i, b);
    }
    
    uint8_t d = curRam->drums[pos];
    if (d) drumOn(d);
    
    const uint8_t t = pos & 63, tri = t < 32 ? t : 63 - t;
    sweepNow = (uint8_t)(((pass ? 32 - tri : tri) * curRam->sweep) >> 5);
    if (++pos == 0) pass ^= 2;
}

IRAM_ATTR void sfxStep() {
    if (sf.i >= sf.n) { sf.n = 0; sf.amp = 0; sf.prio = 0; return; }
    
    audio::Step &s = sf.st[sf.i];
    
    const uint16_t inc = (uint16_t)(s.hz * 164u);
    sf.ticks = (uint8_t)((s.ms * 62u >> 8) + 1);
    sf.dinc = s.endHz ? (int16_t)(((int)s.endHz * 164 - (int)inc) / sf.ticks) : 0;
    sf.inc = inc;
    sf.amp = s.hz ? (sf.soft ? 44 : 104) : 0;
}

IRAM_ATTR void envTick() {
    if (fadeDir < 0) {
        if (fade > 4) fade -= 4;
        else {
            fade = 0;
            if (pendingId != -2) { startSong(pendingId); pendingId = -2; fadeDir = curRam ? 1 : 0; if (curRam) fade = 96; } else fadeDir = 0;
        }
    } else if (fadeDir > 0) {
        if (fade < 247) fade += 8; else { fade = 255; fadeDir = 0; }
    }
    mlvl = (uint8_t)((fade * level) >> 8);
    for (int i = 0; i < 3; i++) {
        Voice &c = v[i];
        if (!c.vol) continue;
        const Inst &in = curRam->inst[i];
        if (c.idx > in.idxFloor + in.idxDecay) c.idx -= in.idxDecay; else c.idx = in.idxFloor;
        c.eff = (i == 1) ? c.idx : (uint8_t)(c.idx + sweepNow > 255 ? 255 : c.idx + sweepNow);
        if (c.gate) {
            if (c.vol > in.volFloor + in.volDecay) c.vol -= in.volDecay; else c.vol = in.volFloor;
        } else {
            c.vol = c.vol > 14 ? c.vol - 14 : 0;
        }
    }
    if (dr.vol) {
        if (dr.kind == 0 && dr.inc > 360) dr.inc -= dr.inc >> 2;
        dr.vol = dr.vol > dr.decay ? dr.vol - dr.decay : 0;
    }
    if (sf.n) {
        sf.inc += sf.dinc;
        if (--sf.ticks == 0) { sf.i++; sfxStep(); }
    }
}

}  // namespace

namespace synth {

IRAM_ATTR uint8_t sample() {
    if (++envCnt >= ENV_DIV) { envCnt = 0; envTick(); }
    if (curRam && --stepCnt == 0) { stepCnt = stepLen; nextStep(); }
    int mix = 0;
    for (int i = 0; i < 3; i++) {
        Voice &c = v[i];
        if (!c.vol) continue;
        c.ph += c.inc;
        c.mph += c.minc;
        
        const int m = ((int8_t)ramSINE[c.mph >> 8] * c.eff) >> 5;
        mix += ((int8_t)ramSINE[(uint8_t)((c.ph >> 8) + m)] * c.vol) >> 7;
    }
    if (dr.vol) {
        dr.ph += dr.inc;
        if (dr.kind) {
            uint16_t x = dr.lfsr;
            x ^= x << 7; x ^= x >> 9; x ^= x << 8;
            dr.lfsr = x;
            const int nz = (int)(x >> 8) - 128;
            mix += (nz * dr.vol) >> 7;
            if (dr.kind == 1) mix += ((int8_t)ramSINE[dr.ph >> 8] * dr.vol) >> 7;
        } else {
            mix += ((int8_t)ramSINE[dr.ph >> 8] * dr.vol) >> 8;
        }
    }
    mix = (mix * mlvl) >> 8;
    if (sf.amp) {
        sf.ph += sf.inc;
        mix += (sf.ph & 0x8000) ? sf.amp : -sf.amp;
    }
    mix = (mix * SYN_GAIN) >> 4;
    if (mix > 96) mix = 96 + ((mix - 96) >> 2); else if (mix < -96) mix = -96 + ((mix + 96) >> 2);
    return (uint8_t)(128 + (mix > 127 ? 127 : mix < -128 ? -128 : mix));
}

#if defined(ESP8266)
#define SYN_LOCK() noInterrupts()
#define SYN_UNLOCK() interrupts()
#else
#define SYN_LOCK()
#define SYN_UNLOCK()
#endif

void music(int song) {
    const int id = (song < 0 || song >= 9) ? -1 : song;
    SYN_LOCK();
    if (!curRam) {
        if (id >= 0) { startSong(id); fade = 0; fadeDir = 1; }
        pendingId = -2;
    } else if (id == curId) {
        pendingId = -2;
        if (fadeDir < 0) fadeDir = 1;
    } else {
        pendingId = (int8_t)id;
        fadeDir = -1;
    }
    SYN_UNLOCK();
}

uint8_t beat() {
    const uint8_t k = kickFlash;
    if (k) kickFlash = k - 1;
    return k;
}

void sfx(const audio::Effect &e) {
    // Входящая структура "e" лежит во Flash-памяти. Копируем ее безопасно.
    audio::Effect ram_e;
    memcpy_P(&ram_e, &e, sizeof(audio::Effect));

    uint8_t n = ram_e.n;
    if (n > 10) n = 10;
    
    audio::Step steps_copy[10];
    memcpy_P(steps_copy, ram_e.steps, n * sizeof(audio::Step));
    
    SYN_LOCK();
    const uint8_t prio = ram_e.flags & 15;
    if (!sf.n || prio >= sf.prio) {
        memcpy(sf.st, steps_copy, n * sizeof(audio::Step));
        sf.n = n; sf.i = 0; sf.prio = prio; sf.soft = (ram_e.flags & audio::SOFT) != 0;
        sf.ph = 0;
        sfxStep();
    }
    SYN_UNLOCK();
}

}  // namespace synth


#if defined(ESP8266)

// ESPboy выводит звук на D3 (GPIO0)
#ifndef SOUND_PIN
#define SOUND_PIN D3
#endif

void IRAM_ATTR synthTimerISR() {
    uint8_t sample = synth::sample();
    sigmaDeltaWrite(0, sample); 
}

void synth::begin() {
    // 1. Копируем все массивы музыки из PROGMEM в оперативку
    memcpy_P(ramSINE, SINE, 256);
    memcpy_P(ramNOTE_INC16, NOTE_INC16, 24);
    
    for(int i = 0; i < 9; i++) {
        SongHdr hdr;
        memcpy_P(&hdr, &SONGS[i], sizeof(SongHdr));
        memcpy_P(ramSongs[i].lead, hdr.lead, 256);
        memcpy_P(ramSongs[i].bass, hdr.bass, 256);
        memcpy_P(ramSongs[i].drums, hdr.drums, 256);
        memcpy_P(ramSongs[i].inst, hdr.inst, sizeof(Inst) * 3);
        ramSongs[i].stepMs = hdr.stepMs;
        ramSongs[i].sweep = hdr.sweep;
        ramSongs[i].delay = hdr.delay;
        ramSongs[i].level = hdr.level;
    }

    // 2. Отключаем любой возможный системный ШИМ на пине, чтобы не было конфликта
    pinMode(SOUND_PIN, OUTPUT);
    digitalWrite(SOUND_PIN, LOW);

    // 3. Запускаем аппаратный SigmaDelta (канал 0, частота 31250 Гц)
    sigmaDeltaSetup(0, 31250);
    sigmaDeltaAttachPin(SOUND_PIN, 0);
    sigmaDeltaEnable(); 

    // 4. Глушим таймер (ЕСЛИ он был запущен Arduino Core) и стартуем для звука
    timer1_disable();
    timer1_attachInterrupt(synthTimerISR);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); 
    timer1_write(625); // 80 MHz / 16 = 5 MHz. 5000000 / 8000 Hz = 625
}

#else
void synth::begin() {}
#endif