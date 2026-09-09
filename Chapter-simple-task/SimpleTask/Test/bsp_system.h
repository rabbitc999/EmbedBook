#ifndef __BSP_SYSTEM_H
#define __BSP_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================================================== */
/* PC仿真层：用软件计数器模拟STM32硬件外设（TIM6中断锁 / DWT高精度定时器 / WFI睡眠） */
/* =================================================================================== */

/* 函数指针有效性检查（模拟单片机上的函数地址校验） */
#define IS_VALID_FUNCTION_POINTER(funcPtr) ((funcPtr) != NULL)

/* 定时器中断锁模拟（PC单线程环境下为空操作） */
#define TIM6 6
#define LL_TIM_EnableIT_UPDATE(timer)  ((void)(timer))
#define LL_TIM_DisableIT_UPDATE(timer) ((void)(timer))

/* CPU睡眠指令模拟（PC环境下为空操作） */
#define __WFI() ((void)0)

/* 高精度定时器模拟：由测试文件维护的64位微秒计数器实现 */
uint64_t Bsp_MockGetTick(void);
uint64_t Bsp_MockGetTimeDiffUs(uint64_t startTick, uint64_t endTick);

#define DWT_GetTick()               Bsp_MockGetTick()
#define DWT_GetTimeDiffUs(start, end) Bsp_MockGetTimeDiffUs(start, end)

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SYSTEM_H */
