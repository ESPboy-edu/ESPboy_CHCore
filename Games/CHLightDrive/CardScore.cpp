#include <CHGame.h>
#include <EEPROM.h>
#include "CardScore.h"

namespace cardscore {
    Status status = Unavailable;
    
    // Добавляем magic-число для проверки валидности данных в EEPROM
    struct Score { uint32_t magic; uint32_t sequence, best; uint16_t wakeups, avoided; };
    
    const uint32_t EEPROM_MAGIC = 0x43534352; // "CSCR"
    const int EEPROM_OFFSET = 256; // Смещение, чтобы не затереть базовые настройки игры

    bool load(uint32_t &best) {
        EEPROM.begin(512);
        Score s;
        EEPROM.get(EEPROM_OFFSET, s);
        
        if (s.magic == EEPROM_MAGIC) {
            best = s.best;
            status = Ready;
            return true;
        }
        
        status = Unavailable;
        return false;
    }

    bool store(uint32_t best, uint16_t wakeups, uint16_t avoided) {
        EEPROM.begin(512);
        Score s;
        EEPROM.get(EEPROM_OFFSET, s);
        
        // Если рекорд уже существует и он лучше текущего, ничего не пишем
        if (s.magic == EEPROM_MAGIC && s.best >= best) {
            status = Saved;
            return true;
        }
        
        s.sequence = (s.magic == EEPROM_MAGIC) ? s.sequence + 1 : 1;
        s.magic = EEPROM_MAGIC;
        s.best = best;
        s.wakeups = wakeups;
        s.avoided = avoided;
        
        EEPROM.put(EEPROM_OFFSET, s);
        bool ok = EEPROM.commit();
        
        status = ok ? Saved : Failed;
        return ok;
    }
}