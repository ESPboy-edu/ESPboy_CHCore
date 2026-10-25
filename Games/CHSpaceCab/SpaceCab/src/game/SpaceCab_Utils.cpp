#include <CHGame.h>
#include "SpaceCab.h"

using PC = Pokitto::Core;
using PD = Pokitto::Display;

// The Pokitto played recorded voices and sounds from the SD card
// (music/SPCAB_FILES/*.raw: "hey taxi!", "platform 3 please", "ouch",
// a cash register ...). CHGame's piezo plays tones: each is a short effect
// in their place (Sounds.h), and the theme tune is left out.

#include "Sounds.h"

void Game::playTakeMe(uint8_t platformNumber) {

    // "Take me to platform n": a call, raised a semitone for each platform.
    audio::sfx(Sfx::TakeMe, platformNumber < 12 ? platformNumber : 12);

}

void Game::playHailTaxi() {

    this->maleOrFemale = random(0, 2);
    audio::sfx(this->maleOrFemale ? Sfx::HailMale : Sfx::HailFemale);

}

void Game::playSoundEffect(uint8_t effectNumber) {

    static const Sfx effects[6] = { Sfx::OuchFemale, Sfx::OuchMale, Sfx::Kaching, Sfx::Explosion, Sfx::RollerDoor, Sfx::Fuel };
    if (effectNumber < 6) audio::sfx(effects[effectNumber]);

}

void Game::playTheme() {
}
