#pragma once
#include "Arduino.h"
struct IWDG_HandleTypeDef {};
struct WatchdogRegs {uint32_t KR=0,PR=0,RLR=0;} inline watchdog;
#define IWDG (&watchdog)
#define IWDG_PRESCALER_256 6
