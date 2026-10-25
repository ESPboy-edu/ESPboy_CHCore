#include "Sounds.h"
#include "PicoSynth.h"
#include "PicoAudioData.h"

namespace snd {

void begin() {
    // Передаем массивы из PicoAudioData.h в движок синтезатора
    pico_synth::begin(PICO_SFX, PICO_MUSIC);
}

void sfx(int n) {
    pico_synth::sfx(n);
}

void music(int n, int fadeMs, int) {
    if (n < 0) {
        pico_synth::stopMusic();
    } else {
        pico_synth::music(n, fadeMs);
    }
}

} // namespace snd
