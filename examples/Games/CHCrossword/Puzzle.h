// A crossword, decoded: the grid, its words and where their clues are.
//
// A puzzle arrives as a blob (the format is described in
// tools/puzzles/cwformat.py, which also has the reference decoder): the
// same bytes whether it is one of the built-in ones in flash or was read
// from a pack on the SD card. The grid and the word list are unpacked into
// RAM once; the clues stay packed and are decoded one at a time, when
// shown. No graphics in here: the host tests build this file as it is.

#pragma once
#include <stdint.h>
#include <pgmspace.h>

namespace puz {

constexpr uint8_t MIN_N = 5, MAX_N = 15;
constexpr uint8_t MAX_CELLS = 225;
constexpr uint8_t MAX_WORDS = 96;
constexpr uint8_t NONE = 0xFF;
constexpr uint8_t TITLE_MAX = 20;
constexpr uint8_t CLUE_MAX = 96;

enum : uint8_t { LETTER = 0x1F, LOCKED = 0x40, REVEALED = 0x80 };

extern uint8_t n;                       
extern uint8_t difficulty;
extern uint8_t whites;                  
extern uint8_t nWords, nAcross;         
extern char title[TITLE_MAX + 1];
extern uint8_t sol[MAX_CELLS];          
extern uint8_t cell[MAX_CELLS];
extern uint8_t wStart[MAX_WORDS];       
extern uint8_t wLen[MAX_WORDS];
extern uint8_t wNum[MAX_WORDS];         

// Unpacks a blob directly from PROGMEM
bool load(const uint8_t *blobPROGMEM, uint16_t len);
bool peek(const uint8_t *blobPROGMEM, uint16_t have, uint8_t &size, uint8_t &diff, char *titleOut);

inline bool isDown(uint8_t w) { return w >= nAcross; }
inline uint8_t step(uint8_t w) { return isDown(w) ? n : 1; }
inline uint8_t cellOf(uint8_t w, uint8_t k) { return (uint8_t)(wStart[w] + k * step(w)); }

uint8_t wordAt(uint8_t c, bool down);
void clue(uint8_t w, char *out);

bool wordFull(uint8_t w);
bool wordRight(uint8_t w);
bool wordLocked(uint8_t w);
void lockWord(uint8_t w);
uint8_t lockedWords();
void clearLocks();

uint16_t crc16_P(const uint8_t *pPROGMEM, uint32_t len);

}  // namespace puz