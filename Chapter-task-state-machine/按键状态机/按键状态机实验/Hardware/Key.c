#include "stm32f10x.h" // 包含STM32F10x系列微控制器的设备头文件
#include "key.h"

KeyCtrl_T KeyArrayCtrls[KEY_NUMBER];

// 按键初始化
void Key_Init(void)
{
    /* 开启时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 开启GPIOD的时钟

    /* GPIO初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 将引脚初始化为上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    KeyArrayCtrls[KEY_ENTER].GetVoltage = KEY1_Get;
    KeyArrayCtrls[KEY_MODE].GetVoltage = KEY2_Get;
    KeyArrayCtrls[KEY_UP].GetVoltage = KEY3_Get;
    KeyArrayCtrls[KEY_DOWN].GetVoltage = KEY4_Get;
    
    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        KeyArrayCtrls[i].State = KEY_STATE_WAIT;
        KeyArrayCtrls[i].Event = KEY_EV_NONE;
        KeyArrayCtrls[i].CurrentVoltage = KeyArrayCtrls[i].LastVoltage = KEY_VOL_RELEASED;
        KeyArrayCtrls[i].DoubleEnable = KeyArrayCtrls[i].RepeatEnable = 1;
        KeyArrayCtrls[i].DounceTime = KeyArrayCtrls[i].DeltaTime = 0;
    }
}

// 1ms调用一次
void Key_Tick(void)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        KeyArrayCtrls[i].DeltaTime++;
        KeyArrayCtrls[i].DounceTime++;
        
        if (KeyArrayCtrls[i].DounceTime >= KEY_DOUNCE_TIME)
        {
            KeyArrayCtrls[i].LastVoltage = KeyArrayCtrls[i].CurrentVoltage;
            KeyArrayCtrls[i].CurrentVoltage = KeyArrayCtrls[i].GetVoltage();
            KeyArrayCtrls[i].DounceTime = 0;
        }
        
        switch(KeyArrayCtrls[i].State)
        {
        case KEY_STATE_WAIT:
        {
            if (KeyArrayCtrls[i].LastVoltage == KEY_VOL_RELEASED
                 && KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_PRESSED)
            {
                KeyArrayCtrls[i].LastVoltage = KEY_VOL_PRESSED;
                KeyArrayCtrls[i].State = KEY_STATE_CLICK;
                KeyArrayCtrls[i].Event |= KEY_EV_CLICK;
                KeyArrayCtrls[i].DeltaTime = 0;
            }
            break;
        }
        case KEY_STATE_CLICK:
        {
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_PRESSED 
                && KeyArrayCtrls[i].DeltaTime >= KEY_LONG_PRESS_TIME)
            {
                KeyArrayCtrls[i].State = KEY_STATE_LONG_PRESS;
                KeyArrayCtrls[i].Event |= KEY_EV_LONG_PRESS;
                KeyArrayCtrls[i].DeltaTime = 0;
                break;
            }
            
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_RELEASED
                && KeyArrayCtrls[i].DoubleEnable != 0)
            {
                KeyArrayCtrls[i].State = KEY_STATE_DOUBLE_CLICK;
                KeyArrayCtrls[i].DeltaTime = 0;
                break;
            }
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_RELEASED
                && KeyArrayCtrls[i].DoubleEnable == 0)
            {
                KeyArrayCtrls[i].State = KEY_STATE_WAIT;
                break;
            }
            break;
        }
        case KEY_STATE_LONG_PRESS:
        {
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_PRESSED 
                && KeyArrayCtrls[i].DeltaTime >= KEY_REPEAT_TIME
                && KeyArrayCtrls[i].RepeatEnable != 0)
            {
                KeyArrayCtrls[i].State = KEY_STATE_LONG_PRESS;
                KeyArrayCtrls[i].Event |= KEY_EV_REPEAT;
                KeyArrayCtrls[i].DeltaTime = 0;
                break;
            }
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_RELEASED)
            {
                KeyArrayCtrls[i].State = KEY_STATE_WAIT;
                break;
            }
            break;
        }
        case KEY_STATE_DOUBLE_CLICK:
        {
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_PRESSED 
                && KeyArrayCtrls[i].DeltaTime <= KEY_DOUBLE_TICK_TIME)
            {
                KeyArrayCtrls[i].State = KEY_STATE_WAIT;
                KeyArrayCtrls[i].Event |= KEY_EV_DOUBLE_CLICK;
                KeyArrayCtrls[i].Event |= KEY_EV_CLICK;
                break;
            }
            if (KeyArrayCtrls[i].CurrentVoltage == KEY_VOL_RELEASED 
                && KeyArrayCtrls[i].DeltaTime >= KEY_DOUBLE_TICK_TIME)
            {
                KeyArrayCtrls[i].State = KEY_STATE_WAIT;
                break;
            }
            break;
        }
        default:
        {
            KeyArrayCtrls[i].State = KEY_STATE_WAIT;
        }
        }
    }
}

uint8_t Key_IsLongPressed(KeyEnum_T CurrentKey)
{
    uint8_t Result = 0;
    if ((KeyArrayCtrls[CurrentKey].Event & KEY_EV_LONG_PRESS) == KEY_EV_LONG_PRESS) 
    {
        KeyArrayCtrls[CurrentKey].Event &= ~(KEY_EV_LONG_PRESS | KEY_EV_CLICK);
        Result = 1;
    }
    return Result;
}
uint8_t Key_IsDoubleClicked(KeyEnum_T CurrentKey)
{
    uint8_t Result = 0;
    if ((KeyArrayCtrls[CurrentKey].Event & KEY_EV_DOUBLE_CLICK) == KEY_EV_DOUBLE_CLICK) 
    {
        KeyArrayCtrls[CurrentKey].Event &= ~(KEY_EV_DOUBLE_CLICK | KEY_EV_CLICK);
        Result = 1;
    }
    return Result;
}

uint8_t Key_IsRepeated(KeyEnum_T CurrentKey)
{
    uint8_t Result = 0;
    if ((KeyArrayCtrls[CurrentKey].Event & KEY_EV_REPEAT) == KEY_EV_REPEAT) 
    {
        KeyArrayCtrls[CurrentKey].Event &= ~(KEY_EV_REPEAT | KEY_EV_CLICK);
        Result = 1;
    }
    return Result;
}

uint8_t Key_IsClicked(KeyEnum_T CurrentKey)
{
    uint8_t Result = 0;
    if ((KeyArrayCtrls[CurrentKey].Event & KEY_EV_CLICK) == KEY_EV_CLICK) 
    {
        KeyArrayCtrls[CurrentKey].Event &= ~(KEY_EV_CLICK);
        Result = 1;
    }
    return Result;
}

uint8_t Key_IsAction(KeyEnum_T CurrentKey)
{
    uint8_t Result = 0;
    if (KeyArrayCtrls[CurrentKey].Event != KEY_EV_NONE) 
    {
        KeyArrayCtrls[CurrentKey].Event = KEY_EV_NONE;
        Result = 1;
    }
    return Result;
}

uint8_t Key_HasEvent(KeyEnum_T CurrentKey, KeyEvent_T event)
{
    if ((KeyArrayCtrls[CurrentKey].Event & event) == event) 
    {
        return 1;
    }
    return 0;
}

void Key_ClearEvent(KeyEnum_T CurrentKey)
{
    KeyArrayCtrls[CurrentKey].Event = KEY_EV_NONE;
}

void Key_ClearAll(void)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        KeyArrayCtrls[i].Event = KEY_EV_NONE;
    }
}

void Key_SetEvent(KeyEnum_T CurrentKey, KeyEvent_T event)
{
    KeyArrayCtrls[CurrentKey].Event = event;
}

uint8_t Key_IsAnyKeyPressed(void)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        if (Key_IsAction((KeyEnum_T)i))
        {
            return 1;
        }
    }
    return 0;
}

/* 读取KEY1按键值 */
KeyVoltage_T KEY1_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_0) == 0)
    {
        return KEY_VOL_PRESSED;
    }
    return KEY_VOL_RELEASED;
}

/* 读取KEY2按键值 */
KeyVoltage_T KEY2_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_1) == 0)
    {
        return KEY_VOL_PRESSED;
    }
    return KEY_VOL_RELEASED;
}

/* 读取KEY3按键值 */
KeyVoltage_T KEY3_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2) == 0)
    {
        return KEY_VOL_PRESSED;
    }
    return KEY_VOL_RELEASED;
}

/* 读取KEY4按键值 */
KeyVoltage_T KEY4_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3) == 0)
    {
        return KEY_VOL_PRESSED;
    }
    return KEY_VOL_RELEASED;
}
