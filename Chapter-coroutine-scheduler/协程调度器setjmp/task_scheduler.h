/**
  **********************************************************************************************************************
  * @file    task_scheduler.h
  * @brief   协作式任务调度器 API 声明
  * @author  const_zpc (adapted)
  * @date    2026-09-05
  **********************************************************************************************************************
  */

#ifndef __TASK_SCHEDULER_H
#define __TASK_SCHEDULER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码 */
#define TASK_ERROR_NOT_INIT               -1
#define TASK_ERROR_STACK_IS_NULL          -2
#define TASK_ERROR_STACK_OVERLAPPING      -3
#define TASK_ERROR_TCB_MEM_FAIL           -4
#define TASK_ERROR_TCB_MEM_STACK_FAIL     -5

/* 任务入口函数类型 */
typedef void (*TaskEntry_f)(int arg);

/* 获取系统时间回调类型（单位：毫秒） */
typedef uint32_t (*TaskGetTime_f)(void);

/* 任务 ID 类型（返回值为 int，负值表示错误） */
typedef int TaskId_t;

/* 条件变量类型 */
typedef struct __TaskCondition_T {
    uint8_t flag;
} TaskCondition_T;

/* 调度器初始化 */
extern void Task_Init(TaskGetTime_f pfnGetTime);

/* 启动调度器（不会返回） */
extern int Task_Start(void);

/* 创建独立栈任务 */
extern TaskId_t Task_CreateWithStack(TaskEntry_f pfnEntry, const char *pName,
                                     void *pStack, size_t stackSize, int arg);

/* 创建共享栈任务 */
extern TaskId_t Task_Create(TaskEntry_f pfnEntry, const char *pName, int arg);

/* 当前任务延时（毫秒） */
extern void Task_Wait(uint32_t timeMs);

/* 等待指定任务退出 */
extern void Task_Join(TaskId_t taskId);

/* 获取当前任务 PID */
extern uint16_t Task_GetPid(void);

/* 条件变量初始化 */
extern void Task_ConditionInit(TaskCondition_T *pCondition);

/* 等待条件变量 */
extern void Task_ConditionWait(TaskCondition_T *pCondition);

/* 通知条件变量 */
extern void Task_ConditionNotify(TaskCondition_T *pCondition);

/* 打印任务列表（调试用，需自行实现 printf） */
extern void Task_Top(void);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_SCHEDULER_H */