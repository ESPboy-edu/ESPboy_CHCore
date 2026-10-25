// Space Cab for CHGame: Vampirics and filmote's Pokitto game (a tribute to
// the C64's Space Taxi), ported from its C++ (src/game/: the game's own
// sources, changed only where CHGame needs it; src/game/Pokitto.* stands in
// for the Pokitto library).
//
// The Pokitto showed a 110x88 screen at 60 frames a second; it is drawn 1:1
// in the middle of CHGame's 128x128 panel, and only that rectangle goes to
// the panel each frame.
//
// The buttons: A is the thruster, B the landing gear, the pad steers. START
// is the Pokitto's C (pause). START held 3 s goes back to the CHGame menu.

#include <ESPboy_CHCore.h>
ESPboyInit myESPboy;

#include "src/game/SpaceCab.h"
#include "src/game/Sounds.h"

#define SC_VERSION "1.0"

using PC = Pokitto::Core;

static Game game;
static GameCookie cookie;

// The Pokitto's palettePico, as RGB565.
static const PROGMEM uint16_t PALETTE[16] = {
    0x0000, 0x194A, 0x792A, 0x042A, 0xAA86, 0x5AA9, 0xC618, 0xFF9D,
    0xF809, 0xFD00, 0xFF64, 0x0726, 0x2D7F, 0x83B3, 0xFBB5, 0xFE75,
};

void sc_cookieFlush();

// START held 3 s: back to the CHGame menu, in real time.
static uint32_t startDown;
static void startHold() {
    if (chgame.pressed(START_BUTTON)) {
        uint32_t now = millis();
        if (!startDown) startDown = now ? now : 1;
        else if (now - startDown >= 3000) chgame.exitToMenu();
    } else {
        startDown = 0;
    }
}

static void flushScreen() {
    gfx_flushRectAsync(Pokitto::SCREEN_X, Pokitto::SCREEN_Y, Pokitto::Display::width, Pokitto::Display::height);
}

// The game's own loops (the title's pads): PD::update(), PC::update(), PC::wait().
void sc_present() {
    flushScreen();
    gfx_wait();
}

void sc_poll() {
    chgame.pollButtons();
    startHold();
    PC::buttons.update();
    audio::update();
}

void sc_wait(uint16_t ms) {
    uint32_t t = millis();
    while (millis() - t < ms) dbg::poll();
}

void setup() {
    myESPboy.begin("CHSpaceCab");
/*
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     ESPboyTerminalGUI *terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     ESPboyOTA2 *OTA2obj = new ESPboyOTA2(terminalGUIobj);
  } 
*/    
    gfx_init(); 
  
    chgame.boot();
    chgame.startExits = false;          // timed in real time: startHold()
    dbg::begin("CSPC " SC_VERSION);
    gfx_begin(GFX_DIV2, GFX_12BPP);
    gfx_setPalette(PALETTE, 16);
    gfx_clear(0);
    gfx_flush();
    sounds_begin();
    chgame.setFrameRate(60);
    cookie.begin("SPACECAB", sizeof(cookie), (char *)&cookie);
    if (cookie.initialised != COOKIE_INITIALISED) cookie.initialise();
    game.setup(&cookie);
}

void loop() {
    dbg::poll();
    if (!chgame.nextFrame()) return;

    dbg::markUpdateStart();
    chgame.pollButtons();
    startHold();
    PC::buttons.update();
    PC::frameCount++;

    dbg::markRenderStart();
    gfx_wait();
    sc_cookieFlush();                   // the high scores, between frames
    game.loop();
    audio::update();
    dbg::markRenderEnd();
    flushScreen();
}
