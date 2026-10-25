#include "Synth.h"
#include "src/atm/ATMlib.h"
#include "src/atm/ATMlibimpl.h"
#include "src/Songs.h"

#if defined(__riscv) && !defined(CHSIM)
#include <Arduino.h>
#include <chgame/RamFunc.h>
#define CHIP_DEVICE 1
#elif defined(ESP8266)
#include <Arduino.h>
#include <sigma_delta.h>
#define CHIP_DEVICE 0
#define RAMFUNC(name) ICACHE_RAM_ATTR
// Audio pin for ESPboy (D3)
#define SOUNDPIN D3
#else
#define CHIP_DEVICE 0
#define RAMFUNC(name)
#endif

namespace {
ATMsynth tracker;
volatile bool ready = false;
volatile unsigned gain = 3, muted = 0;
uint16_t tickFraction = 0;
uint16_t ticksPerMinute = 0;
volatile uint32_t tickCounter = 0;
volatile int8_t scope[128];
unsigned scopeIndex = 0, scopeDivider = 0;

// Double-buffer the controls: the sample ISR never sees a partly updated tick.
struct Mix { uint16_t inc[3]; uint8_t vol[4]; };
Mix controls[2] = {};
volatile unsigned front = 0;

void publish() {
    const unsigned next = front ^ 1;
    for (unsigned i = 0; i < 4; ++i) {
        controls[next].vol[i] = (muted & (1u << i)) ? 0 : osc[i].vol;
        if (i < 3) controls[next].inc[i] = osc[i].freq;
    }
    asm volatile("" ::: "memory");
    front = next;
}
} // end anonymous namespace

#if defined(ESP8266)
// ESP8266 specific ISR for generating audio and tracking tempo
extern uint16_t cia_count;
extern uint16_t cia;
extern uint8_t tickRate; // Вынесли в глобальную область!

RAMFUNC(chipIRQ) void sound_speaker_ISR() {
    if (!ready) return;

    // We reuse the generic chip::sample() mixer for ESP8266 to support the scope and CHGame voices
    uint8_t sample_val = chip::sample();
    
    // Fast hardware register write using Sigma-Delta
    sigmaDeltaWrite(0, sample_val);

    if (chip::paused()) return;

    if (--cia_count) return;
    cia_count = cia;
    
    ATM_playroutine();
    ++tickCounter;
    publish();
}
#endif

namespace chip {
void play(unsigned track) {
    if (track >= SONG_COUNT) return;
    ready = false;
#if CHIP_DEVICE || defined(ESP8266)
    noInterrupts();
#endif
    tracker.play(SONGS[track].data);
    memset(osc, 0, sizeof osc);
    osc[3].freq = 1;
    tickFraction = 0;
    ticksPerMinute = SONGS[track].bpm * 24;
    tickCounter = 0;
    
#if defined(ESP8266)
    // ESPboy tempo initialization
    tickRate = 25;
    cia = 15625 / tickRate;
    cia_count = 1;
#endif

    ATM_playroutine();
    publish();
    ready = true;
#if CHIP_DEVICE || defined(ESP8266)
    interrupts();
#endif
}

void pause(bool p) {
#if CHIP_DEVICE || defined(ESP8266)
    noInterrupts();
#endif
    if (p) tracker.pause(); else tracker.resume();
    publish();
#if CHIP_DEVICE || defined(ESP8266)
    interrupts();
#endif
}

bool paused() { return atmScorePaused != 0; }
void setVolume(unsigned v) { gain = v > 4 ? 4 : v; }
unsigned volume() { return gain; }

void mute(unsigned ch, bool value) {
    if (ch >= 4) return;
#if CHIP_DEVICE || defined(ESP8266)
    noInterrupts();
#endif
    if (value) muted |= 1u << ch; else muted &= ~(1u << ch);
    publish();
#if CHIP_DEVICE || defined(ESP8266)
    interrupts();
#endif
}

unsigned muteMask() { return muted; }

void millisecond() {
#if CHIP_DEVICE
    if (!ready || paused() || !TIMSK4) return;
    // Exact score tempo, including BPMs whose ticks/s are fractional.
    tickFraction += ticksPerMinute;
    if (tickFraction >= 60000) {
        tickFraction -= 60000;
        ATM_playroutine();
        ++tickCounter;
        publish();
    }
#endif
}

RAMFUNC(chipSample) uint8_t sample() {
    if (!ready) return 128;
    const Mix &m = controls[front];
    int mix = 0;
    for (unsigned i = 0; i < 3; ++i) {
        osc[i].phase += m.inc[i];
        unsigned phase = osc[i].phase >> 8;
        const int v = m.inc[i] ? m.vol[i] : 0;
        if (i == 0) mix += phase >= 192 ? -v : v; // ATMlib 25% pulse
        else if (i == 1) mix += phase & 128 ? -v : v;
        else { // NES-like triangle bass in place of ATMlib's default saw.
            const int triangle = phase < 128 ? int(phase) * 2 - 127 : 383 - int(phase) * 2;
            mix += triangle * v / 128;
        }
    }
    uint16_t noise = uint16_t(osc[3].freq << 1);
    noise ^= ((noise >> 15) ^ (noise >> 14)) & 1;
    osc[3].freq = noise;
    mix += noise & 0x8000 ? -int(m.vol[3]) : int(m.vol[3]);
    mix = mix * int(gain) / 4;
    if (++scopeDivider == 8) {
        scopeDivider = 0;
        scope[scopeIndex++ & 127] = int8_t(mix);
    }
    return uint8_t(128 + mix); // Authored voice levels sum to less than 128.
}

Voice voice(unsigned ch) {
    if (ch >= 4) return {};
    return {osc[ch].freq, controls[front].vol[ch], channel[ch].note};
}
uint32_t ticks() { return tickCounter; }

void snapshot(int8_t *out) {
    unsigned start = scopeIndex;
    for (unsigned i = 0; i < 128; ++i) out[i] = scope[(start + i) & 127];
}
} // end namespace chip

#if CHIP_DEVICE
extern "C" volatile uint32_t CFGHR_tmpB;
extern "C" void osSystickHandler(void) { chip::millisecond(); }
extern "C" void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" RAMFUNC(chipIRQ) void TIM3_IRQHandler(void) {
    TIM3->INTFR = ~1u;
    TIM1->CH2CVR = chip::sample();
}
#endif

void chip::begin() {
    ready = false;
#if CHIP_DEVICE
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1;
    RCC->APB1PCENR |= RCC_APB1Periph_TIM3;
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(7u << 15)) | (1u << 15);
    CFGHR_tmpB = (CFGHR_tmpB & ~(15u << 8)) | (11u << 8);
    GPIOB->CFGHR = CFGHR_tmpB;
    TIM1->CTLR1 = 0; TIM1->DMAINTENR = 0; TIM1->CCER = 0;
    TIM1->PSC = 0; TIM1->ATRLR = 255; TIM1->CNT = 0;
    TIM1->CHCTLR1 = 0x6800; TIM1->CH2CVR = 128;
    TIM1->RPTCR = 0; TIM1->BDTR = 0x8000; TIM1->CCER = 0x10;
    TIM1->SWEVGR = 1; TIM1->CTLR1 = 0x81; // 187.5 kHz PWM carrier
    TIM3->CTLR1 = 0; TIM3->DMAINTENR = 0;
    TIM3->PSC = 0; TIM3->ATRLR = 1535; TIM3->CNT = 0;
    TIM3->SWEVGR = 1; TIM3->INTFR = 0; TIM3->DMAINTENR = 1;
    NVIC_SetPriority(TIM3_IRQn, 0);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CTLR1 = 1; // 48 MHz / 1536 = exactly 31,250 samples/s
#elif defined(ESP8266)
    noInterrupts();
    // Setup hardware Sigma-Delta PWM for audio output (channel 0)
    sigmaDeltaSetup(0, 65000);
    sigmaDeltaAttachPin(SOUNDPIN);
    sigmaDeltaEnable();

    // Attach hardware timer1 to our custom ISR
    timer1_attachInterrupt(sound_speaker_ISR);
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
    // 80MHz base clock / SAMPLE_RATE (31250 from Synth.h)
    timer1_write(80000000 / chip::SAMPLE_RATE);
    interrupts();
#endif
    play(0);
}
