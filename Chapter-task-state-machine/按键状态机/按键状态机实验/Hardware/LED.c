#include "stm32f10x.h" // 包含STM32F10x系列微控制器的头文件

// LED初始化
void LED_Init(void)
{
    // 1. 开启GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 开启GPIOD的时钟

    // 2. 配置GPIO引脚
    GPIO_InitTypeDef GPIO_InitStructure;                     // 定义GPIO初始化结构体
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;         // 设置为推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;               // 选择PD14和PD15引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // 设置GPIO速度为50MHz
    GPIO_Init(GPIOD, &GPIO_InitStructure);                   // 应用上述配置初始化GPIOD的PD14和PD15引脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;               // 选择PD14和PD15引脚
    GPIO_Init(GPIOD, &GPIO_InitStructure);                   // 应用上述配置初始化GPIOD的PD14和PD15引脚

    // 3. 设置GPIO引脚的默认电平
    GPIO_SetBits(GPIOD, GPIO_Pin_14 | GPIO_Pin_15); // 将PD14和PD15引脚设置为高电平（默认关闭LED）
}

// LED1开启
void LED1_ON(void)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_14); // 将PD14引脚设置为低电平，点亮LED1
}

// LED1关闭
void LED1_OFF(void)
{
    GPIO_SetBits(GPIOD, GPIO_Pin_14); // 将PD14引脚设置为高电平，关闭LED1
}

// LED1翻转
void LED1_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOD, GPIO_Pin_14) == 0) // 检查PD14引脚的输出状态是否为低电平
    {
        GPIO_SetBits(GPIOD, GPIO_Pin_14); // 如果当前为低电平，则设置为高电平
    }
    else // 如果当前为高电平
    {
        GPIO_ResetBits(GPIOD, GPIO_Pin_14); // 则设置为低电平
    }
}

// LED2开启
void LED2_ON(void)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_15); // 将PD15引脚设置为低电平，点亮LED2
}

// LED2关闭
void LED2_OFF(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_15); // 将PD15引脚设置为高电平，关闭LED2
}

// LED2翻转
void LED2_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOD, GPIO_Pin_15) == 0) // 检查PD15引脚的输出状态是否为低电平
    {
        GPIO_SetBits(GPIOD, GPIO_Pin_15); // 如果当前为低电平，则设置为高电平
    }
    else // 如果当前为高电平
    {
        GPIO_ResetBits(GPIOD, GPIO_Pin_15); // 则设置为低电平
    }
}
