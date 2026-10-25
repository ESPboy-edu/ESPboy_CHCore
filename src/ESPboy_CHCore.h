#pragma once

/**
 * @file ESPboy_CHCore.h
 * @brief Universal Game Engine for ESPboy based on the CHGame architecture.
 * 
 * Include this single header in your main .ino sketch to access 
 * the entire ESPboy game framework (Graphics, Input, Audio, Save, Math).
 * Casino-specific logic (like Palette and Sizzle particles) is intentionally 
 * excluded from this core and should reside within the specific game's source code.
 */

// ---------------------------------------------------------------------------
// 1. ESPboy Hardware Abstraction
// ---------------------------------------------------------------------------
#include "lib/ESPboyInit.h"
#include "lib/ESPboyLED.h"
#include "lib/ESPboyMCP.h"

// ---------------------------------------------------------------------------
// 2. Low-level Graphics Core (Chunk rendering & DMA-like async transfer)
// ---------------------------------------------------------------------------
#include "CHGfx.h"

// ---------------------------------------------------------------------------
// 3. Engine Modules
// ---------------------------------------------------------------------------
#include "CHGame.h"             // Core game loop, frame pacing, and input handling
#include "chgame/RamFunc.h"     // Memory section macros (IRAM_ATTR logic)
#include "chgame/Input.h"       // Button definitions and states
#include "chgame/Draw.h"        // Geometry, span sprites, and 3x5 font rendering
#include "chgame/Mask.h"        // 1bpp masks for outlined/shadowed text and logos
#include "chgame/Fx.h"          // Easing curves, RNG, and screen shake math
#include "chgame/Audio.h"       // Background music and sound effects sequencer
#include "chgame/Save.h"        // Persistent EEPROM save state management
#include "chgame/Fmt.h"         // Lightweight string and number formatting (no printf)

// ---------------------------------------------------------------------------
// 4. Global Instances & Initialization
// ---------------------------------------------------------------------------

/// @brief Global ESPboy hardware instance
extern ESPboyInit myESPboy;

/// @brief Global game controller instance
extern CHGame chgame;

/**
 * @brief Initializes the dynamic double buffers for the chunk rendering pipeline.
 * @details MUST be called in setup() before drawing anything, preferably after 
 * shutting down WiFi to ensure enough contiguous free Heap memory.
 */
extern void gfx_init();