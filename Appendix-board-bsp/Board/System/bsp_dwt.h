#ifndef _BSP_DWT_H
#define _BSP_DWT_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DWT_CPU_FREQ_MHZ    72U                 /* STM32F103 主频：72MHz */
#define DWT_TICK_PER_US     (DWT_CPU_FREQ_MHZ)  /* 每微秒时钟周期数（72个） */
#define DWT_MAX_DELAY_US    0xFFFFFFFEU         /* 最大微秒延时（避免溢出） */
#define DWT_MAX_DELAY_NS    0xFFFFFFFEU         /* 最大纳秒延时（避免溢出） */
#define DWT_MAX_DELAY_MS    8947U               /* 最大毫秒延时（基于32位计数器溢出时间） */

uint8_t DWT_Init(void);
uint32_t DWT_GetTick(void);
void DWT_DelayUs(uint32_t us);
void DWT_DelayNs(uint32_t ns);
void DWT_DelayMs(uint32_t ms);
uint32_t DWT_GetTimeDiffUs(uint32_t startTick, uint32_t endTick);
uint32_t DWT_GetDiffUs(uint32_t startTick);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_DWT_H */