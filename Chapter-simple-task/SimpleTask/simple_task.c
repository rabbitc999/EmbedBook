/**
 * @file simple_task.c
 * @brief 轻量级协作式任务调度器实现
 * @version 2.0
 * @date 2026-08-20
 */

#include "simple_task.h"
#include <stdarg.h>

/* =================================================================================== */
/* 全局任务列表实例 */
/* =================================================================================== */

/** @brief 任务列表全局变量（静态分配） */
static TaskList_T s_TaskList;

/* =================================================================================== */
/* 循环队列操作（仅当启用队列时） */
/* =================================================================================== */

#if TASK_USE_QUENE

/**
 * @brief 任务入队（FIFO）
 * @param taskPtr 待入队的任务控制块指针
 * @note 若任务已在队列或队列满，则忽略
 */
void Task_QueueEnqueue(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    if (taskPtr->isInMsgQuene) return; /* 已在队列则跳过 */

    TASK_DISABLE_TIMER_IRQ(); /* 临界区保护 */

    /* 检查队列是否已满（循环队列：尾+1 == 头） */
    uint8_t nextRear = (s_TaskList.runQueneRear + 1) % (TASK_MAX_COUNT + 1);
    if (nextRear == s_TaskList.runQueneFirst) {
        TASK_ENABLE_TIMER_IRQ();
        return;
    }

    /* 入队操作 */
    s_TaskList.runQuene[s_TaskList.runQueneRear] = taskPtr;
    s_TaskList.runQueneRear = nextRear;
    taskPtr->isInMsgQuene = 1;
    taskPtr->tickTimeout = 0;          /* 重置超时计数器 */
    s_TaskList.isIdleFlag = 0;         /* 有任务就绪，非空闲 */

    TASK_ENABLE_TIMER_IRQ();
}

/**
 * @brief 任务出队（FIFO）
 * @return 队首任务指针，队列空时返回 NULL
 * @note 出队后清除任务的队列标记
 */
Task_T *Task_QueueDequeue(void)
{
    if (s_TaskList.runQueneFirst == s_TaskList.runQueneRear) {
        return NULL; /* 队列为空 */
    }

    TASK_DISABLE_TIMER_IRQ();

    Task_T *taskPtr = s_TaskList.runQuene[s_TaskList.runQueneFirst];
    s_TaskList.runQueneFirst = (s_TaskList.runQueneFirst + 1) % (TASK_MAX_COUNT + 1);
    taskPtr->isInMsgQuene = 0; /* 标记已出队 */

    TASK_ENABLE_TIMER_IRQ();
    return taskPtr;
}

/**
 * @brief 检查任务是否在队列中
 * @param taskPtr 任务指针
 * @return 1=在队列中，0=不在
 */
uint8_t Task_QueueContains(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return 0;

    TASK_DISABLE_TIMER_IRQ();

    uint8_t index = s_TaskList.runQueneFirst;
    while (index != s_TaskList.runQueneRear) {
        if (s_TaskList.runQuene[index] == taskPtr) {
            TASK_ENABLE_TIMER_IRQ();
            return 1;
        }
        index = (index + 1) % (TASK_MAX_COUNT + 1);
    }

    TASK_ENABLE_TIMER_IRQ();
    return 0;
}
#endif /* TASK_USE_QUENE */

/* =================================================================================== */
/* 内部辅助函数 */
/* =================================================================================== */

/**
 * @brief 检查任务状态是否异常（非预期状态）
 * @param taskPtr 任务指针
 * @return 1=异常（需修复），0=正常
 * @note 异常状态包括未定义的枚举值，会被强制重置为 WAIT
 */
static uint8_t Task_IsNotNormalFlag(Task_T *taskPtr)
{
    return taskPtr->systemState != TASK_SYSTEM_STATE_WAIT
           && taskPtr->systemState != TASK_SYSTEM_STATE_READY
#if TASK_USE_STATE_DISABLE
           && taskPtr->systemState != TASK_SYSTEM_STATE_DISABLE
#endif
#if TASK_USE_STATE_ONCE
           && taskPtr->systemState != TASK_SYSTEM_STATE_ONCE
#endif
           && taskPtr->systemState != TASK_SYSTEM_STATE_RUN;
}

/* =================================================================================== */
/* 系统节拍处理（在定时器中断中调用） */
/* =================================================================================== */

/**
 * @brief 系统节拍处理函数
 * @note 必须在 TASK_TICK_PERIOD 间隔的定时器中断中调用
 * 
 * 功能：
 * 1. 更新所有等待/就绪任务的 tickCount
 * 2. 当 tickCount 达到 realPeriod 时，将任务置为 READY 并加入队列
 * 3. 自动修复异常状态的任务
 * 4. 执行硬实时任务（如果启用）
 * 5. 更新系统时间
 */
void Task_Tick(void)
{
    uint16_t taskNum = 0;

    /* --- 处理普通任务 --- */
    for (taskNum = 0; taskNum < s_TaskList.maxTaskId; taskNum++) {
        Task_T *taskPtr = &s_TaskList.taskPool[taskNum];
        if (!taskPtr->isUsed) continue;

        /* 异常状态修复 */
        if (Task_IsNotNormalFlag(taskPtr)) {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
#if TASK_USE_QUENE
            taskPtr->isInMsgQuene = 0;
            taskPtr->tickTimeout = 0;
#endif
            continue;
        }

        /* 仅处理 WAIT 或 READY 状态的任务 */
        if (taskPtr->systemState == TASK_SYSTEM_STATE_WAIT ||
            taskPtr->systemState == TASK_SYSTEM_STATE_READY) {
            taskPtr->tickCount += TASK_TICK_PERIOD;
            if (taskPtr->tickCount >= taskPtr->realPeriod) {
                taskPtr->systemState = TASK_SYSTEM_STATE_READY;
#if TASK_USE_QUENE
                Task_QueueEnqueue(taskPtr);
#endif
                s_TaskList.isIdleFlag = 0;
            }
        }
    }

    /* ========== 处理硬实时任务（在中断上下文直接执行） ========== */
#if TASK_USE_HARD_REAL_TIME_TASK
    for (uint8_t i = 0; i < TASK_HARD_REAL_TIME_MAX_COUNT; i++) {
        HardRealTimeTask_T *rtTask = &s_TaskList.hardRealTimePool[i];
        if (!rtTask->isUsed) continue;

        rtTask->tickCount += TASK_TICK_PERIOD;
        if (rtTask->tickCount >= rtTask->period) {
            rtTask->tickCount = 0;

            /* 构造任务参数（与普通任务一致） */
            TaskParam_T param = {
                .currentTask = NULL,
                .currentPeriod = rtTask->period,
#if TASK_USE_TASK_NAME
                .taskName = rtTask->taskName,
#endif
#if TASK_USE_USER_STATE
                .userState = rtTask->userState,
#endif
#if TASK_USE_USER_PARAM
                .userParam = rtTask->userParam,
#endif
            };

            if (IS_VALID_FUNCTION_POINTER(rtTask->callback)) {
                TaskRet_T ret = rtTask->callback(&param);
#if TASK_USE_USER_STATE
                rtTask->userState = ret.state;
#endif
                /* 动态更新周期（最小为1ms） */
                if (ret.nextPeriod >= 1) {
                    rtTask->period = ret.nextPeriod;
                }
            }
        }
    }
#endif
    /* ============================================================= */

    /* 更新系统时钟 */
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    s_TaskList.systemTick += TASK_TICK_PERIOD * 1000; /* us */
#else
    s_TaskList.systemTick += TASK_TICK_PERIOD;        /* ms */
#endif
}

/* =================================================================================== */
/* 任务执行核心 */
/* =================================================================================== */

/**
 * @brief 执行单个任务（由 Task_Loop 调用）
 * @param taskPtr 任务控制块指针
 * @note 该函数会调用任务回调，处理返回值，并更新任务状态和计时统计
 */
void Task_RunTask(Task_T *taskPtr)
{
    TaskRet_T temp = { 0 };
    TaskSystemState_T lastState = taskPtr->systemState;

    /* 记录开始时间（若启用了任务计时） */
#if TASK_USE_TASK_TIMER
#if TASK_USE_USER_TIMER
    TaskTick_T startRunTimingTick = TASK_TIMER_GET_TICK();
    TaskTick_T startRunSystemTick = s_TaskList.systemTick;
#else
    TaskTick_T startRunTick = s_TaskList.systemTick;
#endif
    taskPtr->systemState = TASK_SYSTEM_STATE_RUN;
    s_TaskList.taskNestDepth += 1;
#endif

    /* 调用回调函数 */
    if (IS_VALID_FUNCTION_POINTER(taskPtr->callback)) {
        TaskParam_T param = {
#if TASK_USE_USER_STATE
            .userState = taskPtr->userState,
#endif
#if TASK_USE_TASK_NAME
            .taskName = taskPtr->taskName,
#endif
#if TASK_USE_USER_PARAM
            .userParam = taskPtr->userParam,
#endif
            .currentPeriod = taskPtr->realPeriod,
            .currentTask = taskPtr
        };
        temp = taskPtr->callback(&param);
    }

    /* 计算运行时间（若启用） */
#if TASK_USE_TASK_TIMER
#if TASK_USE_USER_TIMER
    if (s_TaskList.systemTick - startRunSystemTick > TASK_MS_THRESHOLD * 1000) {
        /* 若耗时超过阈值，使用 systemTick 差值（us） */
        taskPtr->lastRunTick = s_TaskList.systemTick - startRunSystemTick;
    } else {
        /* 否则使用高精度定时器差值（us） */
        TaskTick_T endRunTimingTick = TASK_TIMER_GET_TICK();
        TaskTick_T highResDiff = TASK_TIMER_CALC_DIFF_US(startRunTimingTick, endRunTimingTick);
        taskPtr->lastRunTick = highResDiff;
    }
#else
    /* 非高精度模式，直接使用 ms 差值 */
    uint32_t msDiff = s_TaskList.systemTick - startRunTick;
    taskPtr->lastRunTick = msDiff;
#endif
    taskPtr->totalRunTick += taskPtr->lastRunTick;

    /* 仅当嵌套深度为1时累加总运行时间（避免重复计费） */
    if (s_TaskList.taskNestDepth <= 1) {
        s_TaskList.taskRunTick += taskPtr->lastRunTick;
    }
    s_TaskList.taskNestDepth -= 1;
#endif

    /* 更新用户状态和周期 */
#if TASK_USE_USER_STATE
    taskPtr->userState = temp.state;
#endif
    taskPtr->realPeriod = temp.nextPeriod;
    taskPtr->tickCount = 0; /* 重置计时器，准备下一轮 */

    /* 根据配置更新系统状态 */
#if TASK_USE_STATE_ONCE
    if (taskPtr->realPeriod == TASK_RETURN_EXIT || lastState == TASK_SYSTEM_STATE_ONCE) {
        taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE;
    } else {
        taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
    }
#elif TASK_USE_STATE_DISABLE
    if (taskPtr->realPeriod == TASK_RETURN_EXIT) {
        taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE;
    } else {
        taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
    }
#else
    taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
#endif
}

/* =================================================================================== */
/* 调度主循环 */
/* =================================================================================== */

/**
 * @brief 任务调度主循环
 * @return 所有就绪任务中最小的周期值（用于动态调频）
 * @note 该函数应在主循环中反复调用，执行所有就绪任务
 */
uint16_t Task_Loop(void)
{
    uint8_t isRunning = 0;
    uint32_t minPeriod = 0xFFFF;

#if TASK_USE_QUENE
    /* 队列模式：依次取出队列中的任务执行 */
    Task_T *taskPtr = Task_QueueDequeue();
    while (taskPtr != NULL) {
        /* 异常状态修复 */
        if (Task_IsNotNormalFlag(taskPtr)) {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
            taskPtr->isInMsgQuene = 0;
            taskPtr->tickTimeout = 0;
            taskPtr = Task_QueueDequeue();
            continue;
        }

        /* 执行任务（除非被禁用） */
        if (taskPtr->systemState != TASK_SYSTEM_STATE_DISABLE) {
            Task_RunTask(taskPtr);
        }
        isRunning = 1;
        if (taskPtr->realPeriod < minPeriod) { minPeriod = taskPtr->realPeriod; }

        /* 取下一个任务 */
        taskPtr = Task_QueueDequeue();
    }
#else
    /* 轮询模式：遍历任务池，执行处于 READY 状态的任务 */
    for (uint8_t i = 0; i < s_TaskList.maxTaskId; i++) {
        Task_T *taskPtr = &s_TaskList.taskPool[i];
        if (!taskPtr->isUsed) continue;

        if (Task_IsNotNormalFlag(taskPtr)) {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
            continue;
        }

#if TASK_USE_STATE_ONCE
        if (taskPtr->systemState == TASK_SYSTEM_STATE_READY ||
            taskPtr->systemState == TASK_SYSTEM_STATE_ONCE) {
            Task_RunTask(taskPtr);
            isRunning = 1;
        }
#else
        if (taskPtr->systemState == TASK_SYSTEM_STATE_READY) {
            Task_RunTask(taskPtr);
            isRunning = 1;
        }
#endif
    }
#endif

    /* 更新空闲标志 */
    s_TaskList.isIdleFlag = (isRunning == 0) ? 1 : 0;
    return minPeriod > 0 ? minPeriod : 1;
}

/* =================================================================================== */
/* 用户操作 API 实现 */
/* =================================================================================== */

#if TASK_USE_QUENE
/**
 * @brief 设置队列超时错误回调
 * @param callback 错误回调函数
 */
void Task_SetErrorCallback(TaskErrorCallback callback)
{
    TASK_DISABLE_TIMER_IRQ();
    s_TaskList.errorCallback = callback;
    TASK_ENABLE_TIMER_IRQ();
}
#endif

/**
 * @brief 立即执行指定任务（强制变为就绪并入队）
 * @param taskPtr 任务指针
 */
void Task_RunImmediately(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_READY;
#if TASK_USE_QUENE
    Task_QueueEnqueue(taskPtr);
#endif
    TASK_ENABLE_TIMER_IRQ();
}

#if TASK_USE_STATE_ONCE
/**
 * @brief 单次执行任务（执行后自动禁用）
 * @param taskPtr 任务指针
 */
void Task_RunOnce(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_ONCE;
#if TASK_USE_QUENE
    Task_QueueEnqueue(taskPtr);
#endif
    TASK_ENABLE_TIMER_IRQ();
}
#endif

/**
 * @brief 初始化调度器
 * @note 清零所有任务池和状态
 */
void Task_Init(void)
{
    s_TaskList.maxTaskId = 0;
    s_TaskList.systemTick = 0;
#if TASK_USE_TASK_TIMER
    s_TaskList.taskRunTick = 0;
    s_TaskList.taskNestDepth = 0;
#endif
    s_TaskList.isIdleFlag = 0;

    /* 清空普通任务池 */
    for (int i = 0; i < TASK_MAX_COUNT; i++) {
        s_TaskList.taskPool[i].isUsed = 0;
    }

    /* 清空硬实时任务池 */
#if TASK_USE_HARD_REAL_TIME_TASK
    for (int i = 0; i < TASK_HARD_REAL_TIME_MAX_COUNT; i++) {
        s_TaskList.hardRealTimePool[i].isUsed = 0;
    }
#endif
}

/**
 * @brief 添加普通任务
 * @param callback 任务回调
 * @param ... 可选参数（名称、用户参数）
 * @return 任务控制块指针，失败返回 NULL
 */
#if TASK_USE_TASK_NAME && TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, const char *taskName, void *userParam)
#elif TASK_USE_TASK_NAME
Task_T *Task_Add(TaskCallback callback, const char *taskName)
#elif TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, void *userParam)
#else
Task_T *Task_Add(TaskCallback callback)
#endif
{
    Task_T *newTaskPtr = NULL;
    uint16_t taskNum = 0;

    TASK_DISABLE_TIMER_IRQ();

    /* 在任务池中查找空闲槽位 */
    for (taskNum = 0; taskNum < s_TaskList.maxTaskId + 1 && taskNum < TASK_MAX_COUNT; taskNum++) {
        Task_T *taskPtr = &s_TaskList.taskPool[taskNum];
        if (!taskPtr->isUsed) {
            /* 初始化任务控制块 */
            taskPtr->isUsed = 1;
            taskPtr->callback = callback;
            taskPtr->realPeriod = 0;
            taskPtr->tickCount = 0;
            taskPtr->systemState = TASK_SYSTEM_STATE_READY;
#if TASK_USE_USER_STATE
            taskPtr->userState = 0;
#endif
#if TASK_USE_TASK_NAME
            taskPtr->taskName = taskName;
#endif
#if TASK_USE_USER_PARAM
            taskPtr->userParam = userParam;
#endif
            newTaskPtr = &s_TaskList.taskPool[taskNum];

            /* 更新最大任务ID */
            uint16_t currentId = taskNum + 1;
            if (currentId > s_TaskList.maxTaskId) {
                s_TaskList.maxTaskId = currentId;
            }
            break;
        }
    }

    TASK_ENABLE_TIMER_IRQ();
    return newTaskPtr;
}

/**
 * @brief 删除任务
 * @param taskPtr 任务指针
 */
void Task_Delete(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;

    TASK_DISABLE_TIMER_IRQ();
    taskPtr->isUsed = 0;

    /* 若删除的是最后一个任务，更新最大ID */
    uint16_t deletedId = (taskPtr - s_TaskList.taskPool) + 1;
    if (deletedId == s_TaskList.maxTaskId) {
        while (s_TaskList.maxTaskId > 0 && !s_TaskList.taskPool[s_TaskList.maxTaskId - 1].isUsed) {
            s_TaskList.maxTaskId--;
        }
    }

    TASK_ENABLE_TIMER_IRQ();
}

#if TASK_USE_STATE_DISABLE
/**
 * @brief 启用任务（将其置为就绪）
 * @param taskPtr 任务指针
 */
void Task_Enable(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_READY;
    taskPtr->realPeriod = 0;
    TASK_ENABLE_TIMER_IRQ();
}

/**
 * @brief 禁用任务
 * @param taskPtr 任务指针
 */
void Task_Disable(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE;
    TASK_ENABLE_TIMER_IRQ();
}
#endif

/* ---------- 用户状态、参数、名称操作（根据配置选择性实现） ---------- */
#if TASK_USE_USER_STATE
void Task_SetUserState(Task_T *taskPtr, uint16_t state) {
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->userState = state;
}
uint16_t Task_GetUserState(Task_T *taskPtr) {
    if (taskPtr == NULL || !taskPtr->isUsed) return 0;
    return taskPtr->userState;
}
#endif

#if TASK_USE_USER_PARAM
void Task_SetUserParam(Task_T *taskPtr, void *userParam) {
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->userParam = userParam;
}
void *Task_GetUserParam(Task_T *taskPtr) {
    if (taskPtr == NULL || !taskPtr->isUsed) return NULL;
    return taskPtr->userParam;
}
#endif

#if TASK_USE_TASK_NAME
void Task_SetTaskName(Task_T *taskPtr, const char *taskName) {
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->taskName = taskName;
}
const char *Task_GetTaskName(Task_T *taskPtr) {
    if (taskPtr == NULL || !taskPtr->isUsed) return NULL;
    return taskPtr->taskName;
}
#endif

/* ---------- 返回值构造函数 ---------- */
TaskRet_T Task_Return(uint32_t state, uint16_t nextPeriod) {
    TaskRet_T ret = { .nextPeriod = nextPeriod, .state = state };
    return ret;
}

TaskRet_T Task_Delay(uint16_t nextPeriod) {
    TaskRet_T ret = { .nextPeriod = nextPeriod };
    return ret;
}

#if TASK_USE_STATE_DISABLE
TaskRet_T Task_Exit(void) {
    TaskRet_T ret = { .nextPeriod = TASK_RETURN_EXIT };
    return ret;
}
#endif

/* ---------- 获取任务列表 ---------- */
uint16_t Task_GetList(Task_T *taskListPtr, uint16_t count)
{
    uint16_t validTaskCount = 0;
    if (taskListPtr == NULL || count == 0) return 0;

    TASK_DISABLE_TIMER_IRQ();

    for (uint16_t i = 0; i < s_TaskList.maxTaskId && validTaskCount < count; i++) {
        Task_T *taskPtr = &s_TaskList.taskPool[i];
        if (taskPtr->isUsed) {
            taskListPtr[validTaskCount] = *taskPtr;
            validTaskCount++;
        }
    }

    TASK_ENABLE_TIMER_IRQ();
    return validTaskCount;
}

/* ---------- 空闲标志 ---------- */
#if TASK_USE_IDLE_FLAG
uint8_t Task_IsIdle(void) {
    return s_TaskList.isIdleFlag;
}
#endif

/* ---------- 实时任务计时（测量代码段） ---------- */
#if TASK_USE_REAL_TASK_TIMER
void Task_ReadTimeTaskBegin(void) {
#if TASK_USE_USER_TIMER
    s_TaskList.realTimeTaskStartTick = TASK_TIMER_GET_TICK();
#else
    s_TaskList.realTimeTaskStartTick = s_TaskList.systemTick;
#endif
}

void Task_ReadTimeTaskEnd(void) {
#if TASK_USE_USER_TIMER
    s_TaskList.realTimeTaskLastTick = TASK_TIMER_CALC_DIFF_US(s_TaskList.realTimeTaskStartTick, TASK_TIMER_GET_TICK());
    s_TaskList.realTimeTotalTick += s_TaskList.realTimeTaskLastTick;
#else
    s_TaskList.realTimeTaskLastTick = s_TaskList.systemTick - s_TaskList.realTimeTaskStartTick;
    s_TaskList.realTimeTotalTick += s_TaskList.realTimeTaskLastTick;
#endif
}

TaskTick_T Task_GetReadTimeTaskRunTick(void) {
    return s_TaskList.realTimeTotalTick;
}
#endif

/* ---------- 获取系统时间 ---------- */
TaskTick_T Task_GetSystemTick(void) {
    return s_TaskList.systemTick;
}

/* ---------- 任务计时统计 ---------- */
#if TASK_USE_TASK_TIMER
TaskTick_T Task_GetTaskRunTick(void) {
    return s_TaskList.taskRunTick;
}

float Task_GetIndividualUsage(Task_T *taskPtr) {
    if (s_TaskList.taskRunTick == 0) return 0.0f;
    return (float)taskPtr->totalRunTick / s_TaskList.taskRunTick * 100.0f;
}
#endif

/* ---------- 获取状态字符串 ---------- */
char *Task_GetTaskStateString(Task_T *taskPtr) {
    switch (taskPtr->systemState) {
        case TASK_SYSTEM_STATE_WAIT:   return "等待";
        case TASK_SYSTEM_STATE_READY:  return "就绪";
#if TASK_USE_STATE_DISABLE
        case TASK_SYSTEM_STATE_DISABLE: return "禁用";
#endif
#if TASK_USE_STATE_ONCE
        case TASK_SYSTEM_STATE_ONCE:   return "单次";
#endif
        case TASK_SYSTEM_STATE_RUN:    return "运行";
        default: return "未知";
    }
}

/* ---------- 高级延时功能 ---------- */
#if TASK_USE_ADVANCE_FUNCTION

/**
 * @brief 协程式延时（带打断）
 * @param delayMs 延时时间（ms）
 * @param callback 心跳回调（返回非0则打断）
 * @return 回调返回值
 */
uint8_t Task_DelayInterrupt(uint32_t delayMs, TimerDelayCallback callback)
{
    uint32_t startTick = s_TaskList.systemTick;
    uint8_t callbackRet = 0;
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    delayMs *= 1000; /* 转换为us */
#endif

    while ((s_TaskList.systemTick - startTick) < delayMs) {
        if (IS_VALID_FUNCTION_POINTER(callback)) {
            callbackRet = callback();
            if (callbackRet != 0) break;
        }
        Task_Loop(); /* 允许其他任务执行 */
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }
    return callbackRet;
}

/**
 * @brief 协程式延时（无打断）
 * @param delayMs 延时时间（ms）
 * @param callback 心跳回调
 */
void Task_DelayUntil(uint32_t delayMs, TimerDelayCallback callback)
{
    uint32_t startTick = s_TaskList.systemTick;
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    delayMs *= 1000;
#endif

    while ((s_TaskList.systemTick - startTick) < delayMs) {
        if (IS_VALID_FUNCTION_POINTER(callback)) {
            callback();
        }
        Task_Loop();
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }
}
#endif /* TASK_USE_ADVANCE_FUNCTION */

/* ---------- 调度器主循环（无限） ---------- */
void Task_Schedule(void)
{
    while (1) {
        Task_Loop();
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }
}

/**
 * @brief 安全地向信息缓冲区追加格式化字符串
 * @param info       输出缓冲区
 * @param len        缓冲区总长度
 * @param currentLen 当前已写入长度
 * @param format     格式化字符串
 * @param ...        格式化参数
 * @return 新的已写入长度；缓冲区已满时返回 len
 * @note 该函数保证不会越界写入，并始终保证缓冲区以 '\0' 结尾
 */
static uint32_t Task_AppendInfo(char *info, uint32_t len, uint32_t currentLen, const char *format, ...)
{
    va_list args;
    int written;
    uint32_t remaining;

    if (info == NULL || len == 0) return currentLen;
    if (currentLen >= len) {
        info[len - 1] = '\0';
        return len;
    }

    remaining = len - currentLen;
    va_start(args, format);
    written = vsnprintf(info + currentLen, remaining, format, args);
    va_end(args);

    if (written < 0) {
        info[currentLen] = '\0';
        return currentLen;
    }

    if ((uint32_t)written >= remaining) {
        info[len - 1] = '\0';
        return len;
    }

    return currentLen + (uint32_t)written;
}

/* ---------- 获取系统信息 ---------- */
void Task_GetInfo(char *info, uint32_t len)
{
    if (info == NULL || len == 0) return;

    Task_T taskList[TASK_MAX_COUNT];
    uint16_t taskCount = 0;
#if TASK_USE_TASK_TIMER
    TaskTick_T totalTaskRunTime = Task_GetTaskRunTick();
    float totalUsage = 0.0f;
#endif
#if TASK_USE_REAL_TASK_TIMER
    TaskTick_T realTimeTaskRunTime = Task_GetReadTimeTaskRunTick();
    float realTimeTaskUsage = 0.0f;
#endif
    TaskTick_T systemUpTime = Task_GetSystemTick();
    uint32_t currentLen = 0;
    float systemUpTimeSec = 0.0f;

#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    systemUpTimeSec = (float)systemUpTime / 1000000.0f; /* us -> s */
#else
    systemUpTimeSec = (float)systemUpTime / 1000.0f;    /* ms -> s */
#endif

#if TASK_USE_TASK_TIMER
    if (systemUpTime > 0) {
        totalUsage = (float)totalTaskRunTime * 100.0f / systemUpTime;
        realTimeTaskUsage = (float)realTimeTaskRunTime * 100.0f / systemUpTime;
    }
#endif

    taskCount = Task_GetList(taskList, TASK_MAX_COUNT);

    /* 输出标题 */
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "\r\n--- 活动任务列表 (总计: %d/%d) ---\r\n",
                           taskCount, TASK_MAX_COUNT);
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "系统总运行时间: %.2f s\r\n", systemUpTimeSec);

#if TASK_USE_TASK_TIMER
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "调度任务历史累计CPU占用率: %.2f%%\r\n", totalUsage);
#if TASK_USE_REAL_TASK_TIMER
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "实时任务历史累计CPU占用率: %.2f%%\r\n", realTimeTaskUsage);
#endif
    /* 表头 */
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "------------------------------------------------------------\r\n");
#if TASK_USE_USER_TIMER
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "序号 | 任务名称       | 状态   | 周期 | 个体占用 | 上次耗时  \r\n");
#else
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "序号 | 任务名称       | 状态   | 周期 | 个体占用 | 上次耗时(ms)\r\n");
#endif
    currentLen = Task_AppendInfo(info, len, currentLen,
                           "-----+----------------+--------+------+----------+----------\r\n");

#if TASK_USE_REAL_TASK_TIMER
    /* 实时任务计时占位行 */
#if TASK_USE_USER_TIMER
    currentLen = Task_AppendInfo(info, len, currentLen,
                           " N/A | %-14s | %-6s | %-4d |     N/A%% | %6.3f ms\r\n",
                           "实时任务", "N/A", TASK_READ_TASK_PEROID,
                           (float)s_TaskList.realTimeTaskLastTick / 1000.0f);
#else
    currentLen = Task_AppendInfo(info, len, currentLen,
                           " N/A | %-14s | %-6s | %-4d |     N/A%% | %6.3f ms\r\n",
                           "实时任务", "N/A", TASK_READ_TASK_PEROID,
                           (float)s_TaskList.realTimeTaskLastTick);
#endif
#endif

    /* 逐个任务输出 */
    for (uint16_t i = 0; i < taskCount; i++) {
        if (currentLen >= len) break;
        Task_T *task = &taskList[i];
#if TASK_USE_TASK_NAME
        const char *taskName = task->taskName ? task->taskName : "NULL";
#else
        const char *taskName = "NULL";
#endif
        const char *stateStr = Task_GetTaskStateString(task);
        float individualUsage = Task_GetIndividualUsage(task);

#if TASK_USE_STATE_DISABLE
        if (task->systemState == TASK_SYSTEM_STATE_DISABLE || task->realPeriod == TASK_RETURN_EXIT) {
#if TASK_USE_USER_TIMER
            if (task->lastRunTick > 1200) {
                currentLen = Task_AppendInfo(info, len, currentLen,
                                       "%4d | %-14s | %-6s | ---  | %7.2f%% | %6.3f ms\r\n",
                                       i, taskName, stateStr, individualUsage,
                                       (float)task->lastRunTick / 1000.0f);
            } else {
                currentLen = Task_AppendInfo(info, len, currentLen,
                                       "%4d | %-14s | %-6s | ---  | %7.2f%% | %6llu us\r\n",
                                       i, taskName, stateStr, individualUsage,
                                       task->lastRunTick);
            }
#else
            currentLen = Task_AppendInfo(info, len, currentLen,
                                   "%4d | %-14s | %-6s | ---  | %7.2f%% | %8d ms\r\n",
                                   i, taskName, stateStr, individualUsage, task->lastRunTick);
#endif
        } else {
#endif
#if TASK_USE_USER_TIMER
            if (task->lastRunTick > 1200) {
                currentLen = Task_AppendInfo(info, len, currentLen,
                                       "%4d | %-14s | %-6s | %-4d | %7.2f%% | %6.3f ms\r\n",
                                       i, taskName, stateStr, task->realPeriod,
                                       individualUsage, (float)task->lastRunTick / 1000.0f);
            } else {
                currentLen = Task_AppendInfo(info, len, currentLen,
                                       "%4d | %-14s | %-6s | %-4d | %7.2f%% | %6llu us\r\n",
                                       i, taskName, stateStr, task->realPeriod,
                                       individualUsage, task->lastRunTick);
            }
#else
            currentLen = Task_AppendInfo(info, len, currentLen,
                                   "%4d | %-14s | %-6s | %-4d | %7.2f%% | %8d ms\r\n",
                                   i, taskName, stateStr, task->realPeriod,
                                   individualUsage, task->lastRunTick);
#endif
        }
#if TASK_USE_STATE_DISABLE
    }
#endif
#endif

    currentLen = Task_AppendInfo(info, len, currentLen, "------------------------------------------------------------\r\n");

    if (currentLen < len) info[currentLen] = '\0';
    else if (len > 0) info[len - 1] = '\0';
}

/* =================================================================================== */
/* 硬实时任务 API 实现（新增） */
/* =================================================================================== */

#if TASK_USE_HARD_REAL_TIME_TASK

/**
 * @brief 添加硬实时任务
 * @param callback 任务回调
 * @param ... 可选参数
 * @return 硬实时任务控制块指针，失败返回 NULL
 */
#if TASK_USE_TASK_NAME && TASK_USE_USER_PARAM
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, const char *taskName, void *userParam)
#elif TASK_USE_TASK_NAME
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, const char *taskName)
#elif TASK_USE_USER_PARAM
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, void *userParam)
#else
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback)
#endif
{
    HardRealTimeTask_T *newTask = NULL;
    TASK_DISABLE_TIMER_IRQ();

    for (uint8_t i = 0; i < TASK_HARD_REAL_TIME_MAX_COUNT; i++) {
        HardRealTimeTask_T *p = &s_TaskList.hardRealTimePool[i];
        if (!p->isUsed) {
            p->isUsed = 1;
            p->callback = callback;
            p->period = 1;          /* 默认1ms，需用户设置 */
            p->tickCount = 0;
#if TASK_USE_USER_STATE
            p->userState = 0;
#endif
#if TASK_USE_USER_PARAM
            p->userParam = userParam;
#endif
#if TASK_USE_TASK_NAME
            p->taskName = taskName;
#endif
            newTask = p;
            break;
        }
    }

    TASK_ENABLE_TIMER_IRQ();
    return newTask;
}

/**
 * @brief 删除硬实时任务
 * @param task 任务指针
 */
void Task_DeleteHardRealTime(HardRealTimeTask_T *task)
{
    if (task == NULL) return;
    TASK_DISABLE_TIMER_IRQ();
    task->isUsed = 0;
    TASK_ENABLE_TIMER_IRQ();
}

/**
 * @brief 设置硬实时任务周期
 * @param task 任务指针
 * @param period 周期（ms）
 */
void Task_SetHardRealTimePeriod(HardRealTimeTask_T *task, uint16_t period)
{
    if (task == NULL || !task->isUsed || period == 0) return;
    TASK_DISABLE_TIMER_IRQ();
    task->period = period;
    task->tickCount = 0;   /* 重置计数，立即生效 */
    TASK_ENABLE_TIMER_IRQ();
}

#if TASK_USE_USER_STATE
void Task_SetHardRealTimeUserState(HardRealTimeTask_T *task, uint32_t state) {
    if (task == NULL || !task->isUsed) return;
    task->userState = state;
}
uint32_t Task_GetHardRealTimeUserState(HardRealTimeTask_T *task) {
    if (task == NULL || !task->isUsed) return 0;
    return task->userState;
}
#endif

#if TASK_USE_USER_PARAM
void Task_SetHardRealTimeUserParam(HardRealTimeTask_T *task, void *userParam) {
    if (task == NULL || !task->isUsed) return;
    task->userParam = userParam;
}
void *Task_GetHardRealTimeUserParam(HardRealTimeTask_T *task) {
    if (task == NULL || !task->isUsed) return NULL;
    return task->userParam;
}
#endif

#if TASK_USE_TASK_NAME
const char *Task_GetHardRealTimeTaskName(HardRealTimeTask_T *task) {
    if (task == NULL || !task->isUsed) return NULL;
    return task->taskName;
}
#endif

#endif /* TASK_USE_HARD_REAL_TIME_TASK */

/* =================================================================================== */
/* 文件结束 */
/* =================================================================================== */
