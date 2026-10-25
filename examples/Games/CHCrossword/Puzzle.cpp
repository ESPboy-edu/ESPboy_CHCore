// Unpacking a puzzle blob (Puzzle.h): the grid, the word list and where each
// clue starts. The reference decoder is tools/puzzles/cwformat.py; the host
// tests hold this one to it.

#pragma GCC optimize("Os", "no-ipa-sra")

#include <string.h>
#include <pgmspace.h>
#include "Puzzle.h"
#include "src/game/PuzzleData.h"

namespace puz {

uint8_t n, difficulty, whites, nWords, nAcross;
char title[TITLE_MAX + 1];
uint8_t sol[MAX_CELLS], cell[MAX_CELLS];
uint8_t wStart[MAX_WORDS], wLen[MAX_WORDS], wNum[MAX_WORDS];

static const uint8_t *srcPROGMEM;       // Pointer to the blob in PROGMEM
static uint16_t srcBits;                
static uint16_t wClue[MAX_WORDS];       
static uint8_t locked[(MAX_WORDS + 7) / 8];
static uint8_t nLocked;

uint16_t crc16_P(const uint8_t *p, uint32_t len) {
    uint32_t c = 0xFFFF;
    while (len--) {
        c ^= (uint32_t)pgm_read_byte(p++) << 8;
        for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1;
    }
    return (uint16_t)c;
}

struct Bits {
    const uint8_t *p;
    uint32_t pos, end;
    
    // 1. Переименовываем bit() в readBit()
    uint32_t readBit() {
        if (pos >= end) return 0;
        uint8_t b_byte = pgm_read_byte(p + (pos >> 3));
        uint32_t b = (b_byte >> (7 - (pos & 7))) & 1;
        pos++;
        return b;
    }
    
    uint32_t get(uint8_t k) {
        uint32_t v = 0;
        // 2. Меняем bit() на readBit()
        while (k--) v = (v << 1) | readBit();
        return v;
    }
    
    char sym() {
        uint32_t code = 0, first = 0, index = 0;
        for (uint8_t k = 0; k < HUFF_MAX; k++) {
            // 3. Меняем bit() на readBit()
            code |= readBit();
            uint32_t c = pgm_read_byte(HUFF_COUNT + k);
            if (code - first < c) return (char)pgm_read_byte(HUFF_SYM + index + code - first);
            index += c;
            first = (first + c) << 1;
            code <<= 1;
        }
        return 0;
    }
    
    void text(char *out, uint8_t cap) {
        uint8_t k = 0;
        for (char ch; (ch = sym()) != 0 && pos < end;)
            if (out && k < cap) out[k++] = ch;
        if (out) out[k] = 0;
    }
};

bool peek(const uint8_t *blob, uint16_t have, uint8_t &size, uint8_t &diff, char *titleOut) {
    if (have < 4) return false;
    uint8_t b0 = pgm_read_byte(blob);
    size = b0 & 15;
    diff = b0 >> 5;
    Bits b = {blob, 8, (uint32_t)have * 8};
    b.text(titleOut, TITLE_MAX);
    return size >= MIN_N;
}

bool load(const uint8_t *blob, uint16_t len) {
    if (len < 4) return false;
    uint16_t expectedCrc = pgm_read_byte(blob + len - 2) | (pgm_read_byte(blob + len - 1) << 8);
    if (crc16_P(blob, len - 2u) != expectedCrc) return false;
    
    uint8_t b0 = pgm_read_byte(blob);
    uint32_t size = b0 & 15, cells = size * size;
    if (size < MIN_N) return false;
    
    Bits b = {blob, 8, (len - 2u) * 8};
    b.text(title, TITLE_MAX);
    
    bool asym = b0 & 16;
    uint32_t stored = asym ? cells : (cells + 1) / 2;
    for (uint32_t i = 0; i < stored; i++) {
        uint8_t black = b.readBit() ? 0xFF : 0;
        sol[i] = black;
        if (!asym) sol[cells - 1 - i] = black;
    }
    uint32_t w = 0;
    for (uint32_t i = 0; i < cells; i++) {
        if (sol[i]) sol[i] = 0;
        else { sol[i] = (uint8_t)(b.get(5) + 1); w++; }
    }
    if (b.pos >= b.end) return false;
    n = (uint8_t)size;
    difficulty = b0 >> 5;
    whites = (uint8_t)w;

    uint32_t count = 0;
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t st = pass ? size : 1, num = 0;
        if (pass) nAcross = (uint8_t)count;
        for (uint32_t i = 0; i < cells; i++) {
            if (!sol[i]) continue;
            uint32_t r = i / size, c = i % size;
            bool a = (c == 0 || !sol[i - 1]) && c + 1 < size && sol[i + 1];
            bool d = (r == 0 || !sol[i - size]) && r + 1 < size && sol[i + size];
            if (a || d) num++;
            if (!(pass ? d : a)) continue;
            if (count == MAX_WORDS) return false;
            uint32_t k = 0, lim = pass ? size - r : size - c;
            while (k < lim && sol[i + k * st]) k++;
            wStart[count] = (uint8_t)i;
            wLen[count] = (uint8_t)k;
            wNum[count] = (uint8_t)num;
            count++;
        }
    }
    nWords = (uint8_t)count;
    for (uint32_t i = 0; i < count; i++) {
        wClue[i] = (uint16_t)b.pos;
        b.text(nullptr, 0);
    }
    srcPROGMEM = blob;
    srcBits = (uint16_t)b.end;
    memset(cell, 0, sizeof cell);
    clearLocks();
    return true;
}

uint8_t wordAt(uint8_t c, bool down) {
    if (c >= n * n || !sol[c]) return NONE;
    if (down) while (c >= n && sol[c - n]) c = (uint8_t)(c - n);
    else while (c % n && sol[c - 1]) c--;
    for (uint8_t w = down ? nAcross : 0, e = down ? nWords : nAcross; w < e; w++)
        if (wStart[w] == c) return w;
    return NONE;
}

void clue(uint8_t w, char *out) {
    Bits b = {srcPROGMEM, wClue[w], srcBits};
    b.text(out, CLUE_MAX - 1);
}

bool wordFull(uint8_t w) {
    for (uint8_t k = 0; k < wLen[w]; k++)
        if (!(cell[cellOf(w, k)] & LETTER)) return false;
    return true;
}

bool wordRight(uint8_t w) {
    for (uint8_t k = 0; k < wLen[w]; k++) {
        uint8_t c = cellOf(w, k);
        if ((cell[c] & LETTER) != sol[c]) return false;
    }
    return true;
}

bool wordLocked(uint8_t w) { return locked[w >> 3] >> (w & 7) & 1; }

void lockWord(uint8_t w) {
    if (wordLocked(w)) return;
    locked[w >> 3] |= (uint8_t)(1 << (w & 7));
    nLocked++;
    for (uint8_t k = 0; k < wLen[w]; k++) cell[cellOf(w, k)] |= LOCKED;
}

uint8_t lockedWords() { return nLocked; }

void clearLocks() {
    memset(locked, 0, sizeof locked);
    nLocked = 0;
    for (uint8_t &c : cell) c &= (uint8_t)~LOCKED;
}

}  // namespace puz
