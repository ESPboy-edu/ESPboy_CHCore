#pragma once
#include <Arduino.h>

// Для ESP8266 оставляем макросы пустыми, 
// чтобы не забивать IRAM и оставить память для WiFi/SSL
#define CHGAME_RAMFUNC(name)
#define CHGAME_APP_RAMFUNC(name)

#ifndef RAMFUNC
#define RAMFUNC(name)
#endif