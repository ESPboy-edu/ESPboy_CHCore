#include <Arduino.h>
#include "Input.h"
#include "lib/ESPboyInit.h"

extern ESPboyInit myESPboy;
CHGame chgame;

// Translate ESPboy PAD_ constants to CHGame *_BUTTON constants
uint8_t chgame_readButtons() {
    uint8_t keys = myESPboy.getKeys();
    uint8_t m = 0;
    
    if (keys & PAD_ACT)   m |= A_BUTTON;
    if (keys & PAD_ESC)   m |= B_BUTTON;
    if (keys & PAD_UP)    m |= UP_BUTTON;
    if (keys & PAD_DOWN)  m |= DOWN_BUTTON;
    if (keys & PAD_LEFT)  m |= LEFT_BUTTON;
    if (keys & PAD_RIGHT) m |= RIGHT_BUTTON;
    
    // ESPboy has extra side buttons (LFT, RGT), map them to SELECT and START
    if (keys & PAD_LFT)   m |= SELECT_BUTTON; 
    if (keys & PAD_RGT)   m |= START_BUTTON;  
    
    return m;
}

void CHGame::boot() {
    // Hardware button initialization (MCP23017) is already done in myESPboy.begin()
    cur = prev = chgame_readButtons();
}

void CHGame::setFrameRate(uint8_t fps) {
    period = 1000000u / fps;
    next = micros() + period;
}

bool CHGame::nextFrame() {
    if (lockstep >= 0) {
        if (lockstep == 0) return false;
        lockstep--;
        frameCount++;
        return true;
    }
    uint32_t now = micros();
    if ((int32_t)(now - next) < 0) return false;
    next += period;
    
    // Resync if we fell too far behind (e.g. after a debug pause or flash save)
    if ((int32_t)(now - next) > (int32_t)(3 * period)) next = now + period;
    frameCount++;
    return true;
}

void CHGame::pollButtons() {
    prev = cur;
    cur = (uint8_t)(chgame_readButtons() | injected);
    for (uint8_t i = 0; i < 8; i++) {
        if (cur & (1u << i)) { if (held[i] < 0xFFFF) held[i]++; }
        else held[i] = 0;
    }
    
    // START button (PAD_RGT here) held for 3 seconds (180 frames at 60fps) -> exit to menu
    if (startExits && held[6] >= 180) chgame_exitToMenu();
}

void chgame_exitToMenu() {
    // Just restart the ESPboy. 
    // If the OTA2 bootloader is used, it will intercept the boot process.
    ESP.restart(); 
    for (;;) { }
}

bool CHGame::repeat(uint8_t b, uint8_t delay, uint8_t rate) const {
    for (uint8_t i = 0; i < 8; i++) {
        if (!(b & (1u << i))) continue;
        uint16_t h = held[i];
        if (h == 1) return true;
        if (h > delay && ((h - delay) % rate) == 0) return true;
    }
    return false;
}