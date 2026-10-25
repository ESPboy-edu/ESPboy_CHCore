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

// ESP8266 EEPROM emulation doesn't have image overlap issues like CH32V
bool available() { return !broken; }
static bool twoPages() { return true; }

// Read a specific page from EEPROM into our static buffer
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
    const Header *h = (const Header *)p;
    uint32_t at = crcAt(size);
    return h->magic == magic && h->version == version && *(const uint32_t *)(p + at) == crc32(p, at);
}

const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag) {
    if (!available() || size > MAX_DATA) return nullptr;
    
    // Read Page A
    const uint8_t *a = readPage(0);
    bool va = valid(a, magic, version, size);
    Header hA;
    if (va) memcpy(&hA, a, sizeof(Header)); // Cache header since buffer will be overwritten
    
    // Read Page B
    const uint8_t *b = readPage(256);
    bool vb = valid(b, magic, version, size);
    
    const uint8_t *r = nullptr;
    if (va && vb) {
        Header hB;
        memcpy(&hB, b, sizeof(Header));
        r = (int16_t)(hA.seq - hB.seq) > 0 ? readPage(0) : b; // Reload A if it was newer
    } else {
        r = va ? readPage(0) : (vb ? b : nullptr);
    }
    
    if (!r) return nullptr;
    
    const Header *h = (const Header *)r;
    lastSeq = h->seq;
    if (flag) *flag = h->flag;
    return r + sizeof(Header);
}

void *buffer() {
    // Uses idle chunk buffers to prepare save data (no RAM cost)
    uint8_t *buf = gfx_chunkScratch();          
    memset(buf, 0, PAGE);
    return buf + sizeof(Header);
}

bool write(uint32_t magic, uint8_t version, uint16_t size, uint8_t flag) {
    if (!available() || size > MAX_DATA) return false;
    uint8_t *buf = gfx_chunkScratch();
    Header *h = (Header *)buf;
    h->magic = magic;
    h->version = version;
    h->flag = flag;
    h->seq = (uint16_t)(lastSeq + 1);
    uint32_t at = crcAt(size);
    *(uint32_t *)(buf + at) = crc32(buf, at);
    
    uint32_t offset = (h->seq & 1) ? 256 : 0;   // Alternate pages
    
    if (!writePage(offset, buf)) { broken = true; return false; }
    lastSeq = h->seq;
    return true;
}

}  // namespace save