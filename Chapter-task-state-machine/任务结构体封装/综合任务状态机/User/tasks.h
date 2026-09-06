#ifndef __TASK_H
#define __TASK_H

#include <stdint.h>

#define MAX_TASK 30

typedef enum tagTaskFlag_H {
    TASK_FLAG_WAIT,
    TASK_FLAG_READY,
    TASK_FLAG_DISABLE,
    TASK_FLAG_RUNONCE,
} TaskFlag_H;

typedef enum tagTaskUseState_H {
    TASK_USE_STATE_USED,
    TASK_USE_STATE_UNUSED,
} TaskUseState_H;

typedef uint32_t (*TaskCallback)(uint16_t UserState, uint16_t DeltaTick);

typedef struct tagTask_H
{
	volatile uint16_t Tick;		    // 计数器变量
	volatile uint16_t Period;        // 最大计数值
	TaskCallback Callback;  // 回调函数
	volatile TaskFlag_H Flag;        // 标志位
    volatile TaskUseState_H UseFlag;        // 是否被分配
    uint16_t UserState;
    uint16_t MaxTick;
} Task_H;

#define TASK_RETURN(State, DeltaTick)    (((DeltaTick << 16)) | (State))

// 一种机制：到指定时间置标志位（定时器）
// 主循环：检测标志位，如果该标志位不为0，则执行这个回调函数

void Task_Init(void);  // 初始化
void Task_Loop(void);  // 主循环
void Task_Tick(void);

Task_H *Task_Add(TaskCallback Callback, uint16_t Period);
void Task_Delete(Task_H *TCB);
void Task_Enable(Task_H *TCB);
void Task_Disable(Task_H *TCB);
void Task_RunOnce(Task_H *TCB);

#endif
