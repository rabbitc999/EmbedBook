#include "bsp_system.h"
#include "bsp_periph.h"

#define TMC2209_EN_PIN    PB3
#define TMC2209_DIR_PIN   PB4
#define TMC2209_STEP_PIN  PB5
#define TMC2209_MS1_PIN   PB6
#define TMC2209_MS2_PIN   PB7

/**
 * @brief TMC2209步进电机驱动芯片初始化函数
 * @details 配置TMC2209的GPIO引脚为推挽输出模式，并设置初始状态
 */
void TMC2209_Init(void)
{
    BSP_GPIO_Init(TMC2209_EN_PIN, OUTPUT_PP);
    BSP_GPIO_Init(TMC2209_DIR_PIN, OUTPUT_PP);
    BSP_GPIO_Init(TMC2209_STEP_PIN, OUTPUT_PP);
    BSP_GPIO_Init(TMC2209_MS1_PIN, OUTPUT_PP);
    BSP_GPIO_Init(TMC2209_MS2_PIN, OUTPUT_PP);

    TMC2209_Disable();                                                 // 默认失能驱动器
    TMC2209_SetSubdivision(TMC2209_SUB_DIV_8);                         // 默认1/8步细分
    BSP_GPIO_WritePin(TMC2209_DIR_PIN, 0);    // 默认方向
    BSP_GPIO_WritePin(TMC2209_STEP_PIN, 0);  // 默认 步进信号低
}

/**
 * @brief 使能TMC2209驱动器
 * @details 将EN引脚拉低以使能驱动器
 */
void TMC2209_Enable(void)
{
    BSP_GPIO_WritePin(TMC2209_EN_PIN, 0);
}

/**
 * @brief 失能TMC2209驱动器
 * @details 将EN引脚拉高以失能驱动器
 */
void TMC2209_Disable(void)
{
    BSP_GPIO_WritePin(TMC2209_EN_PIN, 1);
}

/**
 * @brief 设置TMC2209的细分模式
 * @param i 细分模式选择
 */
void TMC2209_SetSubdivision(uint8_t i)
{
    switch (i)
    {
    case TMC2209_SUB_DIV_8:   // 1/8步
        BSP_GPIO_WritePin(TMC2209_MS1_PIN, 0);
        BSP_GPIO_WritePin(TMC2209_MS2_PIN, 0);
        break;
    case TMC2209_SUB_DIV_16:  // 1/16步
        BSP_GPIO_WritePin(TMC2209_MS1_PIN, 1);
        BSP_GPIO_WritePin(TMC2209_MS2_PIN, 1);
        break;
    case TMC2209_SUB_DIV_32:  // 1/32步
        BSP_GPIO_WritePin(TMC2209_MS1_PIN, 1);
        BSP_GPIO_WritePin(TMC2209_MS2_PIN, 0);
        break;
    case TMC2209_SUB_DIV_64:  // 1/64步
        BSP_GPIO_WritePin(TMC2209_MS1_PIN, 0);
        BSP_GPIO_WritePin(TMC2209_MS2_PIN, 1);
        break;
    default:
        break;
    }
}

/**
 * @brief 控制TMC2209步进电机移动指定步数
 * @param Dir 方向选择 (0-顺时针, 1-逆时针)
 * @param Step 要移动的步数
 */
void TMC2209_MoveStep(uint8_t Dir, uint32_t Step)
{
    TMC2209_Enable();
    BSP_GPIO_WritePin(TMC2209_DIR_PIN, (Dir == TMC2209_DIR_CW) ? 0 : 1);
    for (uint32_t i = 0; i < Step; i++)
    {
        BSP_GPIO_WritePin(TMC2209_STEP_PIN, 1);
        DWT_DelayUs(1);  // 步进信号保持时间
        BSP_GPIO_WritePin(TMC2209_STEP_PIN, 0);
        DWT_DelayUs(1);  // 步进间隔时间
    }
    TMC2209_Disable();
}

/**
 * @brief 控制TMC2209步进电机移动1步
 * @param Dir 方向选择 (0-顺时针, 1-逆时针)
 */
void TMC2209_MoveOneStep(uint8_t Dir)
{
    TMC2209_Enable();
    BSP_GPIO_WritePin(TMC2209_DIR_PIN, (Dir == TMC2209_DIR_CW) ? 0 : 1);
    BSP_GPIO_WritePin(TMC2209_STEP_PIN, 1);
    DWT_DelayUs(1);  // 步进信号保持时间
    BSP_GPIO_WritePin(TMC2209_STEP_PIN, 0);
    DWT_DelayUs(1);  // 步进间隔时间
    TMC2209_Disable();
}
