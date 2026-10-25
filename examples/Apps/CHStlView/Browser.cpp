// The intro, the files from LittleFS & PROGMEM, and the refusals.
// Drawn as CHSDtoUSB's instrument panel (Agent.h).
#pragma GCC optimize("Os")
#include "App.h"
#include "StlReader.h"
#include <string.h>
#include <LittleFS.h>

using namespace agent;

Arena arena;

// Декларации встроенных моделей (KNOT исключен ради экономии IROM)
#include "ModelsData.h"

char *putCount(char *p, uint32_t v) {
    if (v < 100000) return fmtInt(p, (int32_t)v);
    return fmtStr(fmtInt(p, (int32_t)((v + 500) / 1000)), "K");
}

void cubeIcon(int x, int y, uint8_t edge, uint8_t face) {
    gfx_fillRect(x + 1, y + 3, 5, 5, face);
    gfx_rect(x, y + 2, 6, 7, edge);
    gfx_hline(x + 2, y, 5, edge);
    gfx_vline(x + 6, y, 7, edge);
    gfx_pixel(x + 1, y + 1, edge);
    gfx_pixel(x + 5, y + 1, edge);
    gfx_pixel(x + 5, y + 8, BG);
}

namespace browser {

// ---- The file list -------------------------------------------------------------
static const uint8_t VISIBLE = 10, LIST_TOP = 24, ROW_H = 9;
static Entry *const entries = arena.entries;
static uint8_t count, stls;
static bool truncated;
static int sel, top;
static int barY = -1;                   
static uint8_t t;                       

static const char *msgTitle, *msgL1, *msgL2;
static char msgBuf[3][22];

static void scan() {
    count = stls = 0;
    truncated = false;

    // 1. Сначала добавляем встроенные модели из PROGMEM
    auto addProgmem = [](const char* name, const uint8_t* data, uint32_t size) {
        if (count >= MAX_ENTRIES) { truncated = true; return; }
        Entry &e = entries[count++];
        e.source = SRC_PROGMEM;
        strncpy(e.name, name, 15);
        e.name[15] = 0;
        e.pgmData = data;
        e.size = size;
        e.isDir = false;
        stls++;
    };

    // Регистрируем все PROGMEM модели кроме тяжелого KNOT
    addProgmem("CUBE.STL", MODEL_CUBE, sizeof(MODEL_CUBE));
    addProgmem("BOWL.STL", MODEL_BOWL, sizeof(MODEL_BOWL));
    addProgmem("GEAR.STL", MODEL_GEAR, sizeof(MODEL_GEAR));
    addProgmem("ICOSPH.STL", MODEL_ICOSPH, sizeof(MODEL_ICOSPH));
    //addProgmem("TORUS.STL", MODEL_TORUS, sizeof(MODEL_TORUS));

    // 2. Сканируем LittleFS
    if (stlreader::state() == stlreader::READY) {
        Dir dir = LittleFS.openDir("/");
        while (dir.next() && count < MAX_ENTRIES) {
            String fileName = dir.fileName();
            String upperName = fileName;
            upperName.toUpperCase(); // Увеличиваем регистр для шрифта!
            
            if (upperName.endsWith(".STL")) {
                Entry &e = entries[count++];
                e.source = SRC_LITTLEFS;
                // Сохраняем имя в ВЕРХНЕМ РЕГИСТРЕ
                strncpy(e.name, upperName.c_str(), 15);
                e.name[15] = '\0';
                e.size = dir.fileSize();
                e.pgmData = nullptr;
                e.isDir = false;
                stls++;
            }
        }
        if (dir.next()) truncated = true;
    }

    if (sel >= count) sel = count ? count - 1 : 0;
    if (top > sel) top = sel;
    if (sel >= top + VISIBLE) top = sel - VISIBLE + 1;
    barY = -1;
}

static void mountAndScan() {
    stlreader::mount();
    sel = top = 0;
    scan();
}

void back() {
    scan();
    screen = Scr::List;
    t = 0;
}

void message(const char *title, const char *l1, const char *l2) {
    msgTitle = strncpy(msgBuf[0], title, 21);
    msgL1 = l1 ? strncpy(msgBuf[1], l1, 21) : nullptr;
    msgL2 = l2 ? strncpy(msgBuf[2], l2, 21) : nullptr;
    fx::shake(14, 3);
    audio::sfx(Sfx::Deny);
    screen = Scr::Msg;
    t = 0;
}

// ---- Screens ------------------------------------------------------------------
static void moveSel(int d) {
    if (!count) return;
    int s = sel + d;
    if (s < 0) s = 0;
    if (s >= count) s = count - 1;
    if (s == sel) return;
    sel = s;
    if (sel < top) top = sel;
    if (sel >= top + VISIBLE) top = sel - VISIBLE + 1;
    audio::sfx(Sfx::Move);
}

static void listUpdate() {
    if (chgame.justPressed(START_BUTTON)) {
        mountAndScan();
        audio::sfx(stlreader::state() == stlreader::READY ? Sfx::Ready : Sfx::Deny);
        t = 0;
        return;
    }
    if (chgame.repeat(UP_BUTTON)) moveSel(-1);
    if (chgame.repeat(DOWN_BUTTON)) moveSel(1);
    if (chgame.repeat(LEFT_BUTTON)) moveSel(-VISIBLE);
    if (chgame.repeat(RIGHT_BUTTON)) moveSel(VISIBLE);
    if (chgame.justPressed(A_BUTTON) && count) {
        viewer::open(entries[sel]);
    }
}

static void listDraw() {
    stlreader::State st = stlreader::state();
    bool ok = st == stlreader::READY;
    statusBar(ok ? "MODELS" : "NO FS", ok ? LIVE : ALERT);
    cubeIcon(2, 1, ok ? LIVE : ALERT, PANEL);
    readout(stls, LIVE, "STL", "");
    gauge(0, 12, 128, 2, count ? sel + 1 : 0, count, MID);

    if (!ok) {
        alert(40, "NO FS", "LittleFS error or", "not formatted", ALERT);
        keys(120, "START:RETRY");
        return;
    }

    tiny(0, 16, "ROOT", DIM);
    char buf[16];
    fmtStr(fmtInt(buf, count), truncated ? "+ ITEMS" : " ITEMS");
    tinyR(127, 16, buf, DIM);

    if (!count) {
        alert(44, "EMPTY", "No .STL files", "found", MID);
    } else {
        int want = (LIST_TOP + (sel - top) * ROW_H) * 4;
        barY = barY < 0 ? want : barY + (want - barY) / 2;
        int by = (barY + 2) / 4;
        gfx_fillRect(0, by - 1, 124, ROW_H, PANEL);
        brackets(0, by - 1, 124, ROW_H, 3, LIVE);
        for (int i = 0; i < VISIBLE && top + i < count; i++) {
            const Entry &e = entries[top + i];
            int y = LIST_TOP + i * ROW_H;
            bool hi = top + i == sel;
            chip(2, y, 15, e.source == SRC_PROGMEM ? "MEM" : "FS", hi && (t & 8) ? PALE : WIRE_CHIP);
            gfx_text(20, y, e.name, hi ? PALE : WIRE_TEXT);
            
            uint32_t n = e.size > STL_HEADER_BYTES ? (e.size - STL_HEADER_BYTES) / STL_RECORD_BYTES : 0;
            putCount(buf, n);
            tinyR(121, y + 1, buf, hi ? PALE : DIM);
        }
        if (count > VISIBLE) {
            int h = VISIBLE * ROW_H, th = h * VISIBLE / count;
            if (th < 6) th = 6;
            int y = LIST_TOP - 1 + (h - th) * top / (count - VISIBLE);
            for (int r = LIST_TOP - 1; r < LIST_TOP - 1 + h; r += 2) gfx_pixel(126, r, GRID);
            gfx_fillRect(125, y, 3, th, MID);
        }
    }
    gfx_hline(0, 116, 128, GRID);
    keys(120, "A:OPEN START:SCAN");
}

// Задержка на 3 секунды: INTRO_END = 180 (60 fps * 3 sec)
static const uint8_t INTRO_MOUNT = 46, INTRO_END = 180;
static const char NAME[] = "STL VIEWER", SUB[] = "WIREFRAME UPLINK";

static void introUpdate() {
    if (t < INTRO_MOUNT && t > 6 && chgame.justPressedMask()) t = INTRO_MOUNT;
    if (t == INTRO_MOUNT) {
        mountAndScan();
        audio::sfx(stlreader::state() == stlreader::READY ? Sfx::Ready : Sfx::Deny);
    }
    if (t > 22 && t < 22 + 2 * (int)sizeof NAME && !(t & 1)) audio::blip(2600, 4, true);
    // Пропускаем заставку по нажатию ИЛИ если время (180 кадров) вышло
    if (t >= INTRO_END || (t > INTRO_MOUNT + 6 && chgame.justPressedMask())) {
        screen = Scr::List;
        t = 0;
    }
}

static void introCube(int cx, int cy, int r, uint16_t yaw) {
    static const uint16_t PITCH = 0x0E00;           
    int32_t cy_ = stlCos(yaw), sy = stlSin(yaw), cp = stlCos(PITCH), sp = stlSin(PITCH);
    int16_t px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
        int32_t x = i & 1 ? r : -r, y = i & 2 ? r : -r, z = i & 4 ? r : -r;
        int32_t x1 = (x * cy_ - z * sy) >> 14, z1 = (x * sy + z * cy_) >> 14;
        int32_t y1 = (y * cp - z1 * sp) >> 14, z2 = (y * sp + z1 * cp) >> 14;
        px[i] = (int16_t)(cx + x1);
        py[i] = (int16_t)(cy + y1);
        pz[i] = (int16_t)z2;
    }
    uint8_t ea[12], eb[12];
    int n = 0;
    for (int i = 0; i < 8; i++)
        for (int bit = 1; bit < 8; bit <<= 1)
            if (!(i & bit)) { ea[n] = (uint8_t)i; eb[n] = (uint8_t)(i | bit); n++; }
    for (int k = 0; k < 12; k++) {
        int best = k;
        for (int j = k + 1; j < 12; j++)
            if (pz[ea[j]] + pz[eb[j]] > pz[ea[best]] + pz[eb[best]]) best = j;
        uint8_t ta = ea[k], tb = eb[k];
        ea[k] = ea[best]; eb[k] = eb[best]; ea[best] = ta; eb[best] = tb;
        int s = 4 - (pz[ea[k]] + pz[eb[k]]) * 3 / (2 * r);
        if (s < 0) s = 0;
        if (s > STL_SHADES - 1) s = STL_SHADES - 1;
        stlLine(px[ea[k]], py[ea[k]], px[eb[k]], py[eb[k]], (uint8_t)(STL_SHADE0 + s));
    }
}

static void introDraw() {
    stlreader::State st = stlreader::state();
    bool up = t >= INTRO_MOUNT, ok = st == stlreader::READY;
    statusBar(up ? (ok ? "ONLINE" : "NO FS") : "STANDBY", up ? (ok ? LIVE : ALERT) : MID);
    cubeIcon(2, 1, up ? (ok ? LIVE : ALERT) : MID, PANEL);
    readout(t < INTRO_MOUNT ? (uint32_t)t * 100 / INTRO_MOUNT : 100, LIVE, "%", "");
    gauge(0, 12, 128, 2, t < INTRO_MOUNT ? t : INTRO_MOUNT, INTRO_MOUNT, MID);
    if (t < 26) sweep(14 + t * 5);
    if (t >= 8) introCube(64, 42, t < 20 ? (t - 8) + 4 : 16, (uint16_t)(t * 700));
    brackets(6, 18, 116, 50, 5, t < 26 ? GRID : MID);
    typed(64 - 60, 76, NAME, t > 22 ? (t - 22) / 2 : 0, LIVE, 2, t < INTRO_MOUNT && (t & 4));
    typed(64 - 48, 96, SUB, t > 34 ? (t - 34) : 0, DIM);
    if (up) {
        const char *s = ok ? "FILESYSTEM ONLINE" : "LITTLEFS ERROR";
        typed(64 - gfx_textWidth(s) / 2, 108, s, (t - INTRO_MOUNT) / 2, ok ? LIVE : ALERT);
    }
    tiny(0, 122, "V" STLV_VERSION, GRID);
}

static void msgUpdate() {
    if (t > 20 && chgame.justPressedMask()) {
        fx::clear();
        if (stlreader::state() != stlreader::READY) mountAndScan();
        else back();
        screen = Scr::List;
        t = 0;
    }
}

static void msgDraw() {
    statusBar("ALERT", ALERT);
    cubeIcon(2, 1, ALERT, PANEL);
    gauge(0, 12, 128, 2, 1, 1, ALERT);
    alert(36, msgTitle, nullptr, nullptr, ALERT);
    if (msgL1) typed(64 - gfx_textWidth(msgL1) / 2, 74, msgL1, t, PALE);
    if (msgL2) typed(64 - gfx_textWidth(msgL2) / 2, 86, msgL2, t > 10 ? t - 10 : 0, DIM);
    if (t > 20) keys(120, "ANY:BACK");
}

void begin() {
    screen = Scr::Intro;
    t = 0;
}

void frame() {
    switch (screen) {
        case Scr::Intro: introUpdate(); break;
        case Scr::List: listUpdate(); break;
        case Scr::Msg: msgUpdate(); break;
        default: break;
    }
    if (screen == Scr::View) return;                
    if (t < 255) t++;
    fx::update();
    pal::tick();
    audio::update();

    gfx_wait();
    pal::commit();
    gfx_clear(BG);
    switch (screen) {
        case Scr::Intro: introDraw(); break;
        case Scr::List: listDraw(); break;
        default: msgDraw(); break;
    }
    fx::applyShake(0, 127, BG);
    gfx_flushAsync();
}

}  
