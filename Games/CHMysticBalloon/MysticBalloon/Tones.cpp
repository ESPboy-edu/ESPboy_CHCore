#include "Tones.h"
#include <Arduino.h>

namespace mystic { namespace tones {

static bool outputEnabled = false;
volatile uint16_t current_hz = 0;
volatile uint32_t remaining_ms = 0;
static uint32_t last_millis = 0;

void begin(bool on) { 
    outputEnabled = on; 
    current_hz = 0;
    remaining_ms = 0;
}

void end() { 
    current_hz = 0;
    remaining_ms = 0;
}

void setEnabled(bool on) { 
    outputEnabled = on;
    if (!on) {
        end();
    }
}

void tone(uint16_t hz, uint16_t duration) {
    if (!outputEnabled) return;
    
    if (hz == 0) {
        current_hz = 0;
        remaining_ms = 0;
        return;
    }
    
    current_hz = hz;
    if (duration > 0) {
        remaining_ms = duration;
    } else {
        remaining_ms = 0xFFFFFFFF; // Бесконечное звучание
    }
    last_millis = millis();
}

bool playing() { 
    if (current_hz > 0) {
        if (remaining_ms != 0xFFFFFFFF) {
            uint32_t now = millis();
            if (now - last_millis >= remaining_ms) {
                current_hz = 0;
                remaining_ms = 0;
                return false;
            }
        }
        return true;
    }
    return false; 
}

bool enabled() { 
    return outputEnabled; 
}

} }