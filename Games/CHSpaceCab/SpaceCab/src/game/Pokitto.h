// The part of the Pokitto library Space Cab uses, on CHGame (Pokitto.cpp).
//
// Space Cab runs in the Pokitto's MODE_FAST_16COLOR: a 110x88 screen in 16
// colours (the PICO-8 palette), shown at 2x on the Pokitto. CHGame's panel is
// 128x128, so the 110x88 screen is drawn 1:1 at (SCREEN_X, SCREEN_Y) in the
// middle of it, and every call below clips to it, as the Pokitto clips to its
// screen. The drawing calls do what the Pokitto library's do, quirks and all
// (drawFastHLine(x,y,1) draws nothing; fillRect(x,y,w,h) fills h+1 rows;
// text paints its background unless that is the invisible colour ...).
//
// Buttons: the Pokitto's A, B, C and pad are CHGame's A, B, START and pad,
// counted as the Pokitto counts them (pressed: the first frame; repeat(b,n):
// every n frames while held). PC::update() / PD::update() / PC::wait() show
// the frame and wait, for the few places the game loops on its own.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>

#define BTN_LEFT   0
#define BTN_UP     1
#define BTN_RIGHT  2
#define BTN_DOWN   3
#define BTN_A      4
#define BTN_B      5
#define BTN_C      6
#define NUM_BTN    7

#define DEC 10

namespace Pokitto {

static const int SCREEN_X = 10, SCREEN_Y = 20;     // where the 110x88 screen sits on the panel (x even: whole bytes)

class Display {
public:
    static const int16_t width = 110, height = 88;
    static uint8_t color, bgcolor, invisiblecolor;
    static int16_t cursorX, cursorY;
    static int8_t adjustCharStep, adjustLineStep;
    static bool persistence, textWrap;

    static void setColor(uint8_t c) { color = c & 15; }
    static void setColor(uint8_t c, uint8_t bc) { color = c & 15; bgcolor = bc & 15; }
    static void setInvisibleColor(uint8_t c) { invisiblecolor = c; }
    static void setCursor(int16_t x, int16_t y) { cursorX = x; cursorY = y; }

    static void clear();
    static void fillScreen(uint16_t c);
    static void drawPixel(int16_t x, int16_t y);
    static void drawPixel(int16_t x, int16_t y, uint8_t col);
    static void drawFastHLine(int16_t x, int16_t y, int16_t w);
    static void drawFastVLine(int16_t x, int16_t y, int16_t h);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h);
    static void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap);

    static void write(uint8_t c);
    static void print(const char *s);
    static void print(char c) { write((uint8_t)c); }
    static void print(unsigned char n, int base = DEC) { print((unsigned long)n, base); }
    static void print(int n, int base = DEC) { print((long)n, base); }
    static void print(unsigned int n, int base = DEC) { print((unsigned long)n, base); }
    static void print(long n, int base = DEC);
    static void print(unsigned long n, int base = DEC);

    static void update();           // show the frame now (inside the game's own loops)

    static void drawColumn(int x, int sy, int ey);
    static void drawRow(int x0, int x1, int y);
};

class Buttons {
public:
    uint8_t states[NUM_BTN];
    void pollButtons() {}           // (read once a frame, by update())
    void update();                  // the frame's buttons, counted as the Pokitto counts them
    bool pressed(uint8_t b) const { return states[b] == 1; }
    bool released(uint8_t b) { if (states[b] == 0xFF) { states[b] = 0; return true; } return false; }
    bool held(uint8_t b, uint8_t time) const { return states[b] == time + 1; }
    bool repeat(uint8_t b, uint8_t period) const {
        if (period <= 1) return states[b] != 0xFF && states[b];
        return states[b] != 0xFF && states[b] % period == 1;
    }
    bool aBtn() const;
    bool bBtn() const;
    bool cBtn() const;
    bool leftBtn() const;
    bool rightBtn() const;
    bool upBtn() const;
    bool downBtn() const;
};

class Sound {};                    // (unused: the game names it)

class Core {
public:
    static Buttons buttons;
    static uint32_t frameCount;
    static bool update();           // inside the game's own loops: the buttons and the sound
    static void wait(uint16_t ms);  // ... and time passing
};

// Pokitto::Cookie: the game's saved data (GameCookie), in CHGame's flash save.
class Cookie {
public:
    void begin(const char *name, int size, char *ptr);
    void saveCookie();                      // written between frames (sc_cookieFlush)
};

}  // namespace Pokitto

// The few LibAudio notes the title screen plays (a square wave).
#define WSQUARE 1
namespace Audio {

struct Note {
    uint8_t n;
    uint16_t ms = 50;
    explicit Note(int note) : n((uint8_t)note) {}
    Note &wave(int) { return *this; }
    Note &duration(uint16_t d) { ms = d; return *this; }
    Note &volume(int) { return *this; }
    void play(int channel = 0);
};
}  // namespace Audio
