#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明类型：用户只能通过 API 访问 */
typedef struct __Timer_T Timer_T;

Timer_T *Timer_Create(void);
void Timer_SetPeriod(Timer_T *pTimer, uint32_t periodMs);

/* PC 模拟节拍：每调用一次累加 elapsedMs */
void Timer_Tick(Timer_T *pTimer, uint32_t elapsedMs);

/* 查询并清除超时标志 */
uint8_t Timer_IsExpired(Timer_T *pTimer);

#ifdef __cplusplus
}
#endif

#endif /* __TIMER_H */
