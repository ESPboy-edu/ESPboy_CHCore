// The puzzle packs (Pack.h)

#pragma GCC optimize("Os", "no-ipa-sra")
#include <string.h>
#include <pgmspace.h>
#include "Pack.h"
#include "Puzzle.h"
#include "src/game/PuzzleData.h"
#include "src/game/BonusData.h"

namespace pack {

static const uint8_t HEAD = 28;
static uint8_t curPack = 0;

static uint32_t le32_P(const uint8_t *p) { 
    return pgm_read_byte(p) | 
          (pgm_read_byte(p + 1) << 8) | 
          (pgm_read_byte(p + 2) << 16) | 
          ((uint32_t)pgm_read_byte(p + 3) << 24); 
}


static const uint8_t *hdr() { 
    return (curPack == 1) ? BONUS_PACK : BUILTIN; 
}

static bool goodHeader(const uint8_t *h) {
    char magic[4];
    for(int i = 0; i < 4; i++) magic[i] = pgm_read_byte(h + i);
    if (memcmp(magic, "CHCW", 4)) return false;
    if (pgm_read_byte(h + 4) != 1 || pgm_read_byte(h + 5) != 1) return false;
    
    uint8_t count = pgm_read_byte(h + 6);
    if (!count || count > 32) return false;

    uint8_t tmp[24 + 4 * 32];
    for (uint32_t i = 0; i < 24; i++) tmp[i] = pgm_read_byte(h + i);
    for (uint32_t i = 0; i < 4u * count; i++) tmp[24 + i] = pgm_read_byte(h + HEAD + i);
    
    uint16_t crc = pgm_read_byte(h + 24) | (pgm_read_byte(h + 25) << 8);
    return puz::crc16_P(tmp, 24 + 4u * count) == crc;
}

void scan() {}
Card card() { return CARD_NONE; }

uint8_t count() { return 2; } 

bool select(uint8_t p) {
    if (p >= 2) return false;
    curPack = p;
    return goodHeader(hdr());
}

uint8_t current() { return curPack; }

const char *name() { 
    return (const char *)(hdr() + 8); 
}

uint8_t puzzles() { return pgm_read_byte(hdr() + 6); }
uint32_t id() { return le32_P(hdr() + 20); }

static void entry(uint8_t i, uint32_t &off, uint32_t &len) {
    uint32_t e = le32_P(hdr() + HEAD + 4 * i);
    off = e >> 12;
    len = e & 0xFFF;
}

bool peek(uint8_t i, uint8_t &size, uint8_t &diff, char *title) {
    if (i >= puzzles()) return false;
    uint32_t off, len;
    entry(i, off, len);
    return puz::peek(hdr() + off, (uint16_t)len, size, diff, title);
}

bool open(uint8_t i) {
    if (i >= puzzles()) return false;
    uint32_t off, len;
    entry(i, off, len);
    return puz::load(hdr() + off, (uint16_t)len);
}

bool find(uint32_t packId) {
    for (uint8_t p = 0; p < 2; p++) {
        if (select(p) && id() == packId) return true;
    }
    select(0);
    return false;
}

}  // namespace pack