/**
  * @file   test_simple_task.c
  * @brief  任务调度器PC仿真测试：用软件微秒时钟模拟DWT，循环调用Task_Tick模拟1ms定时器中断
  * @note   编译（VS2022 x64，工作目录为工程根目录）：
  *         cl /utf-8 /W3 /I. /ITest /Fe:Test\test_simple_task.exe /FoTest\ simple_task.c Test\test_simple_task.c
  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "simple_task.h"

/* =================================================================================== */
/* BSP仿真层实现 */
/* =================================================================================== */

static uint64_t s_SimUs = 0; /* 模拟系统微秒时钟（替代DWT计数器） */

/**
  * @brief 获取模拟微秒时钟计数值
  * @retval 当前微秒计数值
  */
uint64_t Bsp_MockGetTick(void)
{
    return s_SimUs;
}

/**
  * @brief 计算两个微秒时间点的差值
  * @param startTick 起始时间点
  * @param endTick   结束时间点
  * @retval 微秒差值
  */
uint64_t Bsp_MockGetTimeDiffUs(uint64_t startTick, uint64_t endTick)
{
    return endTick - startTick;
}

/* 模拟任务占用CPU时间（推进微秒时钟，让运行时间统计可观测） */
static void Test_BurnUs(uint32_t us)
{
    s_SimUs += us;
}

/* 模拟1ms定时器中断：推进微秒时钟并调用系统节拍 */
static void Test_TimerIsr(void)
{
    s_SimUs += 1000;
    Task_Tick();
}

/* 推进仿真时间：每1ms执行一次"定时器中断 + 主循环调度" */
static void Test_AdvanceMs(uint32_t ms)
{
    uint32_t tickIndex = 0;

    for (tickIndex = 0; tickIndex < ms; tickIndex++)
    {
        Test_TimerIsr();
        Task_Loop();
    }
}

/* =================================================================================== */
/* 测试框架 */
/* =================================================================================== */

static uint32_t s_TotalPassCount = 0; /* 通过的断言计数 */
static uint32_t s_TotalFailCount = 0; /* 失败的断言计数 */

/**
  * @brief 记录单条断言结果（支持printf风格格式化描述）
  * @param isPass 断言是否通过
  * @param desc   断言描述（格式字符串）
  */
static void Test_Report(int isPass, const char *desc, ...)
{
    va_list argList;

    printf(isPass ? "[PASS] " : "[FAIL] ");
    va_start(argList, desc);
    vprintf(desc, argList);
    va_end(argList);
    printf("\n");

    if (isPass)
    {
        s_TotalPassCount++;
    }
    else
    {
        s_TotalFailCount++;
    }
}

/* =================================================================================== */
/* 测试用任务回调 */
/* =================================================================================== */

static uint32_t s_PeriodRunCount = 0; /* 周期任务运行计数 */

/* 周期任务回调：固定10ms周期 */
static TaskRet_T Test_PeriodicCallback(TaskParam_T *param)
{
    (void)param;
    s_PeriodRunCount++;
    return Task_Delay(10);
}

static uint32_t s_DynRunCount = 0;        /* 动态周期任务运行计数 */
static TaskTick_T s_DynRunTicks[4] = {0}; /* 每次运行时的系统时刻 */

/* 动态周期任务回调：首次延时5ms，之后延时25ms */
static TaskRet_T Test_DynPeriodCallback(TaskParam_T *param)
{
    (void)param;
    if (s_DynRunCount < 4)
    {
        s_DynRunTicks[s_DynRunCount] = Task_GetSystemTick();
    }
    s_DynRunCount++;
    return Task_Delay(s_DynRunCount == 1 ? 5 : 25);
}

static uint32_t s_OnceRunCount = 0; /* 单次任务运行计数 */

/* 单次任务回调：执行一次后通过Task_Exit自动禁用 */
static TaskRet_T Test_OnceCallback(TaskParam_T *param)
{
    (void)param;
    s_OnceRunCount++;
    return Task_Exit();
}

static uint32_t s_DeleteRunCountA = 0; /* 删除测试任务A运行计数 */
static uint32_t s_DeleteRunCountB = 0; /* 删除测试任务B运行计数 */

/* 删除测试任务A回调 */
static TaskRet_T Test_DeleteCallbackA(TaskParam_T *param)
{
    (void)param;
    s_DeleteRunCountA++;
    return Task_Delay(5);
}

/* 删除测试任务B回调 */
static TaskRet_T Test_DeleteCallbackB(TaskParam_T *param)
{
    (void)param;
    s_DeleteRunCountB++;
    return Task_Delay(5);
}

static uint32_t s_StatRunCount = 0; /* 统计任务运行计数 */

/* 时间统计任务回调：每次运行占用500us模拟CPU时间 */
static TaskRet_T Test_StatCallback(TaskParam_T *param)
{
    (void)param;
    Test_BurnUs(500);
    s_StatRunCount++;
    return Task_Delay(5);
}

/* 空闲检测任务回调：长周期任务，让系统在间隙处于空闲态 */
static TaskRet_T Test_IdleCallback(TaskParam_T *param)
{
    (void)param;
    return Task_Delay(100);
}

static uint32_t s_HeartbeatCount = 0; /* 阻塞延时心跳计数 */

/* 阻塞延时心跳回调：模拟定时器中断持续到来 */
static uint8_t Test_HeartbeatCallback(void)
{
    Test_TimerIsr();
    s_HeartbeatCount++;
    return 0;
}

static uint32_t s_BreakCount = 0;     /* 可打断延时心跳计数 */
static uint8_t s_BreakReturn = 0;     /* 可打断延时的返回值 */
static TaskTick_T s_BlockStartTick = 0; /* 阻塞开始时刻 */
static TaskTick_T s_BlockEndTick = 0;   /* 阻塞结束时刻 */

/* 可打断延时心跳回调：心跳达到5次时请求打断 */
static uint8_t Test_BreakHeartbeatCallback(void)
{
    Test_TimerIsr();
    s_BreakCount++;
    return (s_BreakCount >= 5) ? 1 : 0;
}

/* 阻塞延时任务回调：任务内使用DelayUntil/DelayInterrupt进行协程式阻塞延时 */
static TaskRet_T Test_BlockDelayCallback(TaskParam_T *param)
{
    (void)param;
    s_BlockStartTick = Task_GetSystemTick();
    Task_DelayUntil(20, Test_HeartbeatCallback);
    s_BlockEndTick = Task_GetSystemTick();
    s_BreakReturn = Task_DelayInterrupt(100, Test_BreakHeartbeatCallback);
    return Task_Exit();
}

/* 演示任务回调：传感器采集，占用150us */
static TaskRet_T Test_DemoSensorCallback(TaskParam_T *param)
{
    (void)param;
    Test_BurnUs(150);
    return Task_Delay(20);
}

/* 演示任务回调：LED闪烁 */
static TaskRet_T Test_DemoLedCallback(TaskParam_T *param)
{
    (void)param;
    return Task_Delay(500);
}

/* 演示任务回调：上电初始化，仅执行一次 */
static TaskRet_T Test_DemoInitCallback(TaskParam_T *param)
{
    (void)param;
    printf("---- 演示场景：上电初始化任务执行完毕 ----\n");
    return Task_Exit();
}

/* =================================================================================== */
/* 测试用例 */
/* =================================================================================== */

/**
  * @brief 用例1：定时运行（周期任务按设定周期被调度）
  */
static void Test_VerifyPeriodicTask(void)
{
    printf("\n===== 用例1：定时运行（周期任务） =====\n");
    Task_Init();
    s_PeriodRunCount = 0;

    Task_Add(Test_PeriodicCallback, "周期任务10ms");
    Test_AdvanceMs(105); /* 首次立即运行，之后每10ms一次，期望约10~11次 */

    Test_Report((s_PeriodRunCount >= 9) && (s_PeriodRunCount <= 12), "周期任务按10ms周期调度（实际运行次数=%u）", s_PeriodRunCount);
}

/**
  * @brief 用例2：任务内Delay动态修改下次运行周期
  */
static void Test_VerifyDynamicPeriod(void)
{
    printf("\n===== 用例2：任务内Delay动态周期 =====\n");
    Task_Init();
    s_DynRunCount = 0;

    Task_Add(Test_DynPeriodCallback, "动态周期任务");
    Test_AdvanceMs(60); /* 期望运行时刻：1ms、6ms、31ms、56ms */

    {
        uint64_t gap01 = s_DynRunTicks[1] - s_DynRunTicks[0];
        uint64_t gap12 = s_DynRunTicks[2] - s_DynRunTicks[1];
        uint64_t gap23 = s_DynRunTicks[3] - s_DynRunTicks[2];

        Test_Report(s_DynRunCount == 4, "动态周期任务共运行4次（实际=%u）", s_DynRunCount);
        Test_Report((gap01 >= 4000) && (gap01 <= 6000), "第一次延时5ms生效（间隔=%llu us）", (unsigned long long)gap01);
        Test_Report((gap12 >= 24000) && (gap12 <= 26000), "第二次延时25ms生效（间隔=%llu us）", (unsigned long long)gap12);
        Test_Report((gap23 >= 24000) && (gap23 <= 26000), "第三次延时25ms生效（间隔=%llu us）", (unsigned long long)gap23);
    }
}

/**
  * @brief 用例3：单次运行（Task_Exit后自动禁用，可重新启用再次运行）
  */
static void Test_VerifyOnceTask(void)
{
    printf("\n===== 用例3：单次运行任务 =====\n");
    Task_Init();
    s_OnceRunCount = 0;

    Task_T *onceTask = Task_Add(Test_OnceCallback, "单次任务");
    Test_AdvanceMs(30);

    Test_Report(s_OnceRunCount == 1, "单次任务仅运行1次（实际=%u）", s_OnceRunCount);
    Test_Report(strcmp(Task_GetTaskStateString(onceTask), "禁用") == 0, "运行后任务进入禁用状态");

    /* 重新启用后允许再次运行一次 */
    Task_Enable(onceTask);
    Test_AdvanceMs(5);
    Test_Report(s_OnceRunCount == 2, "重新启用后可再运行一次（实际=%u）", s_OnceRunCount);
}

/**
  * @brief 用例4：任务禁用与启用
  */
static void Test_VerifyDisableEnable(void)
{
    printf("\n===== 用例4：任务禁用与启用 =====\n");
    Task_Init();
    s_PeriodRunCount = 0;

    Task_T *ctrlTask = Task_Add(Test_PeriodicCallback, "启禁控制任务");
    /* 回调固定延时10ms，此处临时改为5ms便于测试 */
    Test_AdvanceMs(12);
    {
        uint32_t countBefore = s_PeriodRunCount;

        Task_Disable(ctrlTask);
        Test_AdvanceMs(50);
        Test_Report(s_PeriodRunCount == countBefore, "禁用期间任务不被调度（前=%u 后=%u）", countBefore, s_PeriodRunCount);
        Test_Report(strcmp(Task_GetTaskStateString(ctrlTask), "禁用") == 0, "任务状态显示为禁用");

        Task_Enable(ctrlTask);
        Test_AdvanceMs(12);
        Test_Report(s_PeriodRunCount > countBefore, "启用后任务恢复调度（前=%u 后=%u）", countBefore, s_PeriodRunCount);
    }
}

/**
  * @brief 用例5：任务删除与任务池槽位复用
  */
static void Test_VerifyDeleteTask(void)
{
    printf("\n===== 用例5：任务删除 =====\n");
    Task_Init();
    s_DeleteRunCountA = 0;
    s_DeleteRunCountB = 0;

    Task_T *taskA = Task_Add(Test_DeleteCallbackA, "待删除任务A");
    Task_Add(Test_DeleteCallbackB, "存活任务B");
    Test_AdvanceMs(11);

    {
        Task_T listBuffer[TASK_MAX_COUNT];
        uint16_t listCount = 0;
        uint32_t countABefore = s_DeleteRunCountA;
        uint32_t countBBefore = s_DeleteRunCountB;

        Task_Delete(taskA);
        Test_AdvanceMs(20);

        listCount = Task_GetList(listBuffer, TASK_MAX_COUNT);
        Test_Report(listCount == 1, "删除后任务列表仅剩1个任务（实际=%u）", listCount);
        Test_Report(s_DeleteRunCountA == countABefore, "被删除任务不再运行（前=%u 后=%u）", countABefore, s_DeleteRunCountA);
        Test_Report(s_DeleteRunCountB > countBBefore, "存活任务继续运行（前=%u 后=%u）", countBBefore, s_DeleteRunCountB);

        /* 槽位复用：重新添加任务后列表恢复为2个 */
        Task_Add(Test_DeleteCallbackA, "复用槽位任务C");
        listCount = Task_GetList(listBuffer, TASK_MAX_COUNT);
        Test_Report(listCount == 2, "删除后槽位可复用（列表任务数=%u）", listCount);
    }
}

/**
  * @brief 用例6：立即执行（不等周期到，直接唤醒任务）
  */
static void Test_VerifyRunImmediately(void)
{
    printf("\n===== 用例6：立即执行 =====\n");
    Task_Init();
    s_PeriodRunCount = 0;

    Task_T *lazyTask = Task_Add(Test_PeriodicCallback, "长周期任务");
    /* 回调固定延时10ms，先跑2ms（含首次运行） */
    Test_AdvanceMs(2);
    {
        uint32_t countBefore = s_PeriodRunCount;

        Task_RunImmediately(lazyTask);
        Task_Loop();

        Test_Report(s_PeriodRunCount == countBefore + 1, "立即执行唤醒任务（前=%u 后=%u）", countBefore, s_PeriodRunCount);
    }
}

/**
  * @brief 用例7：任务情况输出（字符串缓冲区安全写入）
  */
static void Test_VerifyInfoOutput(void)
{
    printf("\n===== 用例7：任务情况输出 =====\n");
    Task_Init();
    s_PeriodRunCount = 0;

    Task_Add(Test_PeriodicCallback, "周期任务10ms");
    Test_AdvanceMs(25);

    {
        char infoBuffer[2048];
        uint32_t canaryIndex = 0;
        char canaryBuffer[512];
        int canaryIntact = 1;

        /* 大缓冲区：正常输出，包含任务字符串标识 */
        memset(infoBuffer, 0, sizeof(infoBuffer));
        Task_GetInfo(infoBuffer, sizeof(infoBuffer));
        Test_Report(strlen(infoBuffer) < sizeof(infoBuffer), "大缓冲区完整输出（长度=%u）", (uint32_t)strlen(infoBuffer));
        Test_Report(strstr(infoBuffer, "周期任务10ms") != NULL, "输出包含任务字符串标识");
        Test_Report(strstr(infoBuffer, "系统总运行时间") != NULL, "输出包含系统运行时间");

        /* 小缓冲区：故意传入32字节长度，缓冲区实际512字节，尾部作金丝雀内存检测越界 */
        memset(canaryBuffer, (int)0xCC, sizeof(canaryBuffer));
        Task_GetInfo(canaryBuffer, 32);
        Test_Report(canaryBuffer[31] == '\0', "小缓冲区输出以字符串终止符结尾");
        for (canaryIndex = 32; canaryIndex < sizeof(canaryBuffer); canaryIndex++)
        {
            if ((uint8_t)canaryBuffer[canaryIndex] != 0xCC)
            {
                canaryIntact = 0;
                break;
            }
        }
        Test_Report(canaryIntact == 1, "小缓冲区输出不越界（金丝雀内存完好）");
    }
}

/**
  * @brief 用例8：任务占用时间统计（单次耗时/总耗时/个体占用率）
  */
static void Test_VerifyRunTimeStats(void)
{
    printf("\n===== 用例8：任务占用时间统计 =====\n");
    Task_Init();
    s_StatRunCount = 0;

    Task_T *statTask = Task_Add(Test_StatCallback, "统计任务");
    Test_AdvanceMs(30); /* 期望运行6次，每次占用500us */

    Test_Report(s_StatRunCount == 6, "统计任务运行6次（实际=%u）", s_StatRunCount);
    Test_Report(statTask->lastRunTick == 500, "单次运行耗时统计为500us（实际=%llu）", (unsigned long long)statTask->lastRunTick);
    Test_Report(statTask->totalRunTick == 3000, "累计运行耗时统计为3000us（实际=%llu）", (unsigned long long)statTask->totalRunTick);
    Test_Report(Task_GetIndividualUsage(statTask) > 0.0f, "个体CPU占用率大于0（实际=%.2f%%）", Task_GetIndividualUsage(statTask));
    Test_Report(Task_GetTaskRunTick() >= 3000, "系统任务总运行时间同步累计（实际=%llu）", (unsigned long long)Task_GetTaskRunTick());
}

/**
  * @brief 用例9：空闲标志（无就绪任务时调度器进入空闲）
  */
static void Test_VerifyIdleFlag(void)
{
    printf("\n===== 用例9：空闲标志 =====\n");
    Task_Init();

    Task_Add(Test_IdleCallback, "空闲检测任务100ms");
    Test_AdvanceMs(2); /* 首次运行后进入长等待 */

    Test_AdvanceMs(50); /* 50ms内无就绪任务 */
    Test_Report(Task_IsIdle() == 1, "无就绪任务时调度器空闲标志置位");

    Test_AdvanceMs(50); /* 到达100ms周期，任务重新就绪并运行 */
    Test_Report(Task_IsIdle() == 1, "任务运行后队列清空，空闲标志恢复置位");
}

/**
  * @brief 用例10：同一任务不会被重复入队重复执行
  */
static void Test_VerifyNoDuplicateRun(void)
{
    printf("\n===== 用例10：队列去重 =====\n");
    Task_Init();
    s_PeriodRunCount = 0;

    Task_Add(Test_PeriodicCallback, "去重检测任务");
    Test_AdvanceMs(2); /* 首次运行1次 */

    {
        uint32_t loopIndex = 0;
        for (loopIndex = 0; loopIndex < 10; loopIndex++)
        {
            Task_Loop(); /* 队列为空且周期未到，不应产生任何执行 */
        }
    }
    Test_Report(s_PeriodRunCount == 1, "连续多次Loop调用不会重复执行未到期任务（实际=%u）", s_PeriodRunCount);
}

/**
  * @brief 用例11：空指针与空队列安全性
  */
static void Test_VerifyNullSafety(void)
{
    printf("\n===== 用例11：空指针与空队列安全 =====\n");
    Task_Init();

    Task_Delete(NULL);
    Task_Disable(NULL);
    Task_Enable(NULL);
    Task_RunImmediately(NULL);
    Task_SetTaskName(NULL, "空指针");
    Task_GetTaskName(NULL);
    Task_SetErrorCallback(NULL);
    Task_GetInfo(NULL, 100);

    {
        char tempBuffer[16];
        Task_GetInfo(tempBuffer, 0); /* 长度为0的缓冲区 */
    }

    Task_Loop(); /* 空队列调度 */
    Test_Report(1, "全部空指针/空队列调用未崩溃");
}

/**
  * @brief 用例12：任务内阻塞延时（DelayUntil / DelayInterrupt），阻塞期间保持其他任务调度
  */
static void Test_VerifyBlockingDelay(void)
{
    printf("\n===== 用例12：任务内阻塞延时 =====\n");
    Task_Init();
    s_HeartbeatCount = 0;
    s_BreakCount = 0;
    s_PeriodRunCount = 0;

    Task_Add(Test_BlockDelayCallback, "阻塞延时任务");
    Task_Add(Test_PeriodicCallback, "协作任务10ms");
    Test_AdvanceMs(2); /* 首次运行即触发阻塞延时流程：DelayUntil(20ms) + DelayInterrupt(100ms) */

    {
        uint64_t blockElapsed = s_BlockEndTick - s_BlockStartTick;

        Test_Report(blockElapsed >= 20000, "DelayUntil阻塞满20ms（实际=%llu us）", (unsigned long long)blockElapsed);
        Test_Report(s_HeartbeatCount >= 19, "阻塞期间心跳持续触发（实际=%u次）", s_HeartbeatCount);
        Test_Report(s_PeriodRunCount >= 2, "阻塞期间协作任务保持调度（实际=%u次）", s_PeriodRunCount);
        Test_Report((s_BreakReturn == 1) && (s_BreakCount == 5), "DelayInterrupt被心跳打断并提前退出（返回=%u 心跳=%u）", s_BreakReturn, s_BreakCount);
    }
}

/* =================================================================================== */
/* 演示场景与信息输出 */
/* =================================================================================== */

/**
  * @brief 演示场景：构造典型任务集运行后，输出完整任务情况表
  * @note  在模拟中断内调用实时任务计时接口，展示实时任务CPU占用统计
  */
static void Test_RunDemo(void)
{
    char infoBuffer[2048];
    uint32_t tickIndex = 0;

    printf("\n===== 演示场景：任务情况输出 =====\n");
    Task_Init();
    Task_Add(Test_DemoInitCallback, "上电初始化");
    Task_Add(Test_DemoSensorCallback, "传感器采集20ms");
    Task_Add(Test_DemoLedCallback, "LED闪烁500ms");

    /* 推进100ms仿真时间，每20ms模拟一次定时器中断内的实时任务（占用100us） */
    for (tickIndex = 0; tickIndex < 100; tickIndex++)
    {
        if ((tickIndex % TASK_READ_TASK_PEROID) == 0)
        {
            Task_ReadTimeTaskBegin();
            Test_BurnUs(100);
            Task_ReadTimeTaskEnd();
        }
        Test_TimerIsr();
        Task_Loop();
    }

    Test_Report(Task_GetReadTimeTaskRunTick() > 0, "实时任务计时统计有效（累计=%llu us）", (unsigned long long)Task_GetReadTimeTaskRunTick());

    memset(infoBuffer, 0, sizeof(infoBuffer));
    Task_GetInfo(infoBuffer, sizeof(infoBuffer));
    printf("%s\n", infoBuffer);
}

/* =================================================================================== */
/* 入口 */
/* =================================================================================== */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* 无缓冲输出，崩溃时保留已完成用例的打印 */

    printf("============================================================\n");
    printf("  simple_task 任务调度器 PC仿真测试\n");
    printf("  模拟环境：1ms定时器中断 + DWT微秒时钟（软件模拟）\n");
    printf("============================================================\n");

    Test_VerifyPeriodicTask();   /* 定时运行 */
    Test_VerifyDynamicPeriod();  /* 任务内Delay动态周期 */
    Test_VerifyOnceTask();       /* 单次运行 */
    Test_VerifyDisableEnable();  /* 启用禁用 */
    Test_VerifyDeleteTask();     /* 任务删除 */
    Test_VerifyRunImmediately(); /* 立即执行 */
    Test_VerifyInfoOutput();     /* 情况输出 */
    Test_VerifyRunTimeStats();   /* 时间统计 */
    Test_VerifyIdleFlag();       /* 空闲标志 */
    Test_VerifyNoDuplicateRun(); /* 队列去重 */
    Test_VerifyNullSafety();     /* 空指针安全 */
    Test_VerifyBlockingDelay();  /* 阻塞延时 */
    Test_RunDemo();              /* 演示输出 */

    printf("\n============================================================\n");
    printf("  测试结果：通过 %u 项，失败 %u 项\n", s_TotalPassCount, s_TotalFailCount);
    printf("============================================================\n");

    return (s_TotalFailCount == 0) ? 0 : 1;
}
