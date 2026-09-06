#include "bsp_dwt.h"
#include "bsp_timer.h"

/* 静态全局变量 */
static uint8_t s_DWT_InitFlag = 0U;  /* DWT初始化完成标志 */

/**
  * @brief  初始化DWT定时器（使能CYCCNT计数器）
  * @retval 0-初始化成功，1-初始化失败（DWT未使能）
  * @note   需在系统初始化阶段调用，依赖HAL库初始化完成
  */
uint8_t DWT_Init(void)
{
    /* 检查是否已初始化 */
    if (s_DWT_InitFlag == 1U)
    {
        return 0U;
    }

    /* 1. 使能DWT外设（DEMCR寄存器TRCENA位） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 2. 清零CYCCNT计数器 */
    DWT->CYCCNT = 0U;

    /* 3. 使能CYCCNT计数器（DWT_CTRL寄存器CYCCNTENA位） */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 4. 验证初始化结果 */
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U)
    {
        s_DWT_InitFlag = 1U;
        return 0U;
    }
    else
    {
        return 1U;
    }
}

/**
  * @brief  获取DWT当前计数值（32位无符号数）
  * @retval 当前CYCCNT寄存器值（单位：时钟周期）
  * @note   计数器溢出后自动归零，溢出周期约8.9秒（480MHz主频）
  */
uint32_t DWT_GetTick(void)
{
    /* 未初始化则返回0 */
    if (s_DWT_InitFlag == 0U)
    {
        return 0U;
    }

    return DWT->CYCCNT;
}

/**
  * @brief  微秒级精准延时
  * @param  us-延时时长（单位：μs），范围：1~DWT_MAX_DELAY_US
  * @retval 无
  * @note   基于CYCCNT计数器实现，不占用CPU中断资源
  */
void DWT_DelayUs(uint32_t us)
{
    uint32_t startTick = 0U, startHalTick;
    uint32_t targetTick = 0U, deltaHalTick;

    /* 参数检查：超出范围则返回 */
    if ((s_DWT_InitFlag == 0U) || (us == 0U) || (us > DWT_MAX_DELAY_US))
    {
        return;
    }

    /* 计算目标计数值 = 当前计数值 + 微秒×每微秒周期数 */
    startTick = DWT->CYCCNT;
    targetTick = startTick + (us * DWT_TICK_PER_US);
    startHalTick = HAL_GetTick();
    deltaHalTick = us / 1000 + 1;

    /* 等待计数器达到目标值（处理溢出） */
    if (targetTick >= startTick)
    {
        while (DWT->CYCCNT < targetTick && Timer_GetDiffMs(startHalTick) <= deltaHalTick);
    }
    else
    {
        /* 溢出后：先等计数器到最大值，再等计数器到目标值 */
        while (DWT->CYCCNT >= startTick && Timer_GetDiffMs(startHalTick) <= deltaHalTick);
        while (DWT->CYCCNT < targetTick && Timer_GetDiffMs(startHalTick) <= deltaHalTick);
    }
}

/**
  * @brief  纳秒级精准延时
  * @param  us-延时时长（单位：μs），范围：1~DWT_MAX_DELAY_US
  * @retval 无
  * @note   基于CYCCNT计数器实现，不占用CPU中断资源
  */
void DWT_DelayNs(uint32_t ns)
{
    uint32_t startTick = 0U;
    uint32_t targetTick = 0U;

    /* 参数检查：超出范围则返回 */
    if ((s_DWT_InitFlag == 0U) || (ns == 0U) || (ns > DWT_MAX_DELAY_NS))
    {
        return;
    }

    /* 计算目标计数值 = 当前计数值 + 微秒×每微秒周期数 */
    startTick = DWT->CYCCNT;
    targetTick = startTick + (ns / 4);

    /* 等待计数器达到目标值（处理溢出） */
    if (targetTick >= startTick)
    {
        while (DWT->CYCCNT < targetTick);
    }
    else
    {
        /* 溢出后：先等计数器到最大值，再等计数器到目标值 */
        while (DWT->CYCCNT >= startTick);
        while (DWT->CYCCNT < targetTick);
    }
}

/**
  * @brief  毫秒级精准延时
  * @param  ms-延时时长（单位：ms），范围：1~DWT_MAX_DELAY_MS
  * @retval 无
  * @note   内部调用微秒延时函数，超长延时需自行处理溢出
  */
void DWT_DelayMs(uint32_t ms)
{
    /* 参数检查：超出范围则返回 */
    if ((ms == 0U) || (ms > DWT_MAX_DELAY_MS))
    {
        return;
    }

    /* 循环调用微秒延时（1ms = 1000μs） */
    while (ms--)
    {
        DWT_DelayUs(1000U);
    }
}

/**
  * @brief  计算两个DWT计数值的时间差（处理溢出）
  * @param  startTick-起始计数值（DWT_GetTick()返回值）
  * @param  endTick-结束计数值（DWT_GetTick()返回值）
  * @retval 时间差（单位：μs）
  * @note   自动处理计数器溢出情况，适用于测量短时间间隔
  */
uint32_t DWT_GetTimeDiffUs(uint32_t startTick, uint32_t endTick)
{
    uint32_t diffTicks = 0U;

    /* 未初始化则返回0 */
    if (s_DWT_InitFlag == 0U)
    {
        return 0U;
    }

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (DWT_MAX_DELAY_US - startTick + 1U) + endTick;
    }

    /* 转换为微秒（计数值差 ÷ 每微秒周期数） */
    return (diffTicks / DWT_TICK_PER_US);
}

/**
  * @brief  计算此时刻与当前时刻计数值的时间差（处理溢出）
  * @param  startTick-起始计数值（DWT_GetTick()返回值）
  * @retval 时间差（单位：us）
  * @note   自动处理计数器溢出情况，适用于测量短时间间隔
  */
uint32_t DWT_GetDiffUs(uint32_t startTick)
{
    uint32_t diffTicks = 0U, endTick = DWT_GetTick();

    /* 未初始化则返回0 */
    if (s_DWT_InitFlag == 0U)
    {
        return 0U;
    }

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (DWT_MAX_DELAY_US - startTick + 1U) + endTick;
    }

    /* 转换为微秒（计数值差 ÷ 每微秒周期数） */
    return (diffTicks / DWT_TICK_PER_US);
}