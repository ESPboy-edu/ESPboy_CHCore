#pragma once
#include <CHGame.h>
#include "config.h"
#include "StlRender.h"
#include "Agent.h"
#include "Fx.h"
#include "Sounds.h"
#include <LittleFS.h>

extern const uint16_t PALETTE[16];

static_assert(STL_SHADE0 == 8 && STL_SHADES == 8, "the wire ramp is slots 8-15");
enum : uint8_t {
    WIRE_CHIP = STL_SHADE0 + 3,
    WIRE_TEXT = STL_SHADE0 + 6,
    AXIS_X = agent::ALERT, AXIS_Y = agent::LIVE, AXIS_Z = agent::PALE,
};

enum class Scr : uint8_t { Intro, List, View, Msg };
extern Scr screen;

static const uint8_t MAX_ENTRIES = 40;
static const uint16_t DRAFT_POINTS = 256;

// Типы источников для моделей
enum SourceType : uint8_t { SRC_PROGMEM, SRC_LITTLEFS };

struct Entry {
    SourceType source;
    uint32_t size;
    char name[16];          // Имя для отображения
    const uint8_t* pgmData; // Указатель, если это PROGMEM (иначе nullptr)
    bool isDir;             // Оставляем для совместимости (хотя папок у нас не будет)
};

union Arena {
    Entry entries[MAX_ENTRIES];
    int8_t points[DRAFT_POINTS][3];
};
extern Arena arena;

namespace browser {
void begin();                   
void frame();                   
void back();                    
void message(const char *title, const char *l1, const char *l2);
}

namespace viewer {
bool open(const Entry& entry); // Теперь передаем всю структуру Entry
void frame();
char *status(char *p);         
}

void cubeIcon(int x, int y, uint8_t edge, uint8_t face);
char *putCount(char *p, uint32_t v);
