#include "bsp_system.h"

TIM_HandleTypeDef htim1;

/**
  * @brief  TIM1 初始化函数
  * @param  无
  * @retval 无
  */
void Timer_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    /* 1. 使能 TIM1 时钟 */
    __HAL_RCC_TIM1_CLK_ENABLE();
    
    /* 2. 配置 TIM1 时基参数 */
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 72 - 1;          // 预分频器
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;  // 向上计数模式
    htim1.Init.Period = 1000 - 1;           // 自动重装载值
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;  // 时钟分频
    htim1.Init.RepetitionCounter = 0;       // 重复计数器
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;  // 自动重载预装载
    
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    /* 3. 配置时钟源（使用内部时钟）*/
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    /* 4. 配置主模式（可选）*/
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    /* 5. 使能更新中断 */
    HAL_TIM_Base_Start_IT(&htim1);
    
    /* 6. 配置 NVIC（优先级分组和中断通道）*/
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 2, 1);  // 注意：HAL 库中 TIM1_UP 和 TIM10 共享中断
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

/**
  * @brief  计算两个时刻计数值的时间差（处理溢出）
  * @param  startTick-起始计数值（HAL_GetTick()返回值）
  * @param  endTick-结束计数值（HAL_GetTick()返回值）
  * @retval 时间差（单位：ms）
  * @note   自动处理计数器溢出情况，适用于测量时间间隔
  */
uint32_t Timer_GetDiffTick(uint32_t startTick, uint32_t endTick)
{
    uint32_t diffTicks = 0U;

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (0xFFFFFFFF - startTick + 1U) + endTick;
    }

    /* 转换为毫秒 */
    return diffTicks;
}

/**
  * @brief  计算此时刻与当前时刻计数值的时间差（处理溢出）
  * @param  startTick-起始计数值（HAL_GetTick()返回值）
  * @retval 时间差（单位：ms）
  * @note   自动处理计数器溢出情况，适用于测量时间间隔
  */
uint32_t Timer_GetDiffMs(uint32_t startTick)
{
    uint32_t diffTicks = 0U, endTick = HAL_GetTick();

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (0xFFFFFFFF - startTick + 1U) + endTick;
    }

    /* 转换为毫秒 */
    return diffTicks;
}

/*
void TIM1_UP_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM1))
    {
		// 用户代码
        LL_TIM_ClearFlag_UPDATE(TIM1);
    }
}
*/
