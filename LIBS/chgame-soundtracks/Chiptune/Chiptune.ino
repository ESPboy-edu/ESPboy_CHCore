// ATMlib soundtrack player for CHGame. Audio owns TIM1/PB10 and TIM3.

#include <ESPboy_CHCore.h>
ESPboyInit myESPboy;

#include "Synth.h"
#include "src/Songs.h"

unsigned selected = 0;
bool melodyOnly = false;
#ifdef CHSIM
uint32_t simulatedAt = 0;
#endif

static void selectSong(int direction) {
    selected = (selected + SONG_COUNT + direction) % SONG_COUNT;
    chip::play(selected);
}

#if CHGAME_DEBUG
static bool debugState(char command, const char *) {
    if (command != 'H') return false;
    char state[96];
    snprintf(state, sizeof state, "CHIPSTATE %u %u %u %u %lu\n", selected,
             unsigned(chip::paused()), chip::volume(), chip::muteMask(),
             (unsigned long)chip::ticks());
    dbg::print(state);
    return true;
}
#endif

void setup() {
    myESPboy.begin("ATMlib CHiptune");
/*
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     ESPboyTerminalGUI *terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     ESPboyOTA2 *OTA2obj = new ESPboyOTA2(terminalGUIobj);
  } 
*/    
    gfx_init(); 

    chgame.boot();
    dbg::begin("CHIP 1.1");
#if CHGAME_DEBUG
    dbg::hook = debugState;
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    pal::init();
    chgame.setFrameRate(30);
    chip::begin();
}

static void drawPlayer() {
    gfx_clear(INK);
    gfx_fillRect(0, 0, 128, 15, FELT_DK);
    text35(5, 5, "ATM / SOUNDTRACKS", GOLD);
    char number[8];
    fmtInt(number, selected + 1);
    text35(112, 5, number, WHITE);
    number[0] = '/';
    fmtInt(number + 1, SONG_COUNT);
    text35(118, 5, number, GOLD);
    const Song &s = SONGS[selected];
    text35(5, 21, s.game, GOLD);
    text35(5, 32, s.title, WHITE);
    text35(5, 42, s.composer, FELT_LT);
    int8_t waveform[128];
    chip::snapshot(waveform);
    gfx_hline(4, 65, 120, FELT_DK);
    for (int x = 5; x < 124; ++x)
        gfx_line(x - 1, 65 - waveform[x - 1] / 5, x, 65 - waveform[x] / 5, FX_A);
    static const char *names[] = {"PULSE", "SQUARE", "BASS", "NOISE"};
    for (unsigned ch = 0; ch < 4; ++ch) {
        const int x = 5 + ch * 31;
        const bool muted = chip::muteMask() & (1u << ch);
        text35(x, 84, names[ch], muted ? FELT_DK : GOLD);
        gfx_fillRect(x, 93, 24, 5, FELT_DK);
        if (!muted) gfx_fillRect(x, 93, chip::voice(ch).volume * 24 / 40, 5, FX_A);
    }
    gfx_fillRect(5, 105, 118, 2, FELT_DK);
    gfx_fillRect(5, 105, ((chip::ticks() % s.length) * 118) / s.length, 2, GOLD);
    text35(5, 112, chip::paused() ? "A:PLAY" : "A:PAUSE", WHITE);
    text35(44, 112, melodyOnly ? "B:FULL" : "B:LEAD", GOLD);
    text35(86, 112, "<> TRACK", WHITE);
    fmtInt(number, chip::volume());
    text35(5, 122, "VOL", FELT_LT);
    text35(21, 122, number, WHITE);
    text35(35, 122, "UP/DN", FELT_LT);
    text35(76, 122, "START RESET", FELT_LT);
}

void loop() {
    dbg::poll();
#ifdef CHSIM
    // The visual simulator has no timers. Advance the real mixer on virtual time.
    while (simulatedAt != millis()) {
        ++simulatedAt;
        chip::millisecond();
        // Fractional sample clock: 31,250 samples in each simulated second.
        unsigned n = (simulatedAt * 125 / 4) - ((simulatedAt - 1) * 125 / 4);
        while (n--) chip::sample();
    }
#endif
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    if (chgame.justPressed(LEFT_BUTTON)) selectSong(-1);
    if (chgame.justPressed(RIGHT_BUTTON)) selectSong(1);
    if (chgame.justPressed(A_BUTTON)) chip::pause(!chip::paused());
    if (chgame.justPressed(B_BUTTON)) {
        melodyOnly = !melodyOnly;
        for (unsigned i = 1; i < 4; ++i) chip::mute(i, melodyOnly);
    }
    if (chgame.justPressed(UP_BUTTON)) chip::setVolume(chip::volume() + 1);
    if (chgame.justPressed(DOWN_BUTTON) && chip::volume()) chip::setVolume(chip::volume() - 1);
    if (chgame.justPressed(START_BUTTON)) chip::play(selected);
    pal::tick();
    gfx_wait();
    pal::commit();
    drawPlayer();
    gfx_flushAsync();
}
