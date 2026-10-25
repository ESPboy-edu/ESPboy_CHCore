#pragma once
#include <stdint.h>
#include <string.h>

namespace save {

static const uint16_t MAX_DATA = 256 - 12;

constexpr uint32_t magic(const char (&s)[5]) {
    return (uint32_t)(uint8_t)s[0] | (uint32_t)(uint8_t)s[1] << 8 |
           (uint32_t)(uint8_t)s[2] << 16 | (uint32_t)(uint8_t)s[3] << 24;
}

bool available();
const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag = nullptr);
void *buffer();
bool write(uint32_t magic, uint8_t version, uint16_t size, uint8_t flag = 0);

template <class T> bool load(uint32_t magic, uint8_t version, T &data, uint8_t *flag = nullptr) {
    static_assert(sizeof(T) <= MAX_DATA, "save data must fit one flash page");
    const void *p = read(magic, version, sizeof(T), flag);
    if (!p) return false;
    memcpy(&data, p, sizeof(T));
    return true;
}

template <class T> bool store(uint32_t magic, uint8_t version, const T &data, uint8_t flag = 0) {
    static_assert(sizeof(T) <= MAX_DATA, "save data must fit one flash page");
    if (!available()) return false;
    memcpy(buffer(), &data, sizeof(T));
    return write(magic, version, sizeof(T), flag);
}

}  // namespace save