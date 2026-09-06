#ifndef __BSP_KEY_SM_H
#define __BSP_KEY_SM_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键电压状态枚举
 * @details 定义按键的物理电压状态
 */
typedef enum tagKeyVolatge_T
{
    KEY_VOLATGE_RELEASED,  /* 按键释放状态（高电平或低电平取决于硬件设计） */
    KEY_VOLATGE_PRESSED,   /* 按键按下状态（低电平或高电平取决于硬件设计） */
    KEY_VOLATGE_EDGE_UP,   /* 按键上升沿状态（仅内部使用） */
    KEY_VOLATGE_EDGE_DOWN, /* 按键下升沿状态（仅内部使用） */
    KEY_VOLATGE_NONE       /* 按键下升沿状态（仅内部使用） */
} KeyVolatge_T;

/**
 * @brief 按键事件标志（位域）
 * @details 定义按键可能产生的各种事件，使用位域方式以便组合检测
 */
typedef enum tagKeyEvent_T
{
    KEY_EVENT_NONE = 0x00,         /* 无事件 */
    KEY_EVENT_CLICK = 0x01,        /* 单击事件 */
    KEY_EVENT_DOUBLE_CLICK = 0x02, /* 双击事件 */
    KEY_EVENT_LONG_PRESS = 0x04,   /* 长按事件 */
    KEY_EVENT_REPEAT = 0x08,       /* 连击事件 */
    KEY_EVENT_PRESS = 0x10,        /* 按键按下事件 */
    KEY_EVENT_RELEASE = 0x20,      /* 按键释放事件 */
    KEY_EVENT_TIMEOUT = 0x40,      /* 超时事件 */
    KEY_EVENT_IDLE = 0x80,         /* 空闲事件 */
    KEY_EVENT_ALL = 0xFF,          /* 全体事件 */
} KeyEvent_T;

/**
 * @brief 按键状态机状态枚举
 * @details 定义按键状态机的各种状态
 */
typedef enum tagKeyState_T
{
    KEY_STATE_IDLE,                    /* 空闲状态，等待按键按下 */
    KEY_STATE_CLICK_DELAY,             /* 单击事件消抖 */
    KEY_STATE_CLICK,                   /* 按下消抖状态，确认按键按下 */
    KEY_STATE_LONG_PRESS,              /* 长按状态，持续按键中 */
    KEY_STATE_DOUBLE_CLICK_DELAY_DOWN, /* 双击弹开消抖状态 */
    KEY_STATE_DOUBLE_CLICK_WAIT,       /* 双击等待状态 */
    KEY_STATE_DOUBLE_CLICK_DELAY_UP,   /* 双击按下消抖状态 */
    KEY_STATE_DOUBLE_CLICK,            /* 双击状态 */
} KeyState_T;

/**
 * @brief 按键状态机结构体
 * @details 包含按键状态机的所有状态变量和回调函数指针
 */
typedef struct tagKeySM_T
{
    KeyState_T state;       /* 当前状态机状态 */
    uint32_t lastStateTime; /* 上次状态变化时间戳 */
    uint32_t lastVolTime;   /* 上次电平检测时间戳 */
    KeyEvent_T event;       /* 当前按键事件标志 */

    /* 回调函数指针 */
    void (*keyUpCallback)(void);          /* 按键释放回调函数 */
    void (*keyDownCallback)(void);        /* 按键按下回调函数 */
    void (*keyClickCallback)(void);       /* 单击回调函数 */
    void (*keyLongCallback)(void);        /* 长按回调函数 */
    void (*keyDoubleClickCallback)(void); /* 双击回调函数 */
    void (*keyRepeatCallback)(void);      /* 连击回调函数 */
    void (*keyIdleCallback)(void);        /* 空闲回调函数 */

    /* 输入函数指针 */
    uint8_t (*keyInputFunc)(void);   /* 按键输入函数指针 */

    /* 功能开关 */
    uint8_t repeatEnable;        /* 连击功能使能标志 */
    uint8_t doubleEnable;        /* 双击功能使能标志 */
    uint8_t InitFlag;            /* 初始化标志 */
    KeyVolatge_T lastVoltage;    /* 上一次的电平 */
    KeyVolatge_T currentVoltage; /* 这一次的电平 */
} KeySM_T;

void KeySM_Init(KeySM_T *sm, uint8_t (*inputFunc)(void));
void KeySM_Process(KeySM_T *sm);
void KeySM_SetInputFunc(KeySM_T *sm, uint8_t (*inputFunc)(void));
void KeySM_SetUpCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetDownCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetClickCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetLongCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetDoubleClickCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetRepeatCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetIdleCallback(KeySM_T *sm, void (*callback)(void));
void KeySM_SetRepeatEnable(KeySM_T *sm, uint8_t enable);
void KeySM_SetDoubleEnable(KeySM_T *sm, uint8_t enable);
uint8_t KeySM_IsClicked(KeySM_T *sm);
uint8_t KeySM_IsDoubleClicked(KeySM_T *sm);
uint8_t KeySM_IsLongPressed(KeySM_T *sm);
uint8_t KeySM_IsRepeated(KeySM_T *sm);
uint8_t KeySM_HasEvent(KeySM_T *sm, KeyEvent_T event);
void KeySM_ClearEvent(KeySM_T *sm, KeyEvent_T event);
void KeySM_ResetState(KeySM_T *sm);
void KeySM_SetEvent(KeySM_T *sm, KeyEvent_T event);
uint8_t KeySM_IsAction(KeySM_T *sm);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEY_SM_H */
