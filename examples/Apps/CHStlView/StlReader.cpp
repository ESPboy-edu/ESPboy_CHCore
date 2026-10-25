#pragma GCC optimize("Os")
#include <CHGfx.h>
#include "StlReader.h"

namespace stlreader {

static State st = NONE;
static Entry currentEntry;
static File currentFile;

State mount() {
    gfx_wait();
    st = NONE;
    if (LittleFS.begin()) {
        st = READY;
    } else {
        st = NOFS;
    }
    return st;
}

State state() { return st; }

int8_t openFile(const Entry &e) {
    gfx_wait();
    currentEntry = e;
    
    if (e.source == SRC_LITTLEFS) {
        if (currentFile) currentFile.close();
        char path[32] = "/";
        strcat(path, e.name);
        currentFile = LittleFS.open(path, "r");
        if (!currentFile) return -1;
    }
    return 1; // 1 = файл успешно открыт (имитация runCount)
}

const uint8_t *block0() {
    gfx_wait();
    uint8_t *b = gfx_chunkScratch();
    
    if (currentEntry.source == SRC_PROGMEM) {
        memcpy_P(b, currentEntry.pgmData, 512);
        return b;
    } else if (currentEntry.source == SRC_LITTLEFS && currentFile) {
        currentFile.seek(0, SeekSet);
        currentFile.read(b, 512);
        return b;
    }
    return nullptr;
}

bool stream(uint32_t b_idx, uint32_t n, BlockFn fn, void *ctx) {
    gfx_wait();
    uint8_t *buf = gfx_chunkScratch(); // Буфер 512 байт
    
    uint32_t offset = b_idx * 512;
    
    if (currentEntry.source == SRC_LITTLEFS && currentFile) {
        currentFile.seek(offset, SeekSet);
    }

    for (uint32_t i = 0; i < n; i++) {
        if (currentEntry.source == SRC_PROGMEM) {
            // Читаем блок 512 байт из PROGMEM
            memcpy_P(buf, currentEntry.pgmData + offset + (i * 512), 512);
        } else if (currentEntry.source == SRC_LITTLEFS && currentFile) {
            // Читаем из файловой системы
            currentFile.read(buf, 512);
        } else {
            return false;
        }
        
        // Отдаем буфер парсеру (имитация DMA коллбэка)
        fn(buf, ctx);
    }
    return true;
}

} // namespace stlreader