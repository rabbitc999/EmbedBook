#include "stm32f10x.h" // 包含STM32F10x系列微控制器的设备头文件
#include "Key.h"

KeyCtrl_T Keys[KEY_COUNT] = {
    { KEY_STATE_IDLE, KEY1_Get, 0, 0 },
    { KEY_STATE_IDLE, KEY2_Get, 0, 0 },
    { KEY_STATE_IDLE, KEY3_Get, 0, 0 },
    { KEY_STATE_IDLE, KEY4_Get, 0, 0 },
};

// 按键初始化
void KEY_Init(void)
{
    /* 开启时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 开启GPIOD的时钟

    /* GPIO初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 将引脚初始化为上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/* 更新状态 20ms调用一次 */
void Key_StateUpdate(void)
{
    uint16_t i = 0;
    for (i = 0; i < KEY_COUNT; i++)
    {
        Keys[i].LastToggle = Keys[i].CurrentToggle;
        Keys[i].CurrentToggle = Keys[i].GetToggle();
        Keys[i].Count++;
        
        switch(Keys[i].State)
        {
            case KEY_STATE_IDLE:   // 空闲
                if (Keys[i].LastToggle == 0 && Keys[i].CurrentToggle != 0)
                {
                    Keys[i].Event.Click = 1;
                    Keys[i].State = KEY_STATE_PRESS;
                    Keys[i].Count = 0;
                }
                break;
            case KEY_STATE_PRESS:  // 按下
                if (Keys[i].Count > KEY_LONG_COUNTER)
                {
                    Keys[i].Event.Long = 1;
                    Keys[i].State = KEY_STATE_LONG;
                    Keys[i].Count = 0;
                }
                if (Keys[i].CurrentToggle == 0)
                {
                    Keys[i].State = KEY_STATE_IDLE;
                }
                break;
            case KEY_STATE_LONG:   // 长按
                if (Keys[i].Count > KEY_REPEAT_COUNTER)
                {
                    Keys[i].Event.Repeat = 1;
                    Keys[i].Count = 0;
                }
                if (Keys[i].CurrentToggle == 0)
                {
                    Keys[i].State = KEY_STATE_IDLE;
                }
                break;
            default:
                Keys[i].State = KEY_STATE_IDLE;
                break;
        }
    }
}

uint8_t Key_GetClickEvent(Key_T key)
{
    uint8_t Event = Keys[key].Event.Click;
    Keys[key].Event.Click = 0;
    return Event;
}

uint8_t Key_GetLongEvent(Key_T key)
{
    uint8_t Event = Keys[key].Event.Long;
    Keys[key].Event.Long = 0;
    return Event;
}

uint8_t Key_GetRepeatEvent(Key_T key)
{
    uint8_t Event = Keys[key].Event.Repeat;
    Keys[key].Event.Repeat = 0;
    return Event;
}

/* 读取KEY1按键值 */
uint8_t KEY1_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_0) == 0)
    {
        return 1;
    }
    return 0;
}

/* 读取KEY2按键值 */
uint8_t KEY2_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_1) == 0)
    {
        return 1;
    }
    return 0;
}

/* 读取KEY3按键值 */
uint8_t KEY3_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2) == 0)
    {
        return 1;
    }
    return 0;
}

/* 读取KEY4按键值 */
uint8_t KEY4_Get(void)
{
    /* 检测按键是否按下 */
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3) == 0)
    {
        return 1;
    }
    return 0;
}
