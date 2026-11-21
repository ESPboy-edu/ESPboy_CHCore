#pragma once
#include <stdint.h>

namespace card {
typedef void (*BlockFn)(const uint8_t *blk, void *ctx);

bool open();
bool ok();
void frame();
const uint8_t *block(uint32_t k);
bool stream(uint32_t k, uint32_t n, uint8_t *buf1, BlockFn fn, void *ctx);
bool getSave(uint8_t *dst, uint8_t n);
bool putSave(const uint8_t *p, uint8_t n);
}  // namespace card