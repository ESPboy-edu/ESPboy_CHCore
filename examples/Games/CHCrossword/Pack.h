// Where puzzles come from: the built-in pack in flash, and packs on the SD
// card (files *.CWD in the folder CHCW; made by tools/puzzles/build_pack.py
// or tools/puzzles/puz2cwd.py). A pack's layout is in tools/puzzles/cwformat.py.
//
// BUS RULE: the card shares SPI1 with the LCD. scan(), select(), peek() and
// open() may read the card: call them only after gfx_wait(), from update -
// never while drawing (they borrow CHGfx's chunk scratch as a sector buffer,
// which the lettering masks use during render).

#pragma once
#include <stdint.h>

namespace pack {

constexpr uint8_t MAX_PACKS = 2; //(BUILTIN) + (BONUS) 

enum Card : uint8_t { CARD_NONE, CARD_UNREADABLE, CARD_EXFAT, CARD_EMPTY, CARD_OK };

void scan();
Card card();
uint8_t count();                        

bool select(uint8_t p);
uint8_t current();
const char *name();                     
uint8_t puzzles();
uint32_t id();

bool peek(uint8_t i, uint8_t &size, uint8_t &diff, char *title);
bool open(uint8_t i);
bool find(uint32_t packId);

}  // namespace pack