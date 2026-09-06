#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

void Beep_Init(void);

void Beep_Play(uint32_t onMs, uint32_t offMs, uint32_t cycles);
void Beep_Stop(void);
void Beep_Enable(void);
void Beep_Disable(void);

/* 蜂鸣器命令结构 */
typedef struct Beep_T
{
    uint32_t timer;   /* 蜂鸣时间 */
    uint32_t onTime;  /* 蜂鸣持续时间 (ms) */
    uint32_t offTime; /* 静音持续时间 (ms) */
    uint32_t cycles;  /* 重复次数 */
    uint8_t state;    /* 蜂鸣状态 */
    uint8_t enable;   /* 蜂鸣状态 */
} Beep_T;

/* 声明定时器回调函数 */
void Beep_Tick(void);

#ifdef __cplusplus
}
#endif

#endif
