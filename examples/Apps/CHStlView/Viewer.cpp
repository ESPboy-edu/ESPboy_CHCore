// The viewer: opening a model (the scan) and drawing it, streaming via StlReader
#pragma GCC optimize("Os")
#include "App.h"
#include "StlReader.h"
#include <string.h>
#include <stdlib.h>

using namespace agent;

namespace viewer {

// ---- Tuning ---------------------------------------------------------------------
static const uint32_t SPIN_DEG_PER_S = 40, TURN_DEG_PER_S = 120;
static const uint32_t ZOOM_MS_PER_2X = 1000;           
static const uint32_t PITCH_DEFAULT_DEG = 25, YAW_DEFAULT_DEG = 30;
static const uint32_t HUD_MS = 3000, START_HOLD_MS = 500;
static const uint32_t FULL_PIECE_BLOCKS = 128;
static const uint32_t SLOW_MS = 100, PROGRESS_MS = 150;

// ---- The open model ---------------------------------------------------------------
static char name[16];
static uint32_t fileBlocks;
static StlModel model;
static StlView view;
static uint16_t pointCount;
static uint32_t zoomQ12;                
static bool spin;

// Render scheduling.
static uint32_t fullMs;                 
static bool needFull;                   
static bool hudShown, bannerShown;      
static uint32_t hudUntil, hintUntil, last;
static uint32_t clock;
static int16_t shownX0, shownY0, shownX1, shownY1;   

// Stats, over a second.
static uint32_t statStart, statFrames, statRenderUs;
static uint32_t shownFps;
static bool lastWasDraft, frameMoving;

static uint32_t startDownAt;
static bool startHoldFired;
static bool armed;                      

static bool slowModel() { return fullMs > SLOW_MS; }

// ---- Streaming the file -------------------------------------------------------------
static struct {
    StlFeed feed;
    uint32_t off;                       
    StlRecordFn fn;
} stream;

static void onBlock(const uint8_t *blk, void *) {
    stlFeedBytes(stream.feed, stream.off, blk, 512, stream.fn);
    stream.off += 512;
}

enum : int8_t { STREAM_OK = 0, STREAM_IO = -1, STREAM_ABORT = -2 };

static int8_t streamFile(StlRecordFn fn, bool (*between)(uint32_t done, uint32_t total)) {
    stlFeedBegin(stream.feed, model.tris);
    stream.fn = fn;
    stream.off = 0;
    for (uint32_t b = 0; b < fileBlocks; b += FULL_PIECE_BLOCKS) {
        uint32_t n = fileBlocks - b < FULL_PIECE_BLOCKS ? fileBlocks - b : FULL_PIECE_BLOCKS;
        if (!stlreader::stream(b, n, onBlock, nullptr)) return STREAM_IO;
        if (between && !between(b + n, fileBlocks)) return STREAM_ABORT;
    }
    return STREAM_OK;
}

// ---- Opening a model -----------------------------------------------------------------
static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void scanBar(uint32_t done, uint32_t total) {
    statusBar("ANALYSIS", LIVE);
    cubeIcon(2, 1, LIVE, PANEL);
    readout(total ? (uint32_t)((uint64_t)done * 100 / total) : 0, LIVE, "%", "");
    gauge(0, 12, 128, 2, done, total, LIVE, GRID);
}

static uint32_t scanShownAt;
static bool scanProgress(uint32_t done, uint32_t total) {
    uint32_t now = millis();
    if (now - scanShownAt >= 100 || done == total) {
        scanShownAt = now;
        scanBar(done, total);
        gfx_flushRect(0, 0, GFX_W, 14);
    }
    return true;
}

static void resetView() {
    view.yaw = (uint16_t)(YAW_DEFAULT_DEG * 65536 / 360);
    view.pitch = (uint16_t)(PITCH_DEFAULT_DEG * 65536 / 360);
    zoomQ12 = 256u << 4;
    view.zoomQ8 = 256;
    view.cx = GFX_W / 2;
    view.cy = GFX_H / 2;
}

static void enter() {
    screen = Scr::View;
    uint32_t now = millis();
    clock = 0;
    hudUntil = hintUntil = HUD_MS;
    hudShown = true;                    
    shownX0 = 0; shownY0 = 0; shownX1 = GFX_W - 1; shownY1 = GFX_H - 1;
    needFull = true;
    shownFps = 0;
    statStart = last = now;
    statFrames = statRenderUs = 0;
    startDownAt = 0;
    startHoldFired = false;
    armed = false;                      
    audio::sfx(Sfx::Open);
}

bool open(const Entry& e) {
    // 1. СПАСАЕМ ИМЯ СРАЗУ, до того как начнем парсить модель и писать в union!
    strncpy(name, e.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    int8_t runs = stlreader::openFile(e);
    if (runs < 0) { browser::message("ERROR", name, "cannot read"); return false; }
    if (e.size < STL_HEADER_BYTES) { browser::message("NOT AN STL", name, "file too short"); return false; }
    
    const uint8_t *hdr = stlreader::block0();
    if (!hdr) { browser::message("ERROR", name, "while reading"); return false; }
    
    uint32_t tris = rd32le(hdr + 80);
    uint64_t expect = STL_HEADER_BYTES + (uint64_t)tris * STL_RECORD_BYTES;
    if (expect > e.size) {
        if (memcmp(hdr, "solid", 5) == 0) browser::message("ASCII STL", "Re-export it as", "binary STL");
        else browser::message("NOT AN STL", name, "size doesn't match");
        return false;
    }
    if (!tris) { browser::message("EMPTY STL", name, "0 triangles"); return false; }
    fileBlocks = (uint32_t)((expect + 511) >> 9);

    gfx_wait();
    pal::commit();
    gfx_clear(BG);
    scanBar(0, 1);
    gfx_fillRect(6, 22, 116, 52, PANEL);
    brackets(6, 22, 116, 52, 5, LIVE);
    bool big = gfx_textWidthScaled(name, 2) <= 108;
    if (big) outlined(28, name, LIVE);
    else centred(32, name, LIVE);
    char line[24];
    fmtStr(putCount(line, tris), " TRIANGLES");
    centred(48, line, PALE);
    centred(60, "SCANNING", DIM);
    gfx_flush();

    model.tris = tris;                  
    scanShownAt = millis();
    uint32_t t0 = millis();
    
    // ВЫДЕЛЯЕМ ПАМЯТЬ ДЛЯ ЧЕРНОВИКА ИСКЛЮЧИТЕЛЬНО НА ВРЕМЯ СКАНИРОВАНИЯ
    uint32_t *sampleBuffer = (uint32_t *)malloc(DRAFT_POINTS * 3 * sizeof(uint32_t));
    if (!sampleBuffer) {
        browser::message("MEM ERROR", name, "Not enough RAM");
        return false;
    }
    
    stlScanBegin(tris, sampleBuffer, DRAFT_POINTS);
    
    if (streamFile(stlScanRecord, scanProgress) != STREAM_OK) {
        free(sampleBuffer); // Обязательно освобождаем при ошибке
        browser::message("READ ERROR", name, "while streaming");
        return false;
    }
    uint32_t scanMs = millis() - t0;
    
    // Передаем данные в движок, он упакует их в arena.points
    bool scanOk = stlScanEnd(model, arena.points, &pointCount);
    
    // СКАНИРОВАНИЕ ЗАВЕРШЕНО: ВОЗВРАЩАЕМ ПАМЯТЬ СИСТЕМЕ
    free(sampleBuffer);
    
    if (!scanOk) {
        browser::message("NO SHAPE", name, "all NaN or infinite");
        return false;
    }

    fullMs = scanMs + 8;
    resetView();
    view.mode = STL_XRAY;
    view.ortho = false;
    view.dedup = model.closed;
    spin = !slowModel();
    fx::clear();
    enter();
    return true;
}

// ---- Drawing -------------------------------------------------------------------------
static void drawGizmo(int cx, int cy, int len) {
    int32_t R[3][3];
    stlRotation(R);
    static const uint8_t col[3] = {AXIS_X, AXIS_Y, AXIS_Z};
    uint8_t order[3] = {0, 1, 2};
    for (int i = 0; i < 2; i++)                  
        for (int j = 0; j < 2 - i; j++)
            if (R[2][order[j]] < R[2][order[j + 1]]) {
                uint8_t t = order[j]; order[j] = order[j + 1]; order[j + 1] = t;
            }
    for (int i = 0; i < 3; i++) {
        int a = order[i];
        stlLine(cx, cy, cx + ((R[0][a] * len) >> 14), cy + ((R[1][a] * len) >> 14), col[a]);
    }
}

static char *putMilli(char *p, uint32_t m) {
    if (m >= 100000) return fmtInt(p, (int32_t)((m + 500) / 1000));
    uint32_t t = (m + 50) / 100;
    p = fmtInt(p, (int32_t)(t / 10));
    *p++ = '.';
    *p++ = (char)('0' + t % 10);
    *p = 0;
    return p;
}

static void drawHud(const StlFrameInfo &fi) {
    char line[24], *p;
    if (fi.x1 >= fi.x0) {
        int x0 = fi.x0 - 3, y0 = fi.y0 - 3, x1 = fi.x1 + 3, y1 = fi.y1 + 3;
        if (x0 < 1) x0 = 1;
        if (y0 < 20) y0 = 20;
        if (x1 > 126) x1 = 126;
        if (y1 > 116) y1 = 116;
        if (x1 - x0 > 10 && y1 - y0 > 10) brackets(x0, y0, x1 - x0 + 1, y1 - y0 + 1, 4, MID);
    }
    statusBar(name, LIVE);
    cubeIcon(2, 1, LIVE, frameMoving ? MID : PANEL);
    if (frameMoving && shownFps >= 3) readout(shownFps, LIVE, "FR", "/S");
    else readout(fullMs, LIVE, "MS", "");
    gauge(0, 12, 128, 2, lastWasDraft ? 0 : 1, 1, MID);
    gfx_fillRect(0, 14, GFX_W, 8, BG);
    p = putMilli(line, model.sizeMilli[0]); *p++ = 'x';
    p = putMilli(p, model.sizeMilli[1]); *p++ = 'x';
    putMilli(p, model.sizeMilli[2]);
    tiny(0, 16, line, DIM);
    if (!model.closed) tinyR(127, 16, "OPEN", ALERT);
    else {
        fmtStr(putCount(line, model.tris), " TRI");
        tinyR(127, 16, line, DIM);
    }

    gfx_fillRect(0, GFX_H - 12, GFX_W, 12, BG);
    gfx_hline(0, GFX_H - 12, GFX_W, GRID);
    if ((int32_t)(hintUntil - clock) > 0) {
        keys(GFX_H - 8, "START:SPIN HOLD:LIST");
    } else {
        static const char *const MODES[3] = {"X-RAY", "FRONT", "DRAFT"};
        tabs(GFX_H - 7, MODES, lastWasDraft ? 3 : 2, lastWasDraft ? 2 : view.mode);
        p = fmtInt(line, view.zoomQ8 * 10 / 256 / 10); *p++ = '.';
        p = fmtInt(p, view.zoomQ8 * 10 / 256 % 10);
        fmtStr(p, "X");
        gfx_fillRect(127 - tinyW(line) - 2, GFX_H - 8, tinyW(line) + 3, 7, BG);
        tinyR(127, GFX_H - 7, line, PALE);
    }
    drawGizmo(GFX_W - 9, GFX_H - 23, 7);
}

static bool flushed;                    
static uint32_t progressAt;
static bool renderProgress(uint32_t done, uint32_t total) {
    if (armed && (chgame_readButtons() | chgame.injected)) return false;
    uint32_t now = millis();
    if (now - progressAt < PROGRESS_MS) return true;
    progressAt = now;
    StlFrameInfo fi;
    stlFramePeek(fi);
    gauge(0, 12, 128, 2, done, total, LIVE, GRID);
    int16_t x0 = shownX0 < fi.x0 ? shownX0 : fi.x0, y0 = shownY0 < fi.y0 ? shownY0 : fi.y0;
    int16_t x1 = shownX1 > fi.x1 ? shownX1 : fi.x1, y1 = shownY1 > fi.y1 ? shownY1 : fi.y1;
    if (x1 >= x0) gfx_flushRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    gfx_flushRect(0, 12, GFX_W, 2);
    shownX0 = x0; shownY0 = y0; shownX1 = x1; shownY1 = y1;
    return true;
}

static bool render(bool draft, bool hud) {
    gfx_wait();                         
    pal::commit();
    uint32_t t0 = micros();
    gfx_clear(BG);
    stlFrameBegin(model, view);
    int8_t r = STREAM_OK;
    if (draft) {
        stlDrawDraft(model, arena.points, pointCount);
    } else {
        progressAt = millis();
        r = streamFile(stlDrawRecord, slowModel() ? renderProgress : nullptr);
    }
    StlFrameInfo fi;
    stlFrameEnd(fi);
    uint32_t us = micros() - t0;

    if (r == STREAM_IO) {
        browser::message("READ ERROR", name, "while streaming");
        return true;
    }
    if (r == STREAM_ABORT) return false;
    if (!draft) fullMs = us / 1000 + 4;
    lastWasDraft = draft;

    if (hud) drawHud(fi);
    bool banner = fx::bannerActive();
    fx::drawBanner();

    if (hud || hudShown || banner || bannerShown) {
        gfx_flushAsync();
        flushed = true;
    } else {
        int16_t x0 = shownX0 < fi.x0 ? shownX0 : fi.x0, y0 = shownY0 < fi.y0 ? shownY0 : fi.y0;
        int16_t x1 = shownX1 > fi.x1 ? shownX1 : fi.x1, y1 = shownY1 > fi.y1 ? shownY1 : fi.y1;
        if (x1 >= x0) { gfx_flushRectAsync(x0, y0, x1 - x0 + 1, y1 - y0 + 1); flushed = true; }
    }
    shownX0 = fi.x0; shownY0 = fi.y0; shownX1 = fi.x1; shownY1 = fi.y1;
    hudShown = hud;
    bannerShown = banner;
    statFrames++;
    statRenderUs += us;
    return true;
}

// ---- Input and scheduling ------------------------------------------------------------
static void stats(uint32_t now) {
    if (now - statStart < 1000) return;
    uint32_t dt = now - statStart;
    shownFps = (statFrames * 1000 + dt / 2) / dt;
    if (shownFps > STLV_FPS) shownFps = STLV_FPS;   
    statStart = now;
    statFrames = statRenderUs = 0;
}

static void modeBanner(const char *s) {
    fx::banner(s, fx::B_GREEN, 64, 50);
    audio::sfx(Sfx::Mode);
}

static bool input(uint32_t now, uint32_t dt, bool &moving) {
    if (chgame.justPressed(START_BUTTON)) { startDownAt = now; startHoldFired = false; }
    if (startDownAt && chgame.pressed(START_BUTTON) && !startHoldFired && now - startDownAt >= START_HOLD_MS) {
        startHoldFired = true;
        fx::clear();
        audio::sfx(Sfx::Back);
        browser::back();
        return false;
    }
    if (chgame.justReleased(START_BUTTON)) {
        if (!startHoldFired && startDownAt) {
            spin = !spin;
            needFull = true;
            audio::sfx(Sfx::Spin);
        }
        startDownAt = 0;
    }
    if (chgame.justPressed(SELECT_BUTTON)) {
        view.mode = (uint8_t)((view.mode + 1) % STL_MODES);
        modeBanner(view.mode == STL_FRONT ? "FRONT" : "X-RAY");
        needFull = true;
    }

    int32_t turn = (int32_t)(TURN_DEG_PER_S * 65536u / 360u * dt / 1000u);
    bool l = chgame.pressed(LEFT_BUTTON), r = chgame.pressed(RIGHT_BUTTON);
    bool u = chgame.pressed(UP_BUTTON), d = chgame.pressed(DOWN_BUTTON);
    if (l || r || u || d) {
        spin = false;
        moving = true;
        if (l) view.yaw = (uint16_t)(view.yaw - turn);
        if (r) view.yaw = (uint16_t)(view.yaw + turn);
        if (u) view.pitch = (uint16_t)(view.pitch + turn);
        if (d) view.pitch = (uint16_t)(view.pitch - turn);
    }

    bool a = chgame.pressed(A_BUTTON), b = chgame.pressed(B_BUTTON);
    if (a && b) {
        if (chgame.justPressed(A_BUTTON | B_BUTTON)) {
            resetView();
            modeBanner("RESET");
            needFull = true;
        }
    } else if (a || b) {
        uint32_t step = (zoomQ12 * dt / ZOOM_MS_PER_2X * 710u) >> 10;
        if (a) zoomQ12 += step ? step : 1;
        else zoomQ12 -= step ? step : 1;
        if (zoomQ12 < (uint32_t)STL_ZOOM_MIN << 4) zoomQ12 = (uint32_t)STL_ZOOM_MIN << 4;
        if (zoomQ12 > (uint32_t)STL_ZOOM_MAX << 4) zoomQ12 = (uint32_t)STL_ZOOM_MAX << 4;
        view.zoomQ8 = (uint16_t)(zoomQ12 >> 4);
        moving = true;
    }
    return true;
}

void frame() {
    uint32_t real = millis(), dt = real - last;
    last = real;
    if (dt < 1000 / STLV_FPS) dt = 1000 / STLV_FPS;
    if (dt > 100) dt = 100;             
    uint32_t now = clock += dt;

    if (chgame.buttons()) hudUntil = now + HUD_MS;
    bool moving = false;

    if (!armed) armed = !chgame.buttons();
    if (armed && !input(now, dt, moving)) return;
    if (spin) {
        view.yaw = (uint16_t)(view.yaw + SPIN_DEG_PER_S * 65536u / 360u * dt / 1000u);
        moving = true;
    }

    fx::update();
    pal::tick();
    audio::update();

    bool banner = fx::bannerActive();
    if (banner) moving = true;
    if (bannerShown && !banner) needFull = true;
    bool hud = (int32_t)(hudUntil - now) > 0 || (slowModel() && !moving);
    if (hud != hudShown) needFull = true;

    frameMoving = moving;
    flushed = false;
    if (moving) {
        bool draft = slowModel();
        if (render(draft, hud)) needFull = draft;
    } else if (needFull) {
        if (render(false, hud)) needFull = false;
    }
#ifdef CHSIM
    if (!flushed && screen == Scr::View) { gfx_wait(); gfx_flushRectAsync(0, 0, 2, 1); }
#endif
    stats(millis());
}

char *status(char *p) {
    p = fmtStr(p, "VIEW ");
    p = fmtStr(p, name);
    p = fmtInt(fmtStr(p, " tris="), (int32_t)model.tris);
    p = fmtInt(fmtStr(p, " closed="), model.closed);
    p = fmtInt(fmtStr(p, " blocks="), (int32_t)fileBlocks);
    p = fmtInt(fmtStr(p, " full_ms="), (int32_t)fullMs);
    p = fmtInt(fmtStr(p, " fps="), (int32_t)shownFps);
    p = fmtInt(fmtStr(p, " mode="), view.mode);
    p = fmtInt(fmtStr(p, " draft="), lastWasDraft);
    return fmtInt(fmtStr(p, " zoom="), view.zoomQ8);
}

}  // namespace viewer// The viewer: opening a model (the scan) and drawing it, streaming via StlReader
