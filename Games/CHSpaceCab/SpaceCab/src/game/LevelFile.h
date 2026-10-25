// The SD card's level files, from flash (Levels.cpp): the part of Pokitto's
// File the game's Level::reset() uses.
#pragma once
#include <stdint.h>

extern const uint8_t LEVEL_COUNT;           // levels 0 (the title's) ... LEVEL_COUNT-1

class Fle {
public:
    bool openRO(const char *name);
    uint32_t read(void *dst, uint32_t n);
    void seek(uint32_t pos) { _pos = pos <= _size ? pos : _size; }
private:
    const uint8_t *_data = nullptr;
    uint32_t _size = 0, _pos = 0;
};
