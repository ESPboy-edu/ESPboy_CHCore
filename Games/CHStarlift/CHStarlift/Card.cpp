// STARLIFT.DAT in PROGMEM (Card.h)
#include <CHGame.h>
#include "Card.h"
#include "CardIndex.h"
#include "starlift_dat.h" // Имя файла должно совпадать с тем, что вы сгенерировали

namespace card {

bool lowPri;

bool open() {
    // Используем starlift_dat вместо STARLIFT_DAT
    if (memcmp_P("STARLIFT", &starlift_dat[0], 8) == 0 &&
        memcmp_P(&CARD_HASH, &starlift_dat[8], 4) == 0) {
        return true;
    }
    return false;
}

bool ok() { 
    return true; 
}

void frame() { 
}

const uint8_t *at(uint32_t off) {
    return &starlift_dat[off];
}

bool copy(uint8_t *dst, uint32_t off, uint16_t n) {
    memcpy_P(dst, &starlift_dat[off], n);
    return true;
}

}  // namespace card
