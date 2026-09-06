#ifndef __BSP_SYSTEM_H
#define __BSP_SYSTEM_H

#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_rtc.h"
#include "stm32f1xx_ll_pwr.h"

#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define SYSTEM_CLOCK_MHZ  72 /* 系统时钟频率(MHz) */
#define SYSTEM_DEBUG_MODE 1  /* 系统调试标志 */

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 函数定义 */
void System_ClockConfig(void);
void Board_Init(void);
void Assert_Failed(char *file, uint32_t line);

#ifdef __cplusplus
}
#endif

/* 角度弧度转换 */
#define M_PI       3.14159265358979323846f
#define DEG2RAD(x) ((x) * M_PI / 180.0f)
#define RAD2DEG(x) ((x) * 180.0f / M_PI)

/* 调试错误宏 */
#ifdef assert
#undef assert
#endif
#define assert(x)                                                                                   \
    do                                                                                              \
    {                                                                                               \
        uint32_t code = (x);                                                                        \
        if (code == 0) { DEBUG_PRINTF("断言错误 " __FILE__ "(%d), 错误码=%d\r\n", __LINE__, code); } \
    } while (0)

/* 开关全局中断的宏 */
#define BSP_ENABLE_INT()  __set_PRIMASK(0) /* 使能全局中断 */
#define BSP_DISABLE_INT() __set_PRIMASK(1) /* 禁止全局中断 */

/* 标准库引用 */
#include "bsp_oled.h"
#include "bsp_key.h"
#include "bsp_adc.h"
#include "bsp_timer.h"
#include "bsp_rtc.h"
#include "bsp_dht11.h"
#include "bsp_flash.h"
#include "bsp_dwt.h"
#include "bsp_beep.h"
#include "bsp_tmc2209.h"
#include "bsp_hx711.h"
#include "bsp_led.h"
#include "bsp_hot.h"

#endif
