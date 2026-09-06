#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"  // 包含STM32F10x系列微控制器的头文件

#define KEY_LONG_COUNTER    400
#define KEY_REPEAT_COUNTER  50

void KEY_Init(void);
uint8_t KEY1_Get(void);
uint8_t KEY2_Get(void);
uint8_t KEY3_Get(void);
uint8_t KEY4_Get(void);

// 状态
typedef enum tagKeyState_T
{
    KEY_STATE_IDLE,   // 松开状态
    KEY_STATE_PRESS,  // 按下
    KEY_STATE_LONG,   // 长按
} KeyState_T;

// 标志
typedef struct tagKeyEvent_T
{
    uint8_t Click : 1;
    uint8_t Long : 1;
    uint8_t Repeat : 1;
} KeyEvent_T;

// 控制结构体
typedef struct tagKeyCtrl_T {
    KeyState_T State;
    uint8_t (*GetToggle)(void);
    uint16_t Count;
    uint8_t LastToggle;
    uint8_t CurrentToggle;
    KeyEvent_T Event;
} KeyCtrl_T;

// 按键类型
typedef enum tagKey_T
{
    KEY1,
    KEY2,
    KEY3,
    KEY4,
    KEY_COUNT
} Key_T;

void Key_StateUpdate(void);
uint8_t Key_GetClickEvent(Key_T key);
uint8_t Key_GetLongEvent(Key_T key);
uint8_t Key_GetRepeatEvent(Key_T key);

#endif
