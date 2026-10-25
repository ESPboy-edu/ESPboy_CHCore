// CHChess - isometric casino chess for the CHGame handheld (CH32X035,
// 128x128 ST7735, piezo). Rules and CPU: the ch2k engine from ArduChess by
// Peter Brown (tiberiusbrown), MPL-2.0 (ch2k.hpp).
//
// The files, by role:
//   rules and CPU  ch2k.hpp (the engine), Engine (the game's face on it),
//                  Match (turns, the CPU, verdicts, undo; no graphics)
//   screens        Screens (title, setup, play, options, stats, credits),
//                  Stage (the play screen: camera, gloves, pieces in motion, HUD)
//   drawing        Iso (the isometric board and the flat map), Fx (particles,
//                  banners); the art is generated, src/assets/
//   sound          Sounds
//   saving         Save
//   main loop      Frame; config.h holds the build switches
//
// Frame loop: logic runs while the previous frame is still going out over
// DMA; drawing waits for it (one framebuffer), then the new frame is sent.
// The same frame runs from inside the CPU's search (see Frame.h), so the
// game keeps moving while the engine thinks.
#include <ESPboy_CHCore.h>
#include "config.h"
#include "Frame.h"

ESPboyInit myESPboy;

void setup() {
//    Serial.begin(115200);
    myESPboy.begin("CHChess");
/*    
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     ESPboyTerminalGUI *terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     ESPboyOTA2 *OTA2obj = new ESPboyOTA2(terminalGUIobj);
  } 
*/
    gfx_init();
    
    chgame.boot();
    dbg::begin("CHCS " CHCH_VERSION);     // the debug protocol's hello (CHGAME_DEBUG builds)
    gfx_begin(GFX_DIV2, GFX_12BPP);
    frame::begin();
    chgame.setFrameRate(CHCH_FPS);
}

void loop() {
    frame::run(false);
}
