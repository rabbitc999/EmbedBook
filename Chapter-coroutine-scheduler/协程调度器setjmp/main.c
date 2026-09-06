#include "task_scheduler.h"
#include <stdio.h>

/* 系统时间获取函数（示例） */
#ifdef _WIN32
#include <windows.h>
uint32_t GetTimerMs(void) { return GetTickCount(); }
#else
#include <sys/time.h>
uint32_t GetTimerMs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#endif

/* 全局条件变量 */
TaskCondition_T g_Condition;

/* 任务栈（独立栈） */
static uint8_t s_Stack1[1024 * 10];
static uint8_t s_Stack2[1024 * 10];

/* 任务函数示例 */
void TaskFunc1(int arg)
{
    int cnt = 5;
    printf("[%u ms][%u] TaskFunc1 start, arg=%d\n", GetTimerMs(), Task_GetPid(), arg);
    while (cnt--) {
        Task_Wait(1000);
        printf("[%u ms][%u] TaskFunc1 loop\n", GetTimerMs(), Task_GetPid());
        Task_ConditionNotify(&g_Condition);
    }
    printf("[%u ms][%u] TaskFunc1 exit\n", GetTimerMs(), Task_GetPid());
}

void TaskFunc2(int arg)
{
    printf("[%u ms][%u] TaskFunc2 start, arg=%d\n", GetTimerMs(), Task_GetPid(), arg);
    while (1) {
        Task_ConditionWait(&g_Condition);
        printf("[%u ms][%u] TaskFunc2 got condition\n", GetTimerMs(), Task_GetPid());
    }
}

int main(void)
{
    Task_Init(GetTimerMs);
    Task_ConditionInit(&g_Condition);

    /* 创建独立栈任务 */
    Task_CreateWithStack(TaskFunc1, "task1", s_Stack1, sizeof(s_Stack1), 10);
    Task_CreateWithStack(TaskFunc2, "task2", s_Stack2, sizeof(s_Stack2), 20);

    /* 创建共享栈任务 */
    Task_Create(TaskFunc1, "task3", 30);

    printf("Starting scheduler...\n");
    Task_Start();   /* 不会返回 */
    printf("Scheduler exited (should not happen)\n");
    return 0;
}