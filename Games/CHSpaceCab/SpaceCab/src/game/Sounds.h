// The piezo's effects in place of the Pokitto's recorded sounds (Sounds.cpp).
#pragma once
#include <stdint.h>

enum class Sfx : uint8_t { OuchFemale, OuchMale, Kaching, Explosion, RollerDoor, Fuel, HailFemale, HailMale, TakeMe, COUNT };
void sounds_begin();
