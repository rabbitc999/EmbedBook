#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"  // 包含STM32F10x系列微控制器的头文件

#define KEY_LONG_PRESS_TIME  1000
#define KEY_DOUBLE_TICK_TIME 1500
#define KEY_REPEAT_TIME      200
#define KEY_DOUNCE_TIME      50

// 实体按键结构体
typedef enum tagKeyEnum_T
{
    KEY_MODE,
    KEY_ENTER,
    KEY_UP,
    KEY_DOWN,
    KEY_NUMBER
} KeyEnum_T;

// 按键事件标志
typedef enum tagKeyEvent_T
{
    KEY_EV_NONE = 0,             // 无事件
    KEY_EV_CLICK = 0x01,         // 单击
    KEY_EV_DOUBLE_CLICK = 0x02,  // 双击
    KEY_EV_LONG_PRESS = 0x04,    // 长按
    KEY_EV_REPEAT = 0x08         // 重复触发
} KeyEvent_T;

// 按键电平枚举
typedef enum tagKeyVoltage_T
{
    KEY_VOL_RELEASED,  // 按键释放
    KEY_VOL_PRESSED    // 按键按下
} KeyVoltage_T;

// 按键状态枚举
typedef enum tagKeyState_T
{
    KEY_STATE_WAIT,          // 等待状态
    KEY_STATE_CLICK,         // 按下状态
    KEY_STATE_LONG_PRESS,    // 长按状态
    KEY_STATE_DOUBLE_CLICK,  // 双击状态
} KeyState_T;

// 按键控制结构体
typedef struct tagKeyCtrl_T {
    // 状态变量
    KeyState_T State;  // 状态机状态
    KeyEvent_T Event;  // 当前按键事件
    // 物理状态
    KeyVoltage_T CurrentVoltage;
    KeyVoltage_T LastVoltage;
    // 回调函数
    KeyVoltage_T (*GetVoltage)(void);
    // 附加功能
    uint8_t RepeatEnable;
    uint8_t DoubleEnable;
    // 记录的时间
    uint32_t DeltaTime;
    uint32_t DounceTime;
} KeyCtrl_T;

void Key_Init(void);
void Key_Tick(void);
uint8_t Key_IsLongPressed(KeyEnum_T CurrentKey);
uint8_t Key_IsDoubleClicked(KeyEnum_T CurrentKey);
uint8_t Key_IsRepeated(KeyEnum_T CurrentKey);
uint8_t Key_IsClicked(KeyEnum_T CurrentKey);
uint8_t Key_IsAction(KeyEnum_T CurrentKey);
uint8_t Key_HasEvent(KeyEnum_T CurrentKey, KeyEvent_T event);
void Key_ClearEvent(KeyEnum_T CurrentKey);
void Key_ClearAll(void);
void Key_SetEvent(KeyEnum_T CurrentKey, KeyEvent_T event);
uint8_t Key_IsAnyKeyPressed(void);

KeyVoltage_T KEY1_Get(void);
KeyVoltage_T KEY2_Get(void);
KeyVoltage_T KEY3_Get(void);
KeyVoltage_T KEY4_Get(void);

#endif
