#pragma GCC optimize("Os", "no-ipa-sra", "no-inline-functions-called-once", "no-jump-tables", "no-guess-branch-probability")
#include <Arduino.h>
#include <string.h>
#include "Bank.h"
#include "src/bank/BankData.h"

// Подключаем наш банк фраз, перенесенный во флеш-память
#include "phrases_bnk.h"

namespace bank {

static uint8_t nCats;
static bool live;
static uint16_t counts[SECTIONS], firsts[SECTIONS];
static uint32_t cardId;

void begin() {
    live = false;
    
    // Проверка магической строки "WWPB\x01" в начале массива
    if (memcmp_P("WWPB\x01", PHRASES_BNK, 6) != 0) return;
    
    nCats = pgm_read_byte(PHRASES_BNK + 6);
    
    for (uint8_t s = 0; s < SECTIONS; s++) {
        uint32_t n, first;
        memcpy_P(&n, PHRASES_BNK + 8 + 4 * s, 4);
        memcpy_P(&first, PHRASES_BNK + 20 + 4 * s, 4);
        if (!n || n > 0xFFFF || first > 0xFFFF) return;
        counts[s] = (uint16_t)n;
        firsts[s] = (uint16_t)first;
    }
    memcpy_P(&cardId, PHRASES_BNK + 32, 4);
    live = true;
}

bool card() { return live; }
uint16_t count(uint8_t section) { return live && section < SECTIONS ? counts[section] : flashCount(section); }
uint32_t id() { return live ? cardId : BANK_ID; }

bool fetch(uint8_t section, uint16_t index, char *text, char *category) {
    // Если массив не прошел проверку в begin(), используем встроенный банк (fallback)
    if (!live) { flashFetch(section, index, text, category); return true; }
    
    if (section < SECTIONS && index < counts[section]) {
        uint32_t rec = (uint32_t)firsts[section] + index;
        
        // Первый блок (заголовок) занимает 512 байт. Записи идут следом по 64 байта.
        uint32_t offset = 512 + (rec * 64);
        
        if (offset + 64 <= PHRASES_BNK_LEN) {
            uint8_t p[64];
            memcpy_P(p, PHRASES_BNK + offset, 64);
            
            uint8_t sum = 0;
            for (uint8_t i = 0; i < 64; i++) sum = (uint8_t)(sum + p[i]);
            
            uint8_t cat = p[0];
            // Контрольная сумма и проверка соответствия секции и категории
            if (sum == 0xFF && p[1] == section && cat < nCats) {
                memcpy(text, p + 2, TEXT_MAX);
                text[TEXT_MAX - 1] = 0;
                
                // Категории хранятся в нулевом блоке со смещением 64 (каждая по 20 байт)
                uint32_t catOffset = 64 + (cat * 20);
                memcpy_P(category, PHRASES_BNK + catOffset, CAT_MAX);
                category[CAT_MAX - 1] = 0;
                
                return true;
            }
        }
    }
    
    live = false; // При ошибке чтения отключаем внешний банк
    return false;
}

}  // namespace bank