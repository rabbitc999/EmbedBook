/**
 * @file    timer.c
 * @brief   PC 可运行的定时器对象（模拟 STM32 TIM6）
 * @details 与书中的 Timer_Create/Timer_SetPeriod/Timer_IsExpired 接口保持一致；
 *          硬件版本只需把 Timer_Tick 放进定时器中断。
 */

#include <stddef.h>
#include "timer.h"

/* 私有结构体定义放在 .c 中，实现“封装” */
struct __Timer_T
{
    uint32_t periodMs;          /* 周期 */
    uint32_t tickCount;         /* 已累计毫秒 */
    volatile uint8_t expired;   /* 超时标志 */
};

static Timer_T s_Timer;         /* 静态对象池，避免 malloc */

Timer_T *Timer_Create(void)
{
    s_Timer.periodMs = 0u;
    s_Timer.tickCount = 0u;
    s_Timer.expired = 0u;
    return &s_Timer;
}

void Timer_SetPeriod(Timer_T *pTimer, uint32_t periodMs)
{
    if (pTimer != NULL) {
        pTimer->periodMs = periodMs;
    }
}

void Timer_Tick(Timer_T *pTimer, uint32_t elapsedMs)
{
    if (pTimer == NULL) {
        return;
    }
    pTimer->tickCount += elapsedMs;
    if ((pTimer->periodMs != 0u) &&
        (pTimer->tickCount >= pTimer->periodMs)) {
        pTimer->tickCount = 0u;
        pTimer->expired = 1u;
    }
}

uint8_t Timer_IsExpired(Timer_T *pTimer)
{
    uint8_t result = 0u;

    if (pTimer != NULL) {
        result = pTimer->expired;
        pTimer->expired = 0u;    /* 查询并清除 */
    }
    return result;
}
