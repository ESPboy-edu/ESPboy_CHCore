#pragma once
#include "App.h"
#include <LittleFS.h>

namespace stlreader {

enum State : uint8_t { NONE, READY, NOFS };

State mount(); // Инициализация LittleFS
State state();

// Открытие файла из структуры Entry
int8_t openFile(const Entry &e);

// Чтение нулевого блока (для заголовка)
const uint8_t *block0();

// Функция-коллбэк для потока (как в SD-библиотеке)
typedef void (*BlockFn)(const uint8_t *blk, void *ctx);

// Потоковое чтение кусков по 512 байт
bool stream(uint32_t startBlock, uint32_t numBlocks, BlockFn fn, void *ctx);

} // namespace stlreader