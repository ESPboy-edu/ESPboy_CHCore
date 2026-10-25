// Vroom: an incremental quarter-mile racer for CHGame.
// Game.*: portable vehicle model and progression.
// App.*: input, screens, sound, and flash saves.
// View.*: code-drawn pixel art and the race dashboard.

#include <ESPboy_CHCore.h>
ESPboyInit myESPboy;

#include "config.h"
#include "App.h"


void setup() {
    myESPboy.begin("CHVroom");
/*
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     ESPboyTerminalGUI *terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     ESPboyOTA2 *OTA2obj = new ESPboyOTA2(terminalGUIobj);
  } 
*/    
    gfx_init(); 
    chgame.boot();
    dbg::begin("VRM1 " VRM_VERSION);
    gfx_begin(GFX_DIV2, GFX_12BPP);
    pal::init();
    app::begin();
    chgame.setFrameRate(VRM_FPS);
}


void loop() {
    dbg::poll();
    if (!chgame.nextFrame()) return;
    dbg::markUpdateStart();
    // Fixed-rate logic; bounded catch-up keeps racing speed stable on slow frames.
    uint8_t ticks=0;
    do {
        chgame.pollButtons();
        app::update();
        audio::update();
    } while (++ticks<3 && chgame.nextFrame());
    gfx_wait();
    app::savePending();
    dbg::markRenderStart();
    app::render();
    dbg::markRenderEnd();
    gfx_flushAsync();
}
