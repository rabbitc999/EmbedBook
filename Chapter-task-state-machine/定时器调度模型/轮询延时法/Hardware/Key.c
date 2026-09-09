#include "stm32f10x.h" // 包含STM32F10x系列微控制器的设备头文件

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
