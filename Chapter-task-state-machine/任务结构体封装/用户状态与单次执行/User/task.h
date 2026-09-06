#ifndef __TASK_H
#define __TASK_H

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief 任务标签枚举
 * @note 定义系统中所有任务的唯一标识符
 */
typedef enum tagTaskListTag_T
{
    TASK_LED1,            // LED1闪烁任务
    TASK_LED2,            // LED2闪烁任务
    TASK_KEY,             // 按键检测任务
    // 任务添加在此之前 ↑
    MAX_TASK_COUNT       // 任务总数（用于数组边界检查）
} TaskListTag_T;

/**
 * @brief 任务系统状态枚举
 * @note 定义任务在调度系统中的生命周期状态
 */
typedef enum tagTaskSystemState_T
{
    TASK_SYSTEM_STATE_WAIT,     // 等待运行（计时中）
    TASK_SYSTEM_STATE_RUN,      // 运行中（准备执行）
    TASK_SYSTEM_STATE_DISABLE,  // 禁用（不参与调度）
    TASK_SYSTEM_STATE_ONCE      // 单次运行（执行后自动禁用）
} TaskSystemState_T;

/**
 * @brief 任务控制块结构体
 * @note 包含任务的所有控制参数和状态信息
 */
typedef struct tagTask_T
{
    /* 用户定义区域 */
    uint32_t (*Callback)(uint16_t State, uint16_t TickCount);  // 任务回调函数指针
    uint16_t DefaultPeriod;  // 默认触发周期（单位：ms）

    /* 系统变量区域 */
    volatile uint16_t TickCount;      // 当前计时值（单位：ms）
    uint16_t UserState;      // 用户自定义状态（由回调函数维护）
    volatile uint16_t RealPeriod;     // 实际触发周期（可动态修改）
    volatile uint8_t SystemState;  // 任务当前状态（见TaskSystemState_T）
} Task_T;

/* 任务回调返回值构造宏 */
#define TASK_RENTUN(State, Period) (((uint32_t)(State & 0xFFFF) << 16) | (Period & 0xFFFF))
#define TASK_RET_INTERVAL  0        // 使用默认周期调度
#define TASK_RET_EXIT      0xFFFF   // 下次调度时禁用任务
#define TASK_DEFAULT_RET           TASK_RENTUN(0, TASK_RET_INTERVAL)  // 默认返回值
#define TASK_DEFAULT_INIT          TASK_SYSTEM_STATE_WAIT, 0, 0, 0    // 默认初始化值

/* 系统接口函数声明 */
void Task_Init(Task_T *TaskList);  // 初始化任务调度器
void Task_Tick(void);              // 系统节拍处理（定时器中断调用）
void Task_Loop(void);              // 任务调度主循环（主循环调用）

void Task_SetState(TaskListTag_T TaskTag, uint16_t State);  // 设置任务用户状态
uint16_t Task_GetState(TaskListTag_T TaskTag);  // 获取任务用户状态

void Task_Enable(TaskListTag_T TaskTag);    // 启用任务
void Task_Disable(TaskListTag_T TaskTag);   // 禁用任务
void Task_RunImmediately(TaskListTag_T TaskTag);  // 立即执行任务
void Task_RunOnce(TaskListTag_T TaskTag);   // 单次执行任务

#endif
