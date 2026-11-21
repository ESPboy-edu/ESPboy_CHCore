#include <pgmspace.h>
#include <EEPROM.h>
#include <CHGame.h>
#include "Card.h"
#include "CardIndex.h"
#include "sddata.h"

extern const uint8_t CHIX_DAT[] PROGMEM; 

namespace card {
    static uint16_t currentBlock = 0xffff;
    static bool isCardOpen = false;

    bool open() {
        EEPROM.begin(512);
        isCardOpen = true; 
        return true;
    }

    bool ok() { return isCardOpen; }
    
    void frame() { currentBlock = 0xffff; }

    bool stream(uint32_t k, uint32_t n, uint8_t *buf1, BlockFn fn, void *ctx) {
        if (!isCardOpen) return false;
        
        currentBlock = 0xffff; 
        for (uint32_t i = 0; i < n; i++) {
            memcpy_P(buf1, &CHIX_DAT[(k + i) * 512], 512);
            if (fn) fn(buf1, ctx);
        }
        return true;
    }

    const uint8_t *block(uint32_t k) {
        if (!isCardOpen) return nullptr;
        uint8_t *b = gfx_chunkScratch(); 
        
        if (currentBlock != k) {
            memcpy_P(b, &CHIX_DAT[k * 512], 512);
            currentBlock = k;
        }
        return b;
    }

    bool getSave(uint8_t *dst, uint8_t n) {
        uint32_t magic;
        EEPROM.get(0, magic);
        if (magic == CARD_HASH) {
            for (uint8_t i = 0; i < n; i++) dst[i] = EEPROM.read(4 + i);
            return true;
        }
        return false;
    }

    bool putSave(const uint8_t *p, uint8_t n) {
        if (!isCardOpen) return false;
        
        EEPROM.put(0, (uint32_t)CARD_HASH);
        for (uint8_t i = 0; i < n; i++) EEPROM.write(4 + i, p[i]);

        timer1_disable(); 
        EEPROM.commit(); 
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); 
        timer1_write(625); 
        
        return true;
    }
}  // namespace card