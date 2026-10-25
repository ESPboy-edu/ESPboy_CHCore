#pragma once
#include <Arduino.h>

// Форматы данных оригинального картриджа PICO-8
struct P8Note { 
    uint8_t pitch; 
    uint8_t wave; 
    uint8_t vol; 
    uint8_t fx; 
};

struct P8Sfx { 
    P8Note notes[32]; 
    uint8_t speed; 
    uint8_t loop_start; 
    uint8_t loop_end; 
};

struct P8Pattern { 
    uint8_t sfx[4]; 
    bool loop_begin; 
    bool loop_end; 
    bool stop; 
    bool _pad; // Выравнивание до 8 байт для избежания Exception 9
};

namespace pico_synth {
    // Инициализация синтезатора. Передайте указатели на сгенерированные массивы в PROGMEM
    void begin(const P8Sfx* sfx_rom, const P8Pattern* music_rom);
    
    void sfx(int id, int channel = -1);
    void music(int pattern, int fadeMs = 0);
    void stopMusic();
    
    // Вызывать в каждом кадре внутри loop() вместо audio::update()
    void update();

    // Управление прерываниями для безопасного сохранения во флэш
    void pause();
    void resume();
}
