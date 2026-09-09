#include "simple_task.h"

static TaskList_T s_TaskList; /* 任务池 */

/* =================================================================================== */
/* 循环队列实现 */
/* =================================================================================== */

#if TASK_USE_QUENE
/**
 * @brief 任务入FIFO调度队列
 * @param taskPtr 任务标签
 * @note 1. 任务已在队列（isInMsgQuene=1）时忽略；2. 队列满时入队失败；
 *       3. 入队成功后设置isInMsgQuene=1，重置超时计数器
 */
void Task_QueueEnqueue(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    if (taskPtr->isInMsgQuene) return; /* 已在队列，避免重复添加 */

    TASK_DISABLE_TIMER_IRQ();

    /* 检查队列是否已满（循环队列：尾指针+1 == 头指针） */
    uint8_t nextRear = (s_TaskList.runQueneRear + 1) % (TASK_MAX_COUNT + 1);
    if (nextRear == s_TaskList.runQueneFirst)
    {
        TASK_ENABLE_TIMER_IRQ();
        return;
    }

    /* 任务入队：更新尾指针，标记队列状态 */
    s_TaskList.runQuene[s_TaskList.runQueneRear] = taskPtr;
    s_TaskList.runQueneRear = nextRear;
    taskPtr->isInMsgQuene = 1;
    taskPtr->tickTimeout = 0; /* 队列超时计数器 */
    s_TaskList.isIdleFlag = 0;

    TASK_ENABLE_TIMER_IRQ();
}

/**
 * @brief 任务出FIFO调度队列
 * @return 出队的任务标签（队空返回NULL）
 * @note 出队后重置任务的队列标记isInMsgQuene=0
 */
Task_T *Task_QueueDequeue(void)
{
    if (s_TaskList.runQueneFirst == s_TaskList.runQueneRear)
    {
        return NULL; /* 队列为空 */
    }

    TASK_DISABLE_TIMER_IRQ();

    /* 取出队首任务，更新头指针 */
    Task_T *taskPtr = s_TaskList.runQuene[s_TaskList.runQueneFirst];
    s_TaskList.runQueneFirst = (s_TaskList.runQueneFirst + 1) % (TASK_MAX_COUNT + 1);
    taskPtr->isInMsgQuene = 0; /* 标记已出队 */

    TASK_ENABLE_TIMER_IRQ();
    return taskPtr;
}

/**
 * @brief 检查任务是否在FIFO队列中
 * @param taskPtr 任务标签
 * @return 1-任务在队列中，0-不在队列中
 */
uint8_t Task_QueueContains(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return 0;

    TASK_DISABLE_TIMER_IRQ();

    uint8_t index = s_TaskList.runQueneFirst;

    while (index != s_TaskList.runQueneRear)
    {
        if (s_TaskList.runQuene[index] == taskPtr)
        {
            TASK_ENABLE_TIMER_IRQ();
            return 1;
        }
        index = (index + 1) % (TASK_MAX_COUNT + 1);
    }

    TASK_ENABLE_TIMER_IRQ();
    return 0;
}
#endif

/* =================================================================================== */
/* 调度器实现 */
/* =================================================================================== */

/**
 * @brief 检查任务状态是否有效
 * @param taskPtr 任务标签
 * @return 1-无效状态, 0-有效状态
 * @note 有效状态包括：WAIT/READY/DISABLE/ONCE/RUN
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

/**
 * @brief 系统节拍处理函数
 * @note  需在1ms定时器中断中调用
 * 功能：
 * 1. 更新所有等待/运行任务的计时器
 * 2. 将计时达到周期的任务置为运行状态
 * 3. 自动修复异常状态任务
 */
void Task_Tick(void)
{
    uint16_t taskNum = 0;
    for (taskNum = 0; taskNum < s_TaskList.maxTaskId; taskNum++)
    {
        Task_T *taskPtr = &s_TaskList.taskPool[taskNum];
        if (!taskPtr->isUsed) continue;

        /* 自动修复异常状态 */
        if (Task_IsNotNormalFlag(taskPtr))
        {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
#if TASK_USE_QUENE
            taskPtr->isInMsgQuene = 0;
            taskPtr->tickTimeout = 0;
#endif
            continue;
        }

        /* 处理等待/就绪状态任务：周期到则入队 */
        if (taskPtr->systemState == TASK_SYSTEM_STATE_WAIT || taskPtr->systemState == TASK_SYSTEM_STATE_READY)
        {
            taskPtr->tickCount += TASK_TICK_PERIOD;
            if (taskPtr->tickCount >= taskPtr->realPeriod)
            {
                taskPtr->systemState = TASK_SYSTEM_STATE_READY; /* 标记为可执行 */
#if TASK_USE_QUENE
                Task_QueueEnqueue(taskPtr);
#endif
                s_TaskList.isIdleFlag = 0;
            }
        }
    }

#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    s_TaskList.systemTick += TASK_TICK_PERIOD * 1000;
#else
    s_TaskList.systemTick += TASK_TICK_PERIOD;
#endif
}

/**
 * @brief 任务调度主循环
 * @note  需在主循环中持续调用
 * 功能：
 * 1. 执行处于运行状态的任务
 * 2. 处理任务回调返回值
 * 3. 管理任务状态转换
 */
void Task_RunTask(Task_T *taskPtr)
{
    TaskRet_T temp = { 0 };
    TaskSystemState_T lastState = taskPtr->systemState;

#if TASK_USE_TASK_TIMER
    /* 记录任务开始时间 */
#if TASK_USE_USER_TIMER
    TaskTick_T startRunTimingTick = TASK_TIMER_GET_TICK();
    TaskTick_T startRunSystemTick = s_TaskList.systemTick;
#else
    TaskTick_T startRunTick = s_TaskList.systemTick;
#endif
    taskPtr->systemState = TASK_SYSTEM_STATE_RUN;
    s_TaskList.taskNestDepth += 1;
#endif

    /* 开始执行任务 */
    if (IS_VALID_FUNCTION_POINTER(taskPtr->callback))
    {
        /* 准备传递给回调函数的参数结构体 */
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

        /* 执行任务回调并获取返回值 */
        temp = taskPtr->callback(&param); /* 传递参数结构体指针 */
    }

    /* 记录任务运行时间 */
#if TASK_USE_TASK_TIMER
#if TASK_USE_USER_TIMER
    /* 若ms级差值超过TASK_MS_THRESHOLD，则使用systemTick的差值作为基准（仍为us级，保持单位一致） */
    if (s_TaskList.systemTick - startRunSystemTick > TASK_MS_THRESHOLD * 1000)
    {
        taskPtr->lastRunTick = s_TaskList.systemTick - startRunSystemTick;
    }
    else
    {
        /* 计算高精度定时器的耗时结束时间（us级） */
        TaskTick_T endRunTimingTick = TASK_TIMER_GET_TICK();
        /* 计算高精度定时器的耗时（us级） */
        TaskTick_T highResDiff = TASK_TIMER_CALC_DIFF_US(startRunTimingTick, endRunTimingTick);
        /* 否则使用高精度定时器的结果 */
        taskPtr->lastRunTick = highResDiff;
    }
#else
    /* 非高精度模式下，直接使用systemTick差值（ms级） */
    uint32_t msDiff = s_TaskList.systemTick - startRunTick;
    /* 无论是否超过1000，均以systemTick差值为基准（无其他计时源） */
    taskPtr->lastRunTick = msDiff;
#endif
    taskPtr->totalRunTick += taskPtr->lastRunTick;

    /* 如果调用深度为1，这个时候才更新任务运行时间 */
    if (s_TaskList.taskNestDepth <= 1)
    {
        s_TaskList.taskRunTick += taskPtr->lastRunTick;
    }

    s_TaskList.taskNestDepth -= 1;
#endif

    /* 解析返回值 */
#if TASK_USE_USER_STATE
    taskPtr->userState = temp.state;
#endif
    taskPtr->realPeriod = temp.nextPeriod;
    taskPtr->tickCount = 0; /* 清零计时器 */

    /* 特殊返回值处理 */
#if TASK_USE_STATE_ONCE
    if (taskPtr->realPeriod == TASK_RETURN_EXIT || lastState == TASK_SYSTEM_STATE_ONCE)
    {
        taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE; /* 禁用任务 */
    }
    else
    {
        taskPtr->systemState = TASK_SYSTEM_STATE_WAIT; /* 重置为等待状态 */
    }
#elif TASK_USE_STATE_DISABLE
    if (taskPtr->realPeriod == TASK_RETURN_EXIT)
    {
        taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE; /* 禁用任务 */
    }
    else
    {
        taskPtr->systemState = TASK_SYSTEM_STATE_WAIT; /* 重置为等待状态 */
    }
#else
    taskPtr->systemState = TASK_SYSTEM_STATE_WAIT; /* 重置为等待状态 */
#endif
}

/**
 * @brief 任务调度主循环
 * @note  需在主循环中持续调用
 * 功能：
 * 1. 执行处于运行状态的任务
 * 2. 处理任务回调返回值
 * 3. 管理任务状态转换
 */
uint16_t Task_Loop(void)
{
    uint8_t isRunning = 0;
    uint32_t minPeriod = 0xFFFF;
#if TASK_USE_QUENE
    Task_T *taskPtr = Task_QueueDequeue();

    /* 循环处理队列中所有任务 */
    while (taskPtr != NULL)
    {
        /* 修复异常状态 */
        if (Task_IsNotNormalFlag(taskPtr))
        {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT;
            taskPtr->isInMsgQuene = 0;
            taskPtr->tickTimeout = 0;
            taskPtr = Task_QueueDequeue();
            continue;
        }

        /* 执行任务 */
        if (taskPtr->systemState != TASK_SYSTEM_STATE_DISABLE)
        {
            Task_RunTask(taskPtr);
        }
        isRunning = 1;

        /* 取下一个任务 */
        taskPtr = Task_QueueDequeue();
        if (taskPtr->realPeriod < minPeriod) { minPeriod = taskPtr->realPeriod; }
    }
#else
    uint8_t taskNum = 0;
    for (taskNum = 0; taskNum < s_TaskList.maxTaskId; taskNum++)
    {
        Task_T *taskPtr = &s_TaskList.taskPool[taskNum];

        /* 跳过未分配的任务 */
        if (!taskPtr->isUsed) { continue; }

        /* 自动修复异常状态 */
        if (Task_IsNotNormalFlag(taskPtr))
        {
            taskPtr->systemState = TASK_SYSTEM_STATE_WAIT; /* 重置为等待状态 */
            continue;
        }

#if TASK_USE_STATE_ONCE
        /* 处理可执行任务（运行状态/单次执行） */
        if (taskPtr->systemState == TASK_SYSTEM_STATE_READY || taskPtr->systemState == TASK_SYSTEM_STATE_ONCE)
        {
            Task_RunTask(taskPtr);
            isRunning = 1;
        }
#else
        /* 处理可执行任务（运行状态/单次执行） */
        if (taskPtr->systemState == TASK_SYSTEM_STATE_READY)
        {
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
/* 用户操作实现 */
/* =================================================================================== */

#if TASK_USE_QUENE
/**
 * @brief 设置队列错误回调函数
 * @param callback 用户定义的错误回调（队列超时且任务已在队列时触发）
 */
void Task_SetErrorCallback(TaskErrorCallback callback)
{
    TASK_DISABLE_TIMER_IRQ();
    s_TaskList.errorCallback = callback;
    TASK_ENABLE_TIMER_IRQ();
}
#endif

/**
 * @brief 立即执行指定任务
 * @param taskPtr 任务标签
 * @note 任务将在下次Task_Loop时执行
 */
void Task_RunImmediately(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;

    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_READY;
#if TASK_USE_QUENE
    Task_QueueEnqueue(taskPtr); /* 入队替代直接设状态 */
#endif
    TASK_ENABLE_TIMER_IRQ();
}

#if TASK_USE_STATE_ONCE
/**
 * @brief 单次执行指定任务
 * @param taskPtr 任务标签
 * @note 任务执行后自动禁用
 */
void Task_RunOnce(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;

    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_ONCE;
#if TASK_USE_QUENE
    Task_QueueEnqueue(taskPtr); /* 入队替代直接设状态 */
#endif
    TASK_ENABLE_TIMER_IRQ();
}
#endif

/**
 * @brief 初始化函数
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

    /* 初始化任务池 */
    for (int i = 0; i < TASK_MAX_COUNT; i++)
    {
        s_TaskList.taskPool[i].isUsed = 0;
    }
}

/**
 * @brief 添加新任务
 * @param callback 任务回调函数指针
 * @param period 任务执行周期（单位：ms）
 * @return 任务标签，返回 MAX_TASK_COUNT 表示添加失败
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

    /* 遍历任务是否有空位 */
    for (taskNum = 0; taskNum < s_TaskList.maxTaskId + 1 && taskNum < TASK_MAX_COUNT; taskNum++)
    {
        Task_T *taskPtr = &s_TaskList.taskPool[taskNum];

        if (!taskPtr->isUsed)
        {
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
            taskPtr->realPeriod = 0;
            newTaskPtr = &s_TaskList.taskPool[taskNum];

            /* 更新最大任务ID */
            uint16_t currentId = taskNum + 1; /* 任务ID从1开始 */
            if (currentId > s_TaskList.maxTaskId)
            {
                s_TaskList.maxTaskId = currentId;
            }

            break;
        }
    }

    TASK_ENABLE_TIMER_IRQ();

    return newTaskPtr;
}

/**
 * @brief 删除指定任务
 * @param taskPtr 任务标签
 */
void Task_Delete(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;

    /* 标记任务为未使用 */
    TASK_DISABLE_TIMER_IRQ();
    taskPtr->isUsed = 0;

    /* 如果删除的是最后一个任务，需要更新maxTaskId */
    uint16_t deletedId = (taskPtr - s_TaskList.taskPool) + 1;
    if (deletedId == s_TaskList.maxTaskId)
    {
        /* 从后往前找新的最大ID */
        while (s_TaskList.maxTaskId > 0 && !s_TaskList.taskPool[s_TaskList.maxTaskId - 1].isUsed)
        {
            s_TaskList.maxTaskId--;
        }
    }

    TASK_ENABLE_TIMER_IRQ();
}

#if TASK_USE_STATE_DISABLE
/**
 * @brief 启用指定任务
 * @param taskPtr 任务标签
 * @note 将任务置为等待状态，并置任务的等待周期为0
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
 * @brief 禁用指定任务
 * @param taskPtr 任务标签
 * @note 任务将不再参与调度
 */
void Task_Disable(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;

    TASK_DISABLE_TIMER_IRQ();
    taskPtr->systemState = TASK_SYSTEM_STATE_DISABLE;
    TASK_ENABLE_TIMER_IRQ();
}
#endif

#if TASK_USE_USER_STATE
/**
 * @brief 设置任务用户状态
 * @param taskPtr 任务标签
 * @param state 新的用户状态值
 * @note 用户状态由任务回调函数维护使用
 */
void Task_SetUserState(Task_T *taskPtr, uint16_t state)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->userState = state;
}

/**
 * @brief 获取任务用户状态
 * @param taskPtr 任务标签
 * @return 用户状态值
 * @note 用户状态由任务回调函数维护使用
 */
uint16_t Task_GetUserState(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return 0;
    return taskPtr->userState;
}
#endif

#if TASK_USE_USER_PARAM
/**
 * @brief 设置任务用户参数
 * @param taskPtr 任务标签
 * @param userParam 用户参数指针
 * @note 用户参数可以指向任何用户自定义数据结构
 */
void Task_SetUserParam(Task_T *taskPtr, void *userParam)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->userParam = userParam;
}

/**
 * @brief 获取任务用户参数
 * @param taskPtr 任务标签
 * @return 用户参数指针
 * @note 返回的指针需要由调用者确保其有效性
 */
void *Task_GetUserParam(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return NULL;
    return taskPtr->userParam;
}
#endif

#if TASK_USE_TASK_NAME
/**
 * @brief 设置任务名称
 * @param taskPtr 任务标签
 * @param taskName 任务名称字符串
 * @note 任务名称用于调试和日志记录，建议使用静态字符串
 */
void Task_SetTaskName(Task_T *taskPtr, const char *taskName)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return;
    taskPtr->taskName = taskName;
}

/**
 * @brief 获取任务名称
 * @param taskPtr 任务标签
 * @return 任务名称字符串指针
 * @note 返回的指针需要由调用者确保其有效性
 */
const char *Task_GetTaskName(Task_T *taskPtr)
{
    if (taskPtr == NULL || !taskPtr->isUsed) return NULL;
    return taskPtr->taskName;
}
#endif

/**
 * @brief 构造一个完整的任务返回值
 * @param state     任务下一次执行时的用户自定义状态
 * @param nextPeriod 任务下一次执行的周期（单位：ms），设为 TASK_RET_EXIT 可禁用任务
 * @return 返回一个包含指定状态和周期的 TaskRet_T 结构体
 * @note  此函数用于在任务回调函数结束时，同时设置任务的下次状态和执行周期
 */
TaskRet_T Task_Return(uint32_t state, uint16_t nextPeriod)
{
    TaskRet_T ret = { .nextPeriod = nextPeriod, .state = state };
    return ret;
}

/**
 * @brief 构造一个仅包含延迟周期的任务返回值（任务返回时调用，释放当前任务给其他任务）
 * @param nextPeriod 任务下一次执行的周期（单位：ms），设为 TASK_RET_EXIT 可禁用任务
 * @return 返回一个仅包含指定周期的 TaskRet_T 结构体，状态值默认为0
 * @note  此函数是 Task_Ret 的简化版，适用于只关心任务执行间隔，而不需要改变用户状态的场景
 */
TaskRet_T Task_Delay(uint16_t nextPeriod)
{
    TaskRet_T ret = { .nextPeriod = nextPeriod };
    return ret;
}

#if TASK_USE_STATE_DISABLE
/**
 * @brief 构造一个下次不允许运行的返回值
 * @return 返回一个仅包含指定周期的 TaskRet_T 结构体，状态值默认为0，周期默认为 TASK_RET_EXIT
 * @note  此函数是 Task_Ret 的简化版，适用于单次执行的任务
 */
TaskRet_T Task_Exit(void)
{
    TaskRet_T ret = { .nextPeriod = TASK_RETURN_EXIT };
    return ret;
}
#endif

/**
 * @brief 获取声明的任务列表
 * @param taskListPtr 存储的列表数组
 * @param count 列表数组大小
 * @return 声明的有效任务数
 */
uint16_t Task_GetList(Task_T *taskListPtr, uint16_t count)
{
    uint16_t validTaskCount = 0;

    if (taskListPtr == NULL || count == 0) { return 0; }

    TASK_DISABLE_TIMER_IRQ(); /* 保护任务池访问 */

    for (uint16_t i = 0; i < s_TaskList.maxTaskId && validTaskCount < count; i++)
    {
        Task_T *taskPtr = &s_TaskList.taskPool[i];
        if (taskPtr->isUsed)
        {
            taskListPtr[validTaskCount] = *taskPtr;
            validTaskCount++;
        }
    }

    TASK_ENABLE_TIMER_IRQ();

    return validTaskCount;
}

#if TASK_USE_IDLE_FLAG
/**
 * @brief 获取当前周期是否空闲
 * @return 返回空闲标志
 */
uint8_t Task_IsIdle(void)
{
    return s_TaskList.isIdleFlag;
}
#endif

#if TASK_USE_REAL_TASK_TIMER
/**
 * @brief 开始实时任务
 * @note 任务将在定时器短时执行
 */
void Task_ReadTimeTaskBegin(void) /* 开始实时任务 */
{
#if TASK_USE_USER_TIMER
    s_TaskList.realTimeTaskStartTick = TASK_TIMER_GET_TICK();
#else
    s_TaskList.realTimeTaskStartTick = s_TaskList.systemTick;
#endif
}

/**
 * @brief 关闭实时任务
 * @note 任务将在定时器短时执行
 */
void Task_ReadTimeTaskEnd(void) /* 关闭实时任务 */
{
#if TASK_USE_USER_TIMER
    s_TaskList.realTimeTaskLastTick = TASK_TIMER_CALC_DIFF_US(s_TaskList.realTimeTaskStartTick, TASK_TIMER_GET_TICK());
    s_TaskList.realTimeTotalTick += s_TaskList.realTimeTaskLastTick;
#else
    s_TaskList.realTimeTaskLastTick = s_TaskList.systemTick - s_TaskList.realTimeTaskStartTick;
    s_TaskList.realTimeTotalTick += s_TaskList.realTimeTaskLastTick;
#endif
}

/**
 * @brief 获取任务运行总周期（us）
 * @return 任务调度器运行时间
 */
TaskTick_T Task_GetReadTimeTaskRunTick(void)
{
    return s_TaskList.realTimeTotalTick;
}
#endif

/**
 * @brief 获取当前系统运行周期（ms）
 * @return 任务调度器运行时间
 */
TaskTick_T Task_GetSystemTick(void)
{
    return s_TaskList.systemTick;
}

#if TASK_USE_TASK_TIMER
/**
 * @brief 获取任务运行总周期（us）
 * @return 任务调度器运行时间
 */
TaskTick_T Task_GetTaskRunTick(void)
{
    return s_TaskList.taskRunTick;
}

/**
 * @brief 获取当前任务运行个体使用率（扩大100倍）
 * @param taskPtr 任务标签
 * @return 任务运行个体使用率
 */
float Task_GetIndividualUsage(Task_T *taskPtr)
{
    if (s_TaskList.taskRunTick == 0)
    {
        return 0.0f; /* 避免除零 */
    }

    return (float)taskPtr->totalRunTick / s_TaskList.taskRunTick * 100.0f;
}
#endif

/**
 * @brief 获取当前任务运行状态标签字符串
 * @param taskPtr 任务标签
 * @return 任务运行状态标签
 */
char *Task_GetTaskStateString(Task_T *taskPtr)
{
    switch (taskPtr->systemState)
    {
    case TASK_SYSTEM_STATE_WAIT: return "等待"; break;
    case TASK_SYSTEM_STATE_READY: return "就绪"; break;
#if TASK_USE_STATE_DISABLE
    case TASK_SYSTEM_STATE_DISABLE: return "禁用"; break;
#endif
#if TASK_USE_STATE_ONCE
    case TASK_SYSTEM_STATE_ONCE: return "单次"; break;
#endif
    case TASK_SYSTEM_STATE_RUN: return "运行"; break;
    default: return "未知"; break;
    }
}

#if TASK_USE_ADVANCE_FUNCTION
/**
 * @brief 协程调度延时循环（对于堵塞任务的协程调度，需要在任务内调用，带打断功能）
 * @param callback 延时心跳的回调函数，返回非0任务函数直接退出
 * @param delayMs 延时时长（单位：ms）
 * @note 阻塞等待延时完成，期间调用Task_Loop保证实时性
 */
uint8_t Task_DelayInterrupt(uint32_t delayMs, TimerDelayCallback callback)
{
    uint32_t startTick = s_TaskList.systemTick;
    uint8_t callbackRet = 0;
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    delayMs *= 1000; /* ms -> us */
#endif

    /* 等待延时完成 */
    while ((s_TaskList.systemTick - startTick) < delayMs)
    {
        /* 直接调用延时心跳回调函数 */
        if (IS_VALID_FUNCTION_POINTER(callback))
        {
            callbackRet = callback();
            if (callbackRet != 0)
            {
                break;
            }
        }

        Task_Loop(); /* 保证协程调度实时性 */
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        /* 没有任务需要立即执行，让CPU睡眠，等待中断唤醒 */
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }

    return callbackRet;
}

/**
 * @brief 协程调度延时循环（对于堵塞任务的协程调度，需要在任务内调用）
 * @param callback 延时心跳的回调函数，返回非0任务函数直接退出
 * @param delayMs 延时时长（单位：ms）
 * @note 阻塞等待延时完成，期间调用Task_Loop保证实时性
 */
void Task_DelayUntil(uint32_t delayMs, TimerDelayCallback callback)
{
    uint32_t startTick = s_TaskList.systemTick;
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    delayMs *= 1000; /* ms -> us */
#endif

    /* 等待延时完成 */
    while ((s_TaskList.systemTick - startTick) < delayMs)
    {
        /* 直接调用延时心跳回调函数 */
        if (IS_VALID_FUNCTION_POINTER(callback))
        {
            callback();
        }

        Task_Loop(); /* 保证协程调度实时性 */
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        /* 没有任务需要立即执行，让CPU睡眠，等待中断唤醒 */
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }
}
#endif

/**
 * @brief 开启系统调度
 * @note 在main函数后续调用完成系统调度器的执行
 */
void Task_Schedule(void)
{
    /* 等待延时完成 */
    while (1)
    {
        Task_Loop(); /* 保证协程调度实时性 */
#if !TASK_DEBUG_MODE && TASK_USE_IDLE_FLAG
        /* 没有任务需要立即执行，让CPU睡眠，等待中断唤醒 */
        if (Task_IsIdle()) { TASK_CPU_SLEEP(); }
#endif
    }
}

/**
 * @brief 获取任务系统信息并写入字符串缓冲区
 * @param info 存储任务信息的字符串缓冲区
 * @param len 缓冲区最大长度
 */
void Task_GetInfo(char *info, uint32_t len)
{
    if (info == NULL || len == 0)
    {
        return; /* 无效缓冲区，直接返回 */
    }

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
    uint32_t currentLen = 0;      /* 当前已写入的长度 */
    float systemUpTimeSec = 0.0f; /* 系统总运行时间（秒，带两位小数） */

    /* 计算系统总运行时间（转换为秒） */
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
    systemUpTimeSec = (float)systemUpTime / 1000000.0f; /* us -> s */
#else
    systemUpTimeSec = (float)systemUpTime / 1000.0f; /* ms -> s */
#endif

#if TASK_USE_TASK_TIMER
    /* 计算总CPU占用率（避免除零） */
    if (systemUpTime > 0)
    {
        totalUsage = (float)totalTaskRunTime * 100.0f / systemUpTime;
        realTimeTaskUsage = (float)realTimeTaskRunTime * 100.0f / systemUpTime;
    }
#endif

    /* 获取任务列表 */
    taskCount = Task_GetList(taskList, TASK_MAX_COUNT);

    /* 写入标题信息 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "\r\n--- 活动任务列表 (总计: %d/%d) ---\r\n",
                           taskCount, TASK_MAX_COUNT);

    /* 写入系统运行时间（统一显示为秒，带两位小数） */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "系统总运行时间: %.2f s\r\n", systemUpTimeSec);

#if TASK_USE_TASK_TIMER
    /* 写入累计CPU占用率 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "调度任务历史累计CPU占用率: %.2f%%\r\n", totalUsage);
#if TASK_USE_REAL_TASK_TIMER
    /* 写入累计CPU占用率 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "实时任务历史累计CPU占用率: %.2f%%\r\n", realTimeTaskUsage);
#endif
    /* 写入分隔线和表头 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "------------------------------------------------------------\r\n");
#if TASK_USE_USER_TIMER
    /* 用户定时器模式下，上次耗时动态显示单位 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "序号 | 任务名称       | 状态   | 周期 | 个体占用 | 上次耗时  \r\n");
#else
    /* 非用户定时器模式保持原单位 */
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "序号 | 任务名称       | 状态   | 周期 | 个体占用 | 上次耗时(ms)\r\n");
#endif
    currentLen += snprintf(info + currentLen, len - currentLen,
                           "-----+----------------+--------+------+----------+----------\r\n");
#if TASK_USE_REAL_TASK_TIMER
#if TASK_USE_USER_TIMER
    currentLen += snprintf(info + currentLen, len - currentLen,
                           " N/A | %-14s | %-6s | %-4d |     N/A%% | %6.3f ms\r\n",
                           "实时任务", "N/A", TASK_READ_TASK_PEROID,
                           (float)s_TaskList.realTimeTaskLastTick / 1000.0f);
#else
    currentLen += snprintf(info + currentLen, len - currentLen,
                           " N/A | %-14s | %-6s | %-4d |     N/A%% | %6.3f ms\r\n",
                           "实时任务", "N/A", TASK_READ_TASK_PEROID,
                           (float)s_TaskList.realTimeTaskLastTick);
#endif
#endif

    /* 写入每个任务的详细信息 */
    for (uint16_t i = 0; i < taskCount; i++)
    {
        if (currentLen >= len)
        {
            break; /* 缓冲区已满，停止写入 */
        }

        Task_T *task = &taskList[i];
#if TASK_USE_TASK_NAME
        const char *taskName = task->taskName ? task->taskName : "NULL";
#else
        const char *taskName = "NULL";
#endif
        const char *stateStr = Task_GetTaskStateString(task);
        float individualUsage = Task_GetIndividualUsage(task);

#if TASK_USE_STATE_DISABLE
        /* 根据任务状态和定时器类型决定显示格式 */
        if (task->systemState == TASK_SYSTEM_STATE_DISABLE || task->realPeriod == TASK_RETURN_EXIT)
        {
#if TASK_USE_USER_TIMER
            /* 用户定时器模式：上次耗时>1200us显示ms，否则显示us */
            if (task->lastRunTick > 1200)
            {
                currentLen += snprintf(info + currentLen, len - currentLen,
                                       "%4d | %-14s | %-6s | ---  | %7.2f%% | %6.3f ms\r\n",
                                       i, taskName, stateStr, individualUsage,
                                       (float)task->lastRunTick / 1000.0f);
            }
            else
            {
                currentLen += snprintf(info + currentLen, len - currentLen,
                                       "%4d | %-14s | %-6s | ---  | %7.2f%% | %6llu us\r\n",
                                       i, taskName, stateStr, individualUsage,
                                       task->lastRunTick);
            }
#else
            /* 非用户定时器模式保持原格式 */
            currentLen += snprintf(info + currentLen, len - currentLen,
                                   "%4d | %-14s | %-6s | ---  | %7.2f%% | %8d ms\r\n",
                                   i, taskName, stateStr, individualUsage, task->lastRunTick);
#endif
        }
        else
        {
#endif
#if TASK_USE_USER_TIMER
            /* 用户定时器模式：上次耗时>1200us显示ms，否则显示us */
            if (task->lastRunTick > 1200)
            {
                currentLen += snprintf(info + currentLen, len - currentLen,
                                       "%4d | %-14s | %-6s | %-4d | %7.2f%% | %6.3f ms\r\n",
                                       i, taskName, stateStr, task->realPeriod,
                                       individualUsage, (float)task->lastRunTick / 1000.0f);
            }
            else
            {
                currentLen += snprintf(info + currentLen, len - currentLen,
                                       "%4d | %-14s | %-6s | %-4d | %7.2f%% | %6llu us\r\n",
                                       i, taskName, stateStr, task->realPeriod,
                                       individualUsage, task->lastRunTick);
            }
#else
        /* 非用户定时器模式保持原格式 */
        currentLen += snprintf(info + currentLen, len - currentLen,
                               "%4d | %-14s | %-6s | %-4d | %7.2f%% | %8d ms\r\n",
                               i, taskName, stateStr, task->realPeriod,
                               individualUsage, task->lastRunTick);
#endif
        }
#if TASK_USE_STATE_DISABLE
    }
#endif

    /* 写入底部分隔线 */
    currentLen += snprintf(info + currentLen, len - currentLen, "------------------------------------------------------------\r\n");
#endif

    /* 确保字符串以终止符结尾 */
    if (currentLen < len)
    {
        info[currentLen] = '\0';
    }
    else if (len > 0)
    {
        info[len - 1] = '\0'; /* 缓冲区已满时强制添加终止符 */
    }
}
