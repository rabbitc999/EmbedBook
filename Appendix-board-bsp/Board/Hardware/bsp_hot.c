#include "bsp_system.h"
#include "bsp_periph.h"

#include <stdlib.h>

#define HOT_PIN   PA8

/**
  * @brief 热敏电阻控制初始化
  */
void Hot_Init(void)
{
    /* 配置 GPIO 为推挽输出 */
    BSP_GPIO_Init(HOT_PIN, OUTPUT_PP);
    
    /* 默认关闭热敏电阻 */
    BSP_GPIO_WritePin(HOT_PIN, 0);
}

/**
  * @brief 打开热敏电阻
  */
void Hot_On(void)
{
    BSP_GPIO_WritePin(HOT_PIN, 1);
}

/**
  * @brief 关闭热敏电阻
  */
void Hot_Off(void)
{
    BSP_GPIO_WritePin(HOT_PIN, 0);
}