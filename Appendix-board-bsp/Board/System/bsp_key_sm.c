#include "bsp_key_sm.h"
#include "bsp_timer.h"

/* 状态机时间参数定义 */
#define KEY_DEBOUNCE_MS          20   /* 消抖时间（毫秒） */
#define KEY_DOUBLE_CLICK_TIME_MS 500  /* 双击超时时间（毫秒） */
#define KEY_LONGPRESS_TIMEOUT_MS 1500 /* 长按超时时间（毫秒） */
#define KEY_REPEAT_INTERVAL_MS   50   /* 连击间隔时间（毫秒） */

/**
 * @brief 状态机初始化函数
 * @param sm 指向按键状态机结构体的指针
 *
 * @details 初始化按键状态机，设置初始状态为空闲状态
 */
void KeySM_Init(KeySM_T *sm, uint8_t (*inputFunc)(void))
{
    /* 初始化状态机状态 */
    sm->state = KEY_STATE_IDLE;
    sm->lastStateTime = sm->lastVolTime = 0;
    sm->event = KEY_EVENT_NONE;

    /* 清空所有回调函数指针 */
    sm->keyUpCallback = NULL;
    sm->keyDownCallback = NULL;
    sm->keyClickCallback = NULL;
    sm->keyLongCallback = NULL;
    sm->keyDoubleClickCallback = NULL;
    sm->keyRepeatCallback = NULL;
    sm->keyIdleCallback = NULL;

    /* 清空输入函数指针 */
    sm->keyInputFunc = inputFunc;

    /* 设置功能开关默认值 */
    sm->repeatEnable = 0;
    sm->doubleEnable = 0;
    sm->InitFlag = 1;
}

/**
 * @brief 按键状态机处理函数
 * @details 处理所有按键的状态机逻辑，应在主循环中定期调用
 */
void KeySM_Process(KeySM_T *sm)
{
    if (sm->InitFlag == 0) return;
    if (sm->keyInputFunc == NULL) return;

    /* 时间计算 */
    uint32_t deltaStateTick = Timer_GetDiffMs(sm->lastStateTime);
    uint32_t deltaVolTick = Timer_GetDiffMs(sm->lastVolTime);
    uint32_t volatge = KEY_VOLATGE_NONE;

    /* 读取当前电压状态并更新历史状态 */
    if (deltaVolTick >= KEY_DEBOUNCE_MS)
    {
        sm->lastVoltage = sm->currentVoltage;
        sm->currentVoltage = sm->keyInputFunc();

        /* 电平检测：边沿触发 */
        if (sm->currentVoltage != KEY_VOLATGE_RELEASED && sm->lastVoltage == KEY_VOLATGE_RELEASED)
        {
            volatge = KEY_VOLATGE_EDGE_UP;
        }
        else if (sm->currentVoltage == KEY_VOLATGE_RELEASED && sm->lastVoltage != KEY_VOLATGE_RELEASED)
        {
            volatge = KEY_VOLATGE_EDGE_DOWN;
        }
        else
        {
            volatge = sm->currentVoltage;
        }

        sm->lastVolTime = HAL_GetTick();
    }

    /* 调用空闲回调函数 */
    if (sm->keyIdleCallback) { sm->keyIdleCallback(); }
    
    /* 没有检测到新的电平，不处理状态机 */
    if (volatge == KEY_VOLATGE_NONE) { return; }

    /* 状态机处理 */
    switch (sm->state)
    {
    case KEY_STATE_IDLE:
    {
        if (volatge == KEY_VOLATGE_EDGE_UP)
        {
            sm->state = KEY_STATE_CLICK_DELAY; /* 进入按下消抖状态 */
        }
        break;
    }
    case KEY_STATE_CLICK_DELAY:
    {
        if (volatge == KEY_VOLATGE_PRESSED)
        {
            if (sm->keyDownCallback) sm->keyDownCallback();
            sm->state = KEY_STATE_CLICK; /* 进入按下状态 */
            sm->lastStateTime = HAL_GetTick();
        }
        else
        {
            sm->state = KEY_STATE_IDLE; /* 回到初始状态 */
        }
        break;
    }
    case KEY_STATE_CLICK:
    {
        if (volatge == KEY_VOLATGE_EDGE_DOWN)
        {
            /* 按键释放，触发单击事件 */
            sm->event |= KEY_EVENT_CLICK;

            if (sm->doubleEnable)
            {
                sm->state = KEY_STATE_DOUBLE_CLICK_DELAY_DOWN; /* 进入双击消抖状态 */
            }
            else
            {
                sm->state = KEY_STATE_IDLE; /* 回到初始状态 */
                if (sm->keyUpCallback) sm->keyUpCallback();
                if (sm->keyClickCallback) sm->keyClickCallback();
            }
        }
        if (volatge == KEY_VOLATGE_PRESSED && deltaStateTick >= KEY_LONGPRESS_TIMEOUT_MS)
        {
            if (sm->keyLongCallback) sm->keyLongCallback();
            sm->state = KEY_STATE_LONG_PRESS; /* 进入长按状态 */
            sm->lastStateTime = HAL_GetTick();
        }
        break;
    }
    case KEY_STATE_LONG_PRESS:
    {
        if (volatge == KEY_VOLATGE_RELEASED)
        {
            /* 按键释放，触发长按事件 */
            sm->event |= KEY_EVENT_LONG_PRESS;
            sm->state = KEY_STATE_IDLE;
            if (sm->keyUpCallback) sm->keyUpCallback();
        }
        if (volatge == KEY_VOLATGE_PRESSED && deltaStateTick >= KEY_REPEAT_INTERVAL_MS)
        {
            if (sm->keyRepeatCallback) sm->keyRepeatCallback();
            sm->event |= KEY_EVENT_REPEAT;
            sm->lastStateTime = HAL_GetTick();
        }
        break;
    }
    case KEY_STATE_DOUBLE_CLICK_DELAY_DOWN:
    {
        if (volatge == KEY_VOLATGE_RELEASED)
        {
            sm->state = KEY_STATE_DOUBLE_CLICK_WAIT; /* 进入双击等待状态 */
            sm->lastStateTime = HAL_GetTick();
        }
        else
        {
            sm->state = KEY_STATE_IDLE;
        }
        break;
    }
    case KEY_STATE_DOUBLE_CLICK_WAIT:
    {
        if (volatge == KEY_VOLATGE_EDGE_UP && deltaStateTick <= KEY_DOUBLE_CLICK_TIME_MS)
        {
            sm->state = KEY_STATE_DOUBLE_CLICK_DELAY_UP;
        }
        if (volatge == KEY_VOLATGE_RELEASED && deltaStateTick >= KEY_DOUBLE_CLICK_TIME_MS)
        {
            /* 双击超时，保持单击事件 */
            sm->state = KEY_STATE_IDLE; /* 等待释放或长按状态 */
            if (sm->keyUpCallback) sm->keyUpCallback();
            if (sm->keyClickCallback) sm->keyClickCallback();
        }
        break;
    }
    case KEY_STATE_DOUBLE_CLICK_DELAY_UP:
    {
        if (volatge == KEY_VOLATGE_PRESSED)
        {
            sm->state = KEY_STATE_DOUBLE_CLICK;
        }
        else
        {
            sm->state = KEY_STATE_IDLE;
            if (sm->keyUpCallback) sm->keyUpCallback();
        }
        break;
    }
    case KEY_STATE_DOUBLE_CLICK:
    {
        /* 第二次按下，触发双击 */
        sm->event |= KEY_EVENT_DOUBLE_CLICK;
        sm->state = KEY_STATE_CLICK; /* 回到单击状态，检测其他事件 */
        sm->lastStateTime = HAL_GetTick();
        if (sm->keyDoubleClickCallback) sm->keyDoubleClickCallback();
        break;
    }
    default:
        sm->state = KEY_STATE_IDLE; /* 等待释放或长按状态 */
        break;
    }
}

/**
 * @brief 设置按键输入函数
 * @param sm 指向按键状态机的指针
 * @param inputFunc 按键输入函数指针
 */
void KeySM_SetInputFunc(KeySM_T *sm, uint8_t (*inputFunc)(void))
{
    if (sm != NULL)
    {
        sm->keyInputFunc = inputFunc;
    }
}

/**
 * @brief 设置按键释放回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetUpCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyUpCallback = callback;
    }
}

/**
 * @brief 设置按键按下回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetDownCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyDownCallback = callback;
    }
}

/**
 * @brief 设置单击回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetClickCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyClickCallback = callback;
    }
}

/**
 * @brief 设置长按回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetLongCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyLongCallback = callback;
    }
}

/**
 * @brief 设置双击回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetDoubleClickCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyDoubleClickCallback = callback;
    }
}

/**
 * @brief 设置连击回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetRepeatCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyRepeatCallback = callback;
    }
}

/**
 * @brief 设置空闲回调函数
 * @param sm 指向按键状态机的指针
 * @param callback 回调函数指针
 */
void KeySM_SetIdleCallback(KeySM_T *sm, void (*callback)(void))
{
    if (sm != NULL)
    {
        sm->keyIdleCallback = callback;
    }
}

/**
 * @brief 设置连击功能使能
 * @param sm 指向按键状态机的指针
 * @param enable 使能标志（1-使能，0-禁用）
 */
void KeySM_SetRepeatEnable(KeySM_T *sm, uint8_t enable)
{
    if (sm != NULL)
    {
        sm->repeatEnable = enable;
    }
}

/**
 * @brief 设置双击功能使能
 * @param sm 指向按键状态机的指针
 * @param enable 使能标志（1-使能，0-禁用）
 */
void KeySM_SetDoubleEnable(KeySM_T *sm, uint8_t enable)
{
    if (sm != NULL)
    {
        sm->doubleEnable = enable;
    }
}

/**
 * @brief 检测按键单击事件
 * @param sm 指向按键状态机的指针
 * @return uint8_t 1-检测到单击事件，0-未检测到
 */
uint8_t KeySM_IsClicked(KeySM_T *sm)
{
    if (sm == NULL) return 0;
    if (sm->event & KEY_EVENT_CLICK)
    {
        sm->event &= ~KEY_EVENT_CLICK; /* 清除事件标志 */
        return 1;
    }
    return 0;
}

/**
 * @brief 检测按键双击事件
 * @param sm 指向按键状态机的指针
 * @return uint8_t 1-检测到双击事件，0-未检测到
 */
uint8_t KeySM_IsDoubleClicked(KeySM_T *sm)
{
    if (sm == NULL) return 0;
    if ((sm->event & KEY_EVENT_DOUBLE_CLICK) && (sm->doubleEnable))
    {
        sm->event &= ~(KEY_EVENT_DOUBLE_CLICK | KEY_EVENT_CLICK); /* 清除事件标志 */
        return 1;
    }
    return 0;
}

/**
 * @brief 检测按键长按事件
 * @param sm 指向按键状态机的指针
 * @return uint8_t 1-检测到长按事件，0-未检测到
 */
uint8_t KeySM_IsLongPressed(KeySM_T *sm)
{
    if (sm == NULL) return 0;
    if (sm->event & KEY_EVENT_LONG_PRESS)
    {
        sm->event &= ~(KEY_EVENT_LONG_PRESS | KEY_EVENT_CLICK); /* 清除事件标志 */
        return 1;
    }
    return 0;
}

/**
 * @brief 检测按键连击事件
 * @param sm 指向按键状态机的指针
 * @return uint8_t 1-检测到连击事件，0-未检测到
 */
uint8_t KeySM_IsRepeated(KeySM_T *sm)
{
    if (sm == NULL) return 0;
    if ((sm->event & KEY_EVENT_REPEAT) && (sm->repeatEnable))
    {
        sm->event &= ~(KEY_EVENT_REPEAT | KEY_EVENT_CLICK); /* 清除事件标志 */
        return 1;
    }
    return 0;
}

/**
 * @brief 检测按键特定事件
 * @param sm 指向按键状态机的指针
 * @param event 事件类型
 * @return uint8_t 1-检测到指定事件，0-未检测到
 */
uint8_t KeySM_HasEvent(KeySM_T *sm, KeyEvent_T event)
{
    if (sm == NULL) return 0;
    if (sm->event & event)
    {
        sm->event &= ~event; /* 清除事件标志 */
        return 1;
    }
    return 0;
}

/**
 * @brief 检测按键是否具有事件
 * @param sm 指向按键状态机的指针
 * @return uint8_t 1-检测到指定事件，0-未检测到
 */
uint8_t KeySM_IsAction(KeySM_T *sm)
{
    if (sm == NULL) return 0;
    uint8_t flag = KeySM_IsClicked(sm) || KeySM_IsLongPressed(sm) || KeySM_IsDoubleClicked(sm) || KeySM_IsRepeated(sm);
    sm->event = KEY_EVENT_NONE;
    return flag;
}

/**
 * @brief 清除按键特定事件
 * @param sm 指向按键状态机的指针
 * @param event 要清除的事件类型
 */
void KeySM_ClearEvent(KeySM_T *sm, KeyEvent_T event)
{
    if (sm != NULL)
    {
        sm->event &= ~event;
    }
}

/**
 * @brief 重置按键状态机为初始状态
 * @param sm 指向按键状态机的指针
 *
 * @details 此函数将状态机重置为初始状态(KEY_IDLE)。
 */
void KeySM_ResetState(KeySM_T *sm)
{
    if (sm != NULL)
    {
        sm->event = KEY_EVENT_NONE;
        sm->state = KEY_STATE_IDLE; /* 重置为初始状态 */
    }
}

/**
 * @brief 设置按键特定事件
 * @param sm 指向按键状态机的指针
 * @param event 要设置的事件类型
 *
 * @details 此函数用于设置指定的事件标志，常用于虚拟按键的事件触发。
 *          与KeySM_ClearEvent功能相反，用于模拟按键事件的发生。
 */
void KeySM_SetEvent(KeySM_T *sm, KeyEvent_T event)
{
    if (sm != NULL)
    {
        sm->event |= event; /* 设置事件标志 */
    }
}
