#include <Arduino.h>
#include "Rumble.h"

namespace rumble {
    static uint16_t left;
    static uint8_t duty, importance;
    static bool enabled;
    static uint32_t previous;

    static void output(uint8_t strength) {
        // Если к ESPboy подключен вибромотор, раскомментируйте строку ниже и укажите пин
        // analogWrite(D8, strength); 
    }

    void stop() {
        left = duty = importance = 0;
        output(0);
    }

    void begin() {
        previous = millis(); 
        stop();
        enabled = true;
    }

    void setEnabled(bool on) { 
        enabled = on; 
        if (!on) stop(); 
    }

    void pulse(uint8_t strength, uint16_t ms, uint8_t priority) {
        update();
        if (!enabled || (left && priority < importance)) return;
        if (!strength || !ms) { stop(); return; }
        if (ms > MAX_PULSE_MS) ms = MAX_PULSE_MS;
        
        importance = priority; 
        duty = strength; 
        left = ms;
        output(strength);
    }

    void play(Effect effect) {
        static const uint8_t strengths[] = {120, 210, 175, 150, 180, 240, 200};
        static const uint8_t lengths[] = {35, 70, 55, 90, 95, 240, 180};
        unsigned e = unsigned(effect);
        if (e < sizeof(strengths)) pulse(strengths[e], lengths[e], e >= 5 ? 2 : e ? 1 : 0);
    }

    void update() {
        uint32_t now = millis();
        uint32_t elapsed = now - previous; 
        previous = now;
        
        if (left) { 
            if (elapsed >= left) stop(); 
            else left -= elapsed; 
        }
    }

    uint8_t level() { update(); return duty; }
    uint16_t remaining() { update(); return left; }
}