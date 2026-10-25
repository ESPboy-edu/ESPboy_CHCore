// CHMahjong - casino mahjong solitaire for the CHGame handheld (CH32X035,
// 128x128 ST7735, piezo). A sibling of CHBlackjack and CHChess: the same
// palette, lettering and effects.
//
// Frame loop: logic runs while the previous frame is still going out over
// DMA; drawing waits for it (one framebuffer), then the new frame is sent.
//
// The files, by role:
//   rules        MahjongBoard (the pile, the deal, pairs, chips), Nav (where the
//                D-pad goes): no drawing or sound, host-tested
//   frame        Frame (input, logic, drawing, flush; the felt colours)
//   screens      Screens: title, setup, play with its panels, options
//   presenting   Stage (the pile on screen, glove, flights, sparrow, HUD), Fx (sparkle)
//   drawing      Tile (the tile blitters, run from SRAM)
//   sound        Sounds (effects only: there is no music)
//   saving       Save
//   generated    src/assets (art), src/game/Layouts.* (the layouts): do not edit
//   switches     config.h
#include <ESPboy_CHCore.h>
ESPboyInit myESPboy;

#include "config.h"
#include "Frame.h"

void setup() {
//    Serial.begin(115200);

    myESPboy.begin("CHMahjong");
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
    chgame.setFrameRate(CHMJ_FPS);
}

void loop() {
    frame::run();
}
