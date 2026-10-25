// The Pokitto library's calls Space Cab uses, on CHGame (see Pokitto.h).
#include <CHGame.h>
#include "Pokitto.h"

extern const uint8_t font3x5[];

namespace Pokitto {

uint8_t Display::color = 5, Display::bgcolor = 0, Display::invisiblecolor = 14;
int16_t Display::cursorX, Display::cursorY;
int8_t Display::adjustCharStep = 1, Display::adjustLineStep = 1;
bool Display::persistence = true, Display::textWrap = false;
Buttons Core::buttons;
uint32_t Core::frameCount;

// A pixel of the 110x88 screen (already clipped).
static inline void raw(int x, int y, uint8_t c) {
    x += SCREEN_X;
    y += SCREEN_Y;
    uint8_t *p = gfx_fb + y * GFX_FB_STRIDE + (x >> 1);
    *p = (x & 1) ? (uint8_t)((*p & 0x0F) | (c << 4)) : (uint8_t)((*p & 0xF0) | c);
}

// The screen's rows, filled with one colour.
static void fillScreenRows(uint8_t c) {
    uint8_t both = (uint8_t)(c | (c << 4));
    for (int y = 0; y < Display::height; y++)
        memset(gfx_fb + (y + SCREEN_Y) * GFX_FB_STRIDE + SCREEN_X / 2, both, Display::width / 2);
}

void Display::clear() {
    fillScreenRows(bgcolor & 15);
    setCursor(0, 0);
}

void Display::fillScreen(uint16_t c) { fillScreenRows(c & 15); }

void Display::drawPixel(int16_t x, int16_t y) {
    if ((uint16_t)x >= width || (uint16_t)y >= height) return;
    raw(x, y, color);
}

void Display::drawPixel(int16_t x, int16_t y, uint8_t col) {
    if (col == invisiblecolor) return;
    if ((uint16_t)x >= width || (uint16_t)y >= height) return;
    raw(x, y, col & 15);
}

void Display::drawColumn(int x, int sy, int ey) {
    if (x < 0 || x >= width) return;
    if (sy > ey) { int t = sy; sy = ey; ey = t; }
    if (ey >= height) ey = height - 1;
    if (sy < 0) sy = 0;
    if (sy == ey) return;
    for (int y = sy; y <= ey; y++) raw(x, y, color);
}

void Display::drawRow(int x0, int x1, int y) {
    if (y < 0 || y >= height) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x1 >= width) x1 = width - 1;
    if (x0 < 0) x0 = 0;
    if (x0 == x1) return;
    for (int x = x0; x <= x1; x++) raw(x, y, color);
}

void Display::drawFastVLine(int16_t x, int16_t y, int16_t h) {
    if (h < 0) { y += h; h = -h; }
    drawColumn(x, y, y + h);
}

void Display::drawFastHLine(int16_t x, int16_t y, int16_t w) {
    if (w < 0) { x += w; w = -w; }
    drawRow(x, x + w - 1, y);
}

void Display::drawRect(int16_t x0, int16_t y0, int16_t w, int16_t h) {
    drawColumn(x0, y0, y0 + h);
    drawColumn(x0 + w, y0, y0 + h);
    drawRow(x0, x0 + w, y0);
    drawRow(x0, x0 + w, y0 + h);
}

void Display::fillRect(int16_t x0, int16_t y0, int16_t w, int16_t h) {
    int x, y, x1 = x0 + w, y1 = y0 + h;
    if ((x0 < 0 && x1 < 0) || (x0 >= width && x1 >= width)) return;
    if ((y0 < 0 && y1 < 0) || (y0 >= height && y1 >= height)) return;
    if (x0 > x1) { x = x1; x1 = x0; } else x = x0;
    if (y0 > y1) { y = y1; y1 = y0; } else y = y0;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    for (; x < x1; x++) drawColumn(x, y, y1);
}

// A Pokitto 4-bit bitmap: width, height, then rows of (width+1)/2 bytes, the
// left pixel in the high nibble; the invisible colour is left out.
RAMFUNC(pokBitmap) void Display::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap) {
    int w = bitmap[0], h = bitmap[1];
    if (y < -h || y >= height || x < -w || x >= width) return;
    const uint8_t *data = bitmap + 2;
    int stride = (w + 1) >> 1;
    int c0 = x < 0 ? -x : 0, c1 = x + w > width ? width - x : w;
    int r0 = y < 0 ? -y : 0, r1 = y + h > height ? height - y : h;
    uint8_t inv = invisiblecolor;
    if (!((x + SCREEN_X) & 1) && !(c0 & 1)) {
        // Even: a bitmap byte is a screen byte (its nibbles swapped).
        int b0 = c0 >> 1, b1 = (c1 + 1) >> 1;      // bytes; the last may be half (c1 odd)
        bool half = c1 & 1;
        for (int r = r0; r < r1; r++) {
            const uint8_t *src = data + r * stride;
            uint8_t *d = gfx_fb + (y + r + SCREEN_Y) * GFX_FB_STRIDE + ((x + SCREEN_X) >> 1);
            for (int k = b0; k < b1; k++) {
                uint8_t b = src[k];
                uint8_t left = b >> 4, right = b & 15;
                bool lv = left != inv, rv = right != inv && !(half && k == b1 - 1);
                if (lv && rv) d[k] = (uint8_t)(left | (right << 4));
                else if (lv) d[k] = (uint8_t)((d[k] & 0xF0) | left);
                else if (rv) d[k] = (uint8_t)((d[k] & 0x0F) | (right << 4));
            }
        }
        return;
    }
    if (!(c0 & 1)) {
        // Odd: a bitmap byte's left pixel is a screen byte's high nibble, its
        // right pixel the next byte's low nibble.
        int b0 = c0 >> 1, b1 = (c1 + 1) >> 1;
        bool half = c1 & 1;
        for (int r = r0; r < r1; r++) {
            const uint8_t *src = data + r * stride;
            uint8_t *d = gfx_fb + (y + r + SCREEN_Y) * GFX_FB_STRIDE + ((x + SCREEN_X) >> 1);
            for (int k = b0; k < b1; k++) {
                uint8_t b = src[k];
                uint8_t left = b >> 4, right = b & 15;
                if (left != inv) d[k] = (uint8_t)((d[k] & 0x0F) | (left << 4));
                if (right != inv && !(half && k == b1 - 1)) d[k + 1] = (uint8_t)((d[k + 1] & 0xF0) | right);
            }
        }
        return;
    }
    for (int r = r0; r < r1; r++) {
        const uint8_t *src = data + r * stride;
        int sy = y + r + SCREEN_Y;
        uint8_t *row = gfx_fb + sy * GFX_FB_STRIDE;
        for (int i = c0; i < c1; i++) {
            uint8_t b = src[i >> 1];
            uint8_t c = (i & 1) ? (b & 15) : (b >> 4);
            if (c == inv) continue;
            int sx = x + i + SCREEN_X;
            uint8_t *p = row + (sx >> 1);
            *p = (sx & 1) ? (uint8_t)((*p & 0x0F) | (c << 4)) : (uint8_t)((*p & 0xF0) | c);
        }
    }
}

// Display::bufferChar: the glyph's columns, its background painted unless
// that is the invisible colour.
static int bufferChar(int16_t x, int16_t y, uint16_t index) {
    const uint8_t *font = font3x5;
    uint8_t fontW = font[0], fontH = font[1];
    const uint8_t *bitmap = font + 4 + index * (1 + fontW);
    int charW = *bitmap++;
    bool drawBg = Display::bgcolor != Display::invisiblecolor;
    bool drawFg = Display::color != Display::invisiblecolor;
    for (int i = 0; i < charW; ++i) {
        uint8_t bits = *bitmap++;
        for (int j = 0; j < fontH; ++j) {
            bool on = bits & 1;
            int px = x, py = y + j;
            if ((on ? drawFg : drawBg) && (uint16_t)px < Display::width && (uint16_t)py < Display::height)
                raw(px, py, on ? Display::color : Display::bgcolor);
            bits >>= 1;
        }
        x++;
    }
    return charW + Display::adjustCharStep;
}

void Display::write(uint8_t c) {
    const uint8_t *font = font3x5;
    int charstep = 0;
    if (font[3] && c >= 97) c -= 32;
    switch (c) {
    case '\0':
        break;
    case '\n':
        cursorX = 0;
        if (cursorY > height - 2 * font[1]) cursorY = 0;
        else cursorY += font[1] + adjustLineStep;
        break;
    case 13:
        cursorX = 0;
        break;
    default:
        if (c > 127) c = '?';
        if (cursorX >= (width - font[0])) {
            cursorX = 0;
            if (!textWrap) return;
            if (cursorY > height - 2 * font[1]) cursorY = 0;
            else cursorY += font[1] + adjustLineStep;
        }
        charstep = bufferChar(cursorX, cursorY, (uint16_t)(c - font[2]));
        if (c == ' ' && adjustCharStep) charstep = (charstep >> 1) + 1;
        cursorX += charstep;
    }
}

void Display::print(const char *s) {
    while (*s) write((uint8_t)*s++);
}

void Display::print(unsigned long n, int base) {
    char buf[12];
    char *p = buf + sizeof buf - 1;
    *p = 0;
    if (base < 2) base = 10;
    do { unsigned d = n % base; *--p = (char)(d < 10 ? '0' + d : 'A' + d - 10); n /= base; } while (n);
    print(p);
}

void Display::print(long n, int base) {
    if (n < 0 && base == 10) { write('-'); print((unsigned long)-n, base); }
    else print((unsigned long)n, base);
}

// ---------------------------------------------------------------------------

static bool down(uint8_t mask) { return chgame.pressed(mask); }
bool Buttons::aBtn() const { return down(A_BUTTON); }
bool Buttons::bBtn() const { return down(B_BUTTON); }
bool Buttons::cBtn() const { return down(START_BUTTON); }
bool Buttons::leftBtn() const { return down(LEFT_BUTTON); }
bool Buttons::rightBtn() const { return down(RIGHT_BUTTON); }
bool Buttons::upBtn() const { return down(UP_BUTTON); }
bool Buttons::downBtn() const { return down(DOWN_BUTTON); }

void Buttons::update() {
    bool held[NUM_BTN] = { leftBtn(), upBtn(), rightBtn(), downBtn(), aBtn(), bBtn(), cBtn() };
    for (uint8_t i = 0; i < NUM_BTN; i++) {
        if (held[i]) {
            states[i]++;
            if (states[i] == 0xFF) states[i] = 2;
        } else {
            if (states[i] == 0) continue;
            if (states[i] == 0xFF) states[i] = 0;
            else states[i] = 0xFF;
        }
    }
}

}  // namespace Pokitto

// The sketch's (SpaceCab.ino): show the frame and wait for the next; poll.
void sc_present();
void sc_poll();
void sc_wait(uint16_t ms);

namespace Pokitto {

void Display::update() { sc_present(); }
bool Core::update() { sc_poll(); return true; }
void Core::wait(uint16_t ms) { sc_wait(ms); }

// The cookie: the whole GameCookie (the base class has no fields of its own).
static char *s_cookie;
static int s_cookieSize;
static bool s_cookieDirty;
static const uint32_t COOKIE_MAGIC = save::magic("CSPC");

void Cookie::begin(const char *, int size, char *ptr) {
    s_cookie = ptr;
    s_cookieSize = size;
    const void *p = save::read(COOKIE_MAGIC, 1, (uint16_t)size);
    if (p) memcpy(ptr, p, size);
}

void Cookie::saveCookie() { s_cookieDirty = true; }

}  // namespace Pokitto

// Between gfx_wait() and the next flush (the save uses the chunk scratch).
void sc_cookieFlush() {
    using namespace Pokitto;
    if (!s_cookieDirty || !s_cookie) return;
    if (save::available()) {
        memcpy(save::buffer(), s_cookie, s_cookieSize);
        save::write(COOKIE_MAGIC, 1, (uint16_t)s_cookieSize);
    }
    s_cookieDirty = false;
}

// LibAudio's note number as a pitch: (the title's pad noises) moved up into
// the piezo's range.
void Audio::Note::play(int) {
    uint32_t hz = 440;
    int semis = (int)n - 69 + 36;                  // three octaves up
    for (; semis >= 12; semis -= 12) hz *= 2;
    for (; semis < 0; semis += 12) hz /= 2;
    static const PROGMEM uint16_t step[12] = { 1000, 1059, 1122, 1189, 1260, 1335, 1414, 1498, 1587, 1682, 1782, 1888 };
    hz = hz * step[semis] / 1000;
    audio::blip((uint16_t)hz, ms);
}
