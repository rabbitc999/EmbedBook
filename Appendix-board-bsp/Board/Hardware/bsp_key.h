#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "bsp_system.h"
#include "stm32f1xx_ll_gpio.h" /* 需包含GPIO相关定义 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagKeyVolatge_T KeyVolatge_T;
typedef enum tagKeyEvent_T KeyEvent_T;
typedef enum tagKeyState_T KeyState_T;
typedef struct tagKeySM_T KeySM_T;

/* 按键枚举定义 */
typedef enum
{
    KEY_MODE,    /* 模式按键 */
    KEY_ENTER,   /* 确认按键 */
    KEY_UP,      /* 上方向按键 */
    KEY_DOWN,    /* 下方向按键 */
    KEY_NUMBER   /* 按键总数(用于数组边界判断) */
} KeyEnum_T;

void Key_Init(void);
KeyVolatge_T Key_GetVoltage(KeyEnum_T key);
void Key_Tick(void);
KeySM_T *Key_GetSm(KeyEnum_T key);
void Key_SetUpCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetDownCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetClickCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetLongCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetDoubleClickCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetRepeatCallback(KeyEnum_T key, void (*callback)(void));
void Key_SetIdleCallback(KeyEnum_T key, void (*callback)(void));
uint8_t Key_IsClicked(KeyEnum_T key);
uint8_t Key_IsDoubleClicked(KeyEnum_T key);
uint8_t Key_IsLongPressed(KeyEnum_T key);
uint8_t Key_IsRepeated(KeyEnum_T key);
uint8_t Key_HasEvent(KeyEnum_T key, KeyEvent_T event);
uint8_t Key_IsAction(KeyEnum_T key);
void Key_ClearEvent(KeyEnum_T key, KeyEvent_T event);
void Key_ResetState(KeyEnum_T key);
uint8_t Key_IsAnyKeyPressed(void);
void Key_SetEvent(KeyEnum_T key, KeyEvent_T event);
void Key_ClearAll(void);

#ifdef __cplusplus
}
#endif

#endif
