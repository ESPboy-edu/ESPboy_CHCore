#pragma GCC optimize("Os", "no-ipa-sra")
#include <Arduino.h>
#include <EEPROM.h>
#include <CHGfx.h>
#include <string.h>
#include "Save.h"
#include "RamFunc.h"

namespace save {

static const uint32_t PAGE = 256;
static const uint32_t EEPROM_SIZE = 512; // Two pages: PAGE_A (0) and PAGE_B (256)

struct Header { uint32_t magic; uint8_t version, flag; uint16_t seq; };

static uint16_t lastSeq = 0;
static bool broken = false;
static bool eepromInitialized = false;

// Internal cache for read operations
static uint8_t pageBuffer[PAGE];

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static uint32_t crcAt(uint16_t size) { return (uint32_t)(sizeof(Header) + size + 3) & ~3u; }

static void initEEPROM() {
    if (!eepromInitialized) {
        EEPROM.begin(EEPROM_SIZE);
        eepromInitialized = true;
    }
}

bool available() { return !broken; }
static bool twoPages() { return true; }

static const uint8_t *readPage(uint32_t offset) {
    initEEPROM();
    for(uint32_t i = 0; i < PAGE; i++) {
        pageBuffer[i] = EEPROM.read(offset + i);
    }
    return pageBuffer;
}

static bool writePage(uint32_t offset, const uint8_t *buf) {
    initEEPROM();
    for(uint32_t i = 0; i < PAGE; i++) {
        EEPROM.write(offset + i, buf[i]);
    }
    return EEPROM.commit();
}

static bool valid(const uint8_t *p, uint32_t magic, uint8_t version, uint16_t size) {
    Header h;
    memcpy(&h, p, sizeof(Header)); // Безопасное копирование
    uint32_t at = crcAt(size);
    
    uint32_t crc_read;
    memcpy(&crc_read, p + at, sizeof(uint32_t)); // Безопасное копирование
    
    return h.magic == magic && h.version == version && crc_read == crc32(p, at);
}

const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag) {
    if (!available() || size > MAX_DATA) return nullptr;
    
    const uint8_t *a = readPage(0);
    bool va = valid(a, magic, version, size);
    Header hA;
    if (va) memcpy(&hA, a, sizeof(Header));
    
    const uint8_t *b = readPage(256);
    bool vb = valid(b, magic, version, size);
    
    const uint8_t *r = nullptr;
    if (va && vb) {
        Header hB;
        memcpy(&hB, b, sizeof(Header));
        r = (int16_t)(hA.seq - hB.seq) > 0 ? readPage(0) : b; 
    } else {
        r = va ? readPage(0) : (vb ? b : nullptr);
    }
    
    if (!r) return nullptr;
    
    Header h;
    memcpy(&h, r, sizeof(Header));
    lastSeq = h.seq;
    if (flag) *flag = h.flag;
    return r + sizeof(Header);
}

void *buffer() {
    uint8_t *buf = gfx_chunkScratch();          
    memset(buf, 0, PAGE);
    return buf + sizeof(Header);
}

bool write(uint32_t magic, uint8_t version, uint16_t size, uint8_t flag) {
    if (!available() || size > MAX_DATA) return false;
    uint8_t *buf = gfx_chunkScratch();
    
    Header h;
    h.magic = magic;
    h.version = version;
    h.flag = flag;
    h.seq = (uint16_t)(lastSeq + 1);
    
    memcpy(buf, &h, sizeof(Header)); // Безопасное копирование
    
    uint32_t at = crcAt(size);
    uint32_t computed_crc = crc32(buf, at);
    memcpy(buf + at, &computed_crc, sizeof(uint32_t)); // Безопасное копирование
    
    uint32_t offset = (h.seq & 1) ? 256 : 0;
    
    if (!writePage(offset, buf)) { broken = true; return false; }
    lastSeq = h.seq;
    return true;
}

}  // namespace save