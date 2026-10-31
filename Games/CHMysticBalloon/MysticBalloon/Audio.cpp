#include "Audio.h"
#include "Tones.h"
#include "src/atm/ATMlib.h"
#include "src/atm/ATMlibimpl.h"
#include "src/MusicData.h"
#include <Arduino.h>
#include <sigma_delta.h>

#define MYBL_AUDIO_DEVICE 1
#define SOUND_PIN D3

namespace mystic { namespace tones {
    extern volatile uint16_t current_hz;
    extern volatile uint32_t remaining_ms;
}}

namespace {
ATMsynth tracker;
volatile bool on = false;
volatile mystic::audio::Mode selectedMode = mystic::audio::Atm;
volatile bool ready = false, frozen = false;
uint8_t currentSong = 255, previousState = 255, previousLevel = 255, bonusIndex = 0;
volatile uint32_t tickCounter = 0;
#if CHGAME_DEBUG
volatile uint32_t sampleCounter = 0;
#endif
uint32_t clockFraction = 0;
uint16_t phases[3] = {};
struct Mix { uint16_t increment[3]; uint8_t volume[3]; };
Mix controls[2] = {};
volatile uint8_t front = 0;

bool hw_initialized = false;

void ICACHE_RAM_ATTR publish() {
    const unsigned next = front ^ 1;
    for (unsigned i=0; i<3; ++i) {
        controls[next].increment[i] = osc[i].freq;
        controls[next].volume[i] = osc[i].vol;
    }
    asm volatile("" ::: "memory");
    front = next;
}

void play(unsigned n) {
    noInterrupts(); // Мягко ставим прерывания на паузу на пару микросекунд
    ready = false;
    
    tracker.play(n==1 ? mysticTitle : n>=2 && n<7 ? gameTunes[n-2] : silentSong);
    memset(osc,0,sizeof osc);
    memset(phases,0,sizeof phases);
    clockFraction=0; tickCounter=0;
    ATM_playroutine();
    publish();
    currentSong=n;
    ready=true;
    
    interrupts(); // Запускаем прерывания обратно. Ни один тик не потерян!
}

void ICACHE_RAM_ATTR audioISR() {
    if (selectedMode == mystic::audio::Atm) {
        static uint16_t msAcc = 0;
        msAcc += 1000;
        if (msAcc >= 31250) {
            msAcc -= 31250;
            mystic::audio::millisecond(); 
        }
        
        sigmaDeltaWrite(0, mystic::audio::sample()); 
    } else {
        // РЕЖИМ BEEPS
        static uint16_t phase = 0;
        uint16_t hz = mystic::tones::current_hz;
        if (hz > 0 && mystic::tones::remaining_ms > 0) {
            uint16_t half_period = 31250 / (2 * hz);
            if (half_period == 0) half_period = 1;
            
            phase++;
            if (phase >= half_period * 2) phase = 0;
            
            if (phase < half_period) sigmaDeltaWrite(0, 192);
            else sigmaDeltaWrite(0, 64);
        } else {
            sigmaDeltaWrite(0, 128);
        }
    }
}

void startHardware() {
    if (hw_initialized) return; 
    
    sigmaDeltaSetup(0, 312500);
    sigmaDeltaAttachPin(SOUND_PIN);
    sigmaDeltaEnable();

    timer1_isr_init();
    timer1_attachInterrupt(audioISR);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    timer1_write(160); 
    
    hw_initialized = true;
}

void stopHardware() {
    // Больше никаких отключений таймера. Он работает всегда!
}
}

namespace mystic { namespace audio {
void begin(bool enabled, Mode m) {
    ready=false;
    tones::begin(false);
    tones::end();
    
    on=enabled; selectedMode=m; frozen=false;
    currentSong=previousState=previousLevel=255; bonusIndex=0;
    clockFraction=tickCounter=0;
#if CHGAME_DEBUG
    sampleCounter=0;
#endif

    if (on) startHardware();

    if (m==Beeps) {
        tones::begin(enabled);
    } else {
        play(0);                 
    }
}
void setMode(Mode m) { if (m!=selectedMode) begin(on,m); }
Mode mode() { return selectedMode; }
bool enabled() { return on; }
void setEnabled(bool value) {
    if (on==value) return;
    on=value;
    
    if (on) startHardware();
    
    if (selectedMode==Beeps) {
        tones::setEnabled(value);
    } else {
        ready=false;
        play(0); 
        currentSong=255;
    }
}

void update(uint8_t state, uint8_t level) {
    if (state==6 && previousState!=6) bonusIndex=0;
    previousState=state;
    if (selectedMode!=Atm) return;
    unsigned want=0;
    if (on) {
        if (state<6) want=1;
        else if (state==7 || state==8)
            want=currentSong>=2 && currentSong<7 && level==previousLevel ? currentSong : 2+(level%TUNE_COUNT);
    }
    if (want!=currentSong) play(want);
    previousLevel=level;
}

void tone(uint16_t hz, uint16_t duration, Effect effect) {
    if (selectedMode==Beeps) { tones::tone(hz,duration); return; }
    if (!on || !ready) return;
    const uint8_t *track=sfxSelect;
    switch (effect) {
        case Move: track=sfxMove; break;
        case Select: case CountEnd: track=sfxSelect; break;
        case Jump: track=sfxJump; break;
        case Step: track=sfxStep; break;
        case Land: track=sfxLand; break;
        case Wind: track=sfxWind; break;
        case Hurt: track=sfxHurt; break;
        case Key: track=sfxKey; break;
        case Coin: track=sfxCoin; break;
        case Pop: track=sfxPop; break;
        case Count: track=sfxCountList[bonusIndex>5 ? 3 : bonusIndex>>1]; ++bonusIndex; break;
    }
    
    // Умная маршрутизация каналов:
    // Если играет заглавная тема (currentSong == 1), отправляем SFX на Канал 1 (барабаны).
    // Во всех остальных случаях (в игре) отправляем на Канал 0 (он там свободен).
    uint8_t sfxChannel = (currentSong == 1) ? 1 : 0;
    
    tracker.playSfx(track, sfxChannel);
}

bool effectPlaying() {
    return ready && *static_cast<volatile uint8_t *>(&atmSfxMask)!=0;
}
bool saveReady() { return selectedMode==Beeps ? !tones::playing() : !effectPlaying(); }
void suspend() {
    frozen=true;
}
void resume() {
    frozen=false;
}

void ICACHE_RAM_ATTR millisecond() {
    if (!on || selectedMode!=Atm || !ready || frozen) return;
    clockFraction+=15625;
    const uint32_t period=uint32_t(cia)*1000;
    if (clockFraction>=period) {
        clockFraction-=period;
        ATM_playroutine(); ++tickCounter;
        publish();
    }
}

ICACHE_RAM_ATTR uint8_t sample() {
    if (!ready || !on || frozen) return 128;
#if CHGAME_DEBUG
    ++sampleCounter;
#endif
    const Mix &m=controls[front];
    for (unsigned i=0;i<3;++i) phases[i]+=m.increment[i];
    const unsigned p0=phases[0]>>8, p1=phases[1]>>8, p2=phases[2]>>8;
    int mix=p0>=192 ? -int(m.volume[0]) : int(m.volume[0]);
    mix+=p1&128 ? -int(m.volume[1]) : int(m.volume[1]);
    mix+=((int(p2)-128)*int(m.volume[2]))>>8;
    
    if (mix > 127) mix = 127;
    else if (mix < -128) mix = -128;
    
    return uint8_t(128+mix);
}

Voice voice(unsigned ch) {
    if (ch>=3) return {};
    return {controls[front].increment[ch],controls[front].volume[ch],
            (atmSfxMask & (1u<<ch)) ? sfxV.note : channel[ch].note};
}
unsigned song() { return currentSong; }
uint32_t ticks() { return tickCounter; }
uint32_t samples() {
#if CHGAME_DEBUG
    return sampleCounter;
#else
    return 0;
#endif
}
} }