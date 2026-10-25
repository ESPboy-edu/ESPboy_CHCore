#include "PicoSynth.h"
#include <sigma_delta.h>
#include <math.h>

namespace pico_synth {

static const uint16_t SAMPLE_RATE = 16000;
static const uint16_t BUFFER_SIZE = 2048; 
static volatile uint8_t ringBuffer[BUFFER_SIZE];
static volatile uint16_t head = 0;
static volatile uint16_t tail = 0;

static uint16_t NOTE_FREQ[64];

struct Channel {
    int8_t sfxId;
    int8_t noteIdx;
    uint32_t tickTimer;
    uint32_t speedSamples;
    
    uint16_t phase;
    uint16_t phaseStep;
    uint8_t wave;
    uint8_t vol;
    uint8_t fx;
    
    uint32_t rng;
    int8_t noiseVal; // Хранит текущее значение тонального шума
    bool isMusic;
};

static Channel channels[4];
static int currentPattern = -1;
static bool musicPlaying = false;

static const P8Sfx* rom_sfx = nullptr;
static const P8Pattern* rom_music = nullptr;

void IRAM_ATTR onAudioTimer() {
    if (tail != head) {
        uint8_t sample = ringBuffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
        sigmaDeltaWrite(0, sample);
    }
}

static inline int8_t renderWave(uint8_t wave, uint16_t phase, uint32_t &rng, int8_t noiseVal) {
    switch (wave) {
        case 0: // Синус
            return (int8_t)(sinf(phase * (2.0f * M_PI / 65536.0f)) * 63.0f);
        case 1: // Треугольник
            if (phase < 16384) return (int8_t)((phase * 4) >> 10);
            if (phase < 49152) return (int8_t)(63 - (((phase - 16384) * 4) >> 10));
            return (int8_t)(-63 + (((phase - 49152) * 4) >> 10));
        case 2: // Пила
            return (int8_t)((phase >> 9) - 64);
        case 3: // Меандр
            return (phase < 32768) ? 63 : -64;
        case 4: // Пульс
            return (phase < 21845) ? 63 : -64;
        case 5: // Орган
            return (int8_t)(((renderWave(1, phase, rng, noiseVal)) + (renderWave(1, (phase * 2) & 0xFFFF, rng, noiseVal))) / 2);
        case 6: // Тональный шум (возвращаем зафиксированное значение)
            return noiseVal>>1;
        case 7: // Phaser
            return (int8_t)(((renderWave(1, phase, rng, noiseVal)) + (renderWave(1, (uint16_t)(phase * 0.96f), rng, noiseVal))) / 2);
        default:
            return 0;
    }
}

void begin(const P8Sfx* sfx_rom, const P8Pattern* music_rom) {
    rom_sfx = sfx_rom;
    rom_music = music_rom;

    for (int i = 0; i < 64; i++) {
        NOTE_FREQ[i] = (uint16_t)(440.0 * pow(2.0, (i - 33) / 12.0) + 0.5);
    }

    pinMode(0, OUTPUT);
    sigmaDeltaSetup(0, 312500);
    sigmaDeltaAttachPin(0);
    sigmaDeltaEnable();
    sigmaDeltaWrite(0, 128);

    timer1_isr_init();
    timer1_attachInterrupt(onAudioTimer);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    timer1_write(313);

    for (int i = 0; i < 4; i++) {
        channels[i].sfxId = -1;
        channels[i].rng = 0x12345678 ^ (i * 0x55555555);
        channels[i].noiseVal = 0;
    }
}

static void triggerNote(int ch, uint8_t sfxId, uint8_t noteIdx) {
    if (!rom_sfx) return;
    Channel &c = channels[ch];
    
    P8Note note;
    memcpy_P(&note, &rom_sfx[sfxId].notes[noteIdx], sizeof(P8Note));

    c.wave = note.wave;
    c.vol = note.vol;
    c.fx = note.fx;

    if (note.vol > 0 && note.pitch < 64) {
        uint32_t freq = NOTE_FREQ[note.pitch];
        c.phaseStep = (uint16_t)((freq * 65536ULL) / SAMPLE_RATE);
    } else {
        c.vol = 0;
    }
}

void sfx(int id, int ch) {
    if (id < 0 || id >= 64 || !rom_sfx) return;
    
    if (ch < 0) {
        for (int i = 3; i >= 0; i--) {
            if (channels[i].sfxId == -1) { ch = i; break; }
        }
        if (ch < 0) {
            for (int i = 3; i >= 0; i--) {
                if (!channels[i].isMusic) { ch = i; break; }
            }
        }
        if (ch < 0) ch = 3;
    }

    Channel &c = channels[ch];
    c.sfxId = id;
    c.noteIdx = 0;
    c.tickTimer = 0;
    c.isMusic = false;

    uint8_t spd;
    memcpy_P(&spd, &rom_sfx[id].speed, 1);
    if (spd == 0) spd = 1;
    c.speedSamples = spd * 133;

    triggerNote(ch, id, 0);
}

static void startPattern(int pat) {
    if (pat < 0 || pat >= 64 || !rom_music) return;
    currentPattern = pat;
    
    P8Pattern p;
    memcpy_P(&p, &rom_music[pat], sizeof(P8Pattern));

    for (int ch = 0; ch < 4; ch++) {
        uint8_t sfxId = p.sfx[ch];
        if (sfxId < 64) {
            Channel &c = channels[ch];
            c.sfxId = sfxId;
            c.noteIdx = 0;
            c.tickTimer = 0;
            c.isMusic = true;
            
            uint8_t spd;
            memcpy_P(&spd, &rom_sfx[sfxId].speed, 1);
            if (spd == 0) spd = 1;
            c.speedSamples = spd * 133;
            
            triggerNote(ch, sfxId, 0);
        } else {
            channels[ch].sfxId = -1;
            channels[ch].vol = 0;
        }
    }
}

void music(int pat, int fadeMs) {
    musicPlaying = true;
    startPattern(pat);
}

void stopMusic() {
    musicPlaying = false;
    currentPattern = -1;
    for (int i = 0; i < 4; i++) {
        if (channels[i].isMusic) {
            channels[i].sfxId = -1;
            channels[i].vol = 0;
        }
    }
}

void pause() {
    timer1_disable();
}

void resume() {
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    timer1_write(313);
}

void update() {
    if (!rom_sfx) return;

    static int boot_protect = 60;
    if (boot_protect > 0) {
        boot_protect--;
        pinMode(0, OUTPUT);
        sigmaDeltaAttachPin(0);
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
        timer1_write(313);
    }
    
    while (((tail - head - 1 + BUFFER_SIZE) % BUFFER_SIZE) > 0) {
        int16_t mix = 0;
        bool patternFinished = false;

        for (int ch = 0; ch < 4; ch++) {
            Channel &c = channels[ch];
            if (c.sfxId < 0) continue;

            if (c.vol > 0) {
                uint16_t oldPhase = c.phase;
                c.phase += c.phaseStep;

                // МАГИЯ PICO-8: Генерируем новое случайное число только 8 раз за полный цикл волны.
                // Это превращает оглушительный белый шум в плотный ретро-бас/снэйр нужной тональности.
                if ((c.phase >> 13) != (oldPhase >> 13) || c.noiseVal == 0) {
                    c.rng ^= c.rng << 13; c.rng ^= c.rng >> 17; c.rng ^= c.rng << 5;
                    c.noiseVal = (int8_t)((c.rng & 0x7F) - 64);
                }

                int8_t raw = renderWave(c.wave, c.phase, c.rng, c.noiseVal);
                mix += (raw * c.vol) / 7;
            }

            c.tickTimer++;
            if (c.tickTimer >= c.speedSamples) {
                c.tickTimer = 0;
                c.noteIdx++;

                uint8_t lend, lstart;
                memcpy_P(&lend, &rom_sfx[c.sfxId].loop_end, 1);
                memcpy_P(&lstart, &rom_sfx[c.sfxId].loop_start, 1);

                if (lend > 0 && c.noteIdx >= lend) {
                    c.noteIdx = lstart;
                }

                if (c.noteIdx >= 32) {
                    if (c.isMusic) patternFinished = true;
                    c.sfxId = -1;
                    c.vol = 0;
                } else {
                    triggerNote(ch, c.sfxId, c.noteIdx);
                }
            }
        }

        if (patternFinished && musicPlaying) {
            P8Pattern p;
            memcpy_P(&p, &rom_music[currentPattern], sizeof(P8Pattern));
            
            if (p.stop) {
                stopMusic();
            } else if (p.loop_end) {
                int nextPat = currentPattern;
                while (nextPat > 0) {
                    P8Pattern prev;
                    memcpy_P(&prev, &rom_music[nextPat], sizeof(P8Pattern));
                    if (prev.loop_begin) break;
                    nextPat--;
                }
                startPattern(nextPat);
            } else {
                startPattern((currentPattern + 1) % 64);
            }
        }

        // Больше не делим на 2! Если звуков много, PICO-8 аппаратно их жестко обрезает (clipping).
        // Это и есть тот самый теплый ламповый дисторшн на громких ударных, при этом тихая мелодия останется громкой.
        if (mix > 127) mix = 127;
        if (mix < -128) mix = -128;
        
        uint16_t next = (head + 1) % BUFFER_SIZE;
        ringBuffer[head] = (uint8_t)(mix + 128);
        head = next;
    }
}

} // namespace pico_synth
