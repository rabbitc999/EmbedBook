#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

void Timer_Init(void);
uint32_t Timer_GetDiffTick(uint32_t startTick, uint32_t endTick);
uint32_t Timer_GetDiffMs(uint32_t startTick);

#endif
