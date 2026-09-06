#include "bsp_system.h"
#include "bsp_periph.h"

#define LED1_PIN  PB12
#define LED2_PIN  PB0

/**
 * @brief  LED 初始化函数
 * @note   初始化 PA1 和 PA2 为推挽输出模式
 */
void LED_Init(void)
{
    /* 1. 配置 PA1 和 PA2 为推挽输出 */
    BSP_GPIO_Init(LED1_PIN, OUTPUT_PP);
    BSP_GPIO_Init(LED2_PIN, OUTPUT_PP);

    /* 2. 初始化 LED 为高电平（熄灭） */
    BSP_GPIO_WritePin(LED1_PIN, GPIO_PIN_SET);
    BSP_GPIO_WritePin(LED2_PIN, GPIO_PIN_SET);
}

/**
 * @brief 开启LED1
 */
void LED1_ON(void)
{
    BSP_GPIO_WritePin(LED1_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 关闭LED1
 */
void LED1_OFF(void)
{
    BSP_GPIO_WritePin(LED1_PIN, GPIO_PIN_SET);
}

/**
 * @brief 切换LED1状态
 */
void LED1_Turn(void)
{
    BSP_GPIO_TogglePin(LED1_PIN);
}

/**
 * @brief 开启LED2
 */
void LED2_ON(void)
{
    BSP_GPIO_WritePin(LED2_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 关闭LED2
 */
void LED2_OFF(void)
{
    BSP_GPIO_WritePin(LED2_PIN, GPIO_PIN_SET);
}

/**
 * @brief 切换LED2状态
 */
void LED2_Turn(void)
{
    BSP_GPIO_TogglePin(LED1_PIN);
}
