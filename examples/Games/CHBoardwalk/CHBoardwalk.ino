// BOARDWALK - a casino property game for the CHGame handheld (CH32X035,
// 128x128 ST7735, piezo): roll, buy, build, and tap to outbid the table.
//
// Frame loop: logic runs while the previous frame is still going out over
// DMA; drawing waits for it (one framebuffer), then the new frame is sent.
//
// The files, by role:
//   rules, no graphics (host-tested)  Tiles (the board's data), Game, Cpu, Text
//   the play screen                   Stage, Iso (the board), Cards, MapView, Fx
//   the screens and the frame loop    Screens, Frame
//   sound, saving                     Sounds, Save
//   generated art (tools/assets.py)   src/assets/Assets.*
//   build switches                    config.h
#include <ESPboy_CHCore.h>
#include "config.h"
#include "Frame.h"

ESPboyInit myESPboy;

void setup() {
//    Serial.begin(115200);
    myESPboy.begin("CHBoardwalk");
/*
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     ESPboyTerminalGUI *terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     ESPboyOTA2 *OTA2obj = new ESPboyOTA2(terminalGUIobj);
  } 
*/
    gfx_init();

    chgame.boot();
    gfx_begin(GFX_DIV2, GFX_12BPP);
    frame::begin();
    chgame.setFrameRate(CHBW_FPS);
}

void loop() {
    frame::run();
}
