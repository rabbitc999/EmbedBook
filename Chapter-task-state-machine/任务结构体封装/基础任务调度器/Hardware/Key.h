#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"  // 包含STM32F10x系列微控制器的头文件

void KEY_Init(void);
uint8_t KEY1_Get(void);
uint8_t KEY2_Get(void);
uint8_t KEY3_Get(void);
uint8_t KEY4_Get(void);

#endif
