/**
 * @file simple_task_config.h
 * @brief 轻量级任务调度器用户配置文件
 * @version 2.0
 * @date 2026-08-20
 * 
 * 用户可根据项目需求修改此文件中的宏定义，以裁剪调度器功能。
 * 所有配置均在此集中管理，便于移植和优化。
 */

#ifndef __SIMPLE_TASK_CONFIG_H
#define __SIMPLE_TASK_CONFIG_H

/* =================================================================================== */
/* 核心参数 */
/* =================================================================================== */

/** @brief 系统支持的最大普通任务数量（静态分配） */
#define TASK_MAX_COUNT            16

/** @brief 系统节拍周期（单位：ms），即 Task_Tick() 的调用间隔 */
#define TASK_TICK_PERIOD          1

/** @brief 任务在队列中等待的超时时间（单位：ms），超时后触发错误回调 */
#define TASK_QUEUE_TIMEOUT        1000

/** @brief 毫秒级时间差值阈值（单位：ms），超过此值切换计时基准（仅用户定时器模式） */
#define TASK_MS_THRESHOLD         1000

/** @brief 调试模式开关（1=启用，0=禁用），影响空闲时CPU睡眠行为 */
#define TASK_DEBUG_MODE           1

/* =================================================================================== */
/* 功能开关（根据需要开启/关闭以节省资源） */
/* =================================================================================== */

/** @brief 是否使用用户高精度定时器（如DWT）进行微秒级计时 */
#define TASK_USE_USER_TIMER       1

/** @brief 是否启用FIFO队列调度（1=启用，0=轮询模式） */
#define TASK_USE_QUENE            1

/** @brief 是否启用任务运行时间统计（个体占用率、总占用率） */
#define TASK_USE_TASK_TIMER       1

/** @brief 是否启用空闲标志（可用于CPU睡眠） */
#define TASK_USE_IDLE_FLAG        1

/** @brief 是否启用任务“单次执行”状态（运行一次后自动禁用） */
#define TASK_USE_STATE_ONCE       0

/** @brief 是否启用任务“禁用”状态（不参与调度） */
#define TASK_USE_STATE_DISABLE    1

/** @brief 是否启用用户自定义状态（由任务回调维护） */
#define TASK_USE_USER_STATE       0

/** @brief 是否启用用户参数指针（可指向任意数据结构） */
#define TASK_USE_USER_PARAM       0

/** @brief 是否启用任务名称（便于调试和信息输出） */
#define TASK_USE_TASK_NAME        1

/** @brief 是否启用高级功能（Task_DelayUntil、Task_DelayInterrupt） */
#define TASK_USE_ADVANCE_FUNCTION 1

/** @brief 是否启用实时任务计时（测量代码段执行时间） */
#define TASK_USE_REAL_TASK_TIMER  1

/** @brief 实时任务计时周期（单位：ms），用于Task_GetInfo显示 */
#define TASK_READ_TASK_PEROID     20

/* =================================================================================== */
/* 硬实时任务配置（高级功能） */
/* =================================================================================== */

/**
 * @brief 是否启用硬实时任务（在中断中执行，保证严格周期性）
 * @note 启用后需配置 TASK_HARD_REAL_TIME_MAX_COUNT
 */
#define TASK_USE_HARD_REAL_TIME_TASK    1

/** @brief 最大硬实时任务数量（推荐1~2，过多会增加中断处理时间） */
#define TASK_HARD_REAL_TIME_MAX_COUNT   2

#endif /* __SIMPLE_TASK_CONFIG_H */