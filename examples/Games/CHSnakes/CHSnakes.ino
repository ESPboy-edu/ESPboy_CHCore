// SNAKES & LADDERS - the childhood board game, casino style, for the CHGame
// handheld (CH32X035, 128x128 ST7735, piezo): roll, hop, climb, get eaten.
//
// Frame loop: logic runs while the previous frame is still going out over
// DMA; drawing waits for it (one framebuffer), then the new frame is sent.
//
// The files, by role:
//   rules, no graphics (host-tested)  Layout (the ladders and snakes), Game, Cpu
//   the play screen                   Stage, BoardView (the board drawn), Fx
//   the screens and the frame loop    Screens, Frame
//   sound, saving                     Sounds, Save
//   generated art (tools/assets.py)   src/assets/Assets.*
//   build switches                    config.h

#include <ESPboy_CHCore.h>
ESPboyInit myESPboy;

#include "config.h"
#include "Frame.h"

void setup() {
     myESPboy.begin("CHSnakes");
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
    chgame.setFrameRate(CHSN_FPS);
}

void loop() {
    frame::run();
}
