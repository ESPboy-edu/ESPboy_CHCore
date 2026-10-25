#pragma GCC optimize("Os", "no-ipa-sra", "no-caller-saves")
#include "Dict.h"
#include "src/dict/DictData.h"

namespace dict {

void begin() {
    // Больше ничего инициализировать не нужно
}

bool card() { return false; }

uint32_t count() { return DICT_WORDS; }

bool has(const uint8_t *w, uint8_t n) {
    // Перенаправляем поиск ВСЕХ слов во встроенный словарь в PROGMEM
    return hasCore(w, n);
}

bool randomWord(char *out, uint32_t r) {
    // Упрощенная заглушка или можно реализовать выбор случайного слова из hasCore
    return false; 
}

}  // namespace dict
