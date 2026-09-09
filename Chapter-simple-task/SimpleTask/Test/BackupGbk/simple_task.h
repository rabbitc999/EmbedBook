#ifndef __SIMPLE_TASK_H
#define __SIMPLE_TASK_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* =================================================================================== */
/* 系统配置 */
/* =================================================================================== */

/* 用户头文件 */
#include "bsp_system.h" /* 包含板级支持包(BSP)的系统头文件，提供底层硬件（如定时器、中断等）相关的定义和函数接口，是任务调度器与硬件交互的基础 */

/* 任务配置宏 */
#define TASK_MAX_COUNT            16                /* 系统支持的最大任务数量，即最多可创建16个任务 */
#define TASK_TICK_PERIOD          1                 /* Task_Tick函数的调用周期（单位：ms），该函数用于更新任务计时和状态，1ms表示每1毫秒触发一次任务计时更新 */
#define TASK_QUEUE_TIMEOUT        1000              /* 当启用消息队列（TASK_USE_QUENE=1）时，任务在队列中等待的超时时间（单位：ms），超过此时间未被执行会触发超时处理 */
#define TASK_MS_THRESHOLD         1000              /* 毫秒级时间差值的判断阈值（单位：ms），当时间差值超过此值时，系统会切换计时基准（如从微秒级计时切换为毫秒级计时），用于平衡计时精度和效率 */
#define TASK_DEBUG_MODE           1                 /* 是否启用调试模式。若为1，会在Task_DelayInterrupt等函数中使用IDLE状态的相关指令，便于调试；若为0，则可能优化空闲状态处理（如CPU睡眠） */
#define TASK_USE_USER_TIMER       1                 /* 任务调度器是否使用用户自定义的计时器（1=启用，0=禁用）。启用后会使用用户提供的高精度计时接口（如DWT）进行时间计算 */
#define TASK_USE_QUENE            1                 /* 是否启用消息队列功能（1=启用，0=禁用）。启用后任务通过队列进行调度，支持任务入队、出队和超时管理 */
#define TASK_USE_TASK_TIMER       1                 /* 是否启用任务计时器功能（1=启用，0=禁用）。启用后会记录任务的运行时间（如单次运行时长、总运行时长），用于任务性能统计 */
#define TASK_USE_IDLE_FLAG        1                 /* 是否启用空闲任务标志（1=启用，0=禁用）。启用后通过标志位标识系统是否处于空闲状态，可用于触发CPU睡眠等节能操作 */
#define TASK_USE_STATE_ONCE       0                 /* 是否启用任务单次运行状态（1=启用，0=禁用）。启用后任务可设置为仅运行一次，运行后自动进入禁用状态 */
#define TASK_USE_STATE_DISABLE    1                 /* 是否启用任务禁用状态（1=启用，0=禁用）。启用后支持将任务设置为禁用状态，禁用状态的任务不参与调度 */
#define TASK_USE_USER_STATE       0                 /* 是否启用任务的用户自定义状态（1=启用，0=禁用）。启用后任务可保存自定义状态变量，供任务回调函数使用 */
#define TASK_USE_USER_PARAM       0                 /* 是否启用任务的用户参数（1=启用，0=禁用）。启用后可向任务传递自定义参数（指针类型），增强任务的灵活性 */
#define TASK_USE_TASK_NAME        1                 /* 是否启用任务名称功能（1=启用，0=禁用）。启用后可给任务命名，便于调试和任务信息展示（如Task_GetInfo函数） */
#define TASK_USE_ADVANCE_FUNCTION 1                 /* 是否启用任务高级功能（1=启用，0=禁用）。启用后支持Task_DelayUntil、Task_DelayInterrupt等高级延时和中断处理函数 */
#define TASK_USE_REAL_TASK_TIMER  1                 /* 是否启用实时任务计时功能 */
#define TASK_READ_TASK_PEROID     20                /* 实时任务定时器的定时毫秒数 */

/* 系统配置 */
#define TASK_ENABLE_TIMER_IRQ()   LL_TIM_EnableIT_UPDATE(TIM6)  /* 使能TIM6定时器的更新中断，用于释放定时器中断锁。在任务调度的临界区操作（如队列操作）结束后调用，允许定时器中断继续触发 */
#define TASK_DISABLE_TIMER_IRQ()  LL_TIM_DisableIT_UPDATE(TIM6) /* 禁用TIM6定时器的更新中断，用于加上定时器中断锁。在任务调度的临界区操作（如队列操作）前调用，防止中断干扰临界资源 */
#define TASK_CPU_SLEEP()          __WFI()                       /* CPU睡眠指令（Wait For Interrupt），用于系统空闲时让CPU进入低功耗状态，等待中断（如定时器中断）唤醒后继续执行调度器 */

/* 用户高精度定时器配置宏（如DWT） */
#if TASK_USE_USER_TIMER
#define TASK_TIMER_GET_TICK()               DWT_GetTick()                 /* 获取用户定时器的计数器值（高精度计时），用于记录任务运行的起始/结束时间点，依赖DWT（数据观察点和跟踪单元）实现 */
#define TASK_TIMER_CALC_DIFF_US(start, end) DWT_GetTimeDiffUs(start, end) /* 计算两个时间点（start和end）的差值（单位：us），用于精确测量任务运行时长，基于DWT的计时差值计算实现 */
#endif

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/* 任务标签类型 */
typedef uint8_t TaskListTag_T;
typedef struct tagTask_T Task_T;

/* 任务系统状态枚举 */
typedef enum tagTaskSystemState_T
{
    TASK_SYSTEM_STATE_RUN,   /* 正在运行 */
    TASK_SYSTEM_STATE_WAIT,  /* 等待运行 */
    TASK_SYSTEM_STATE_READY, /* 准备运行 */
#if TASK_USE_STATE_DISABLE
    TASK_SYSTEM_STATE_DISABLE, /* 禁用 */
#endif
#if TASK_USE_STATE_ONCE
    TASK_SYSTEM_STATE_ONCE /* 单次运行 */
#endif
} TaskSystemState_T;

/* 任务返回值结构体 */
typedef struct tagTaskRet_T
{
    uint32_t state;      /* 下一次状态 */
    uint16_t nextPeriod; /* 下一次周期值 */
} TaskRet_T;

/* 任务函数参数结构体 */
typedef struct tagTaskParam_T
{
    Task_T *currentTask;    /* 当前任务 */
    uint16_t currentPeriod; /* 此次运行周期 */
#if TASK_USE_TASK_NAME
    const char *taskName; /* 任务名称 */
#endif
#if TASK_USE_USER_STATE
    uint32_t userState; /* 此次用户状态 */
#endif
#if TASK_USE_USER_PARAM
    void *userParam; /* 用户参数 */
#endif
} TaskParam_T;

typedef TaskRet_T (*TaskCallback)(TaskParam_T *param);
typedef uint8_t (*TimerDelayCallback)(void);

#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
typedef uint64_t TaskTick_T; /* 运行时间统计（单位：us） */
#else
typedef uint32_t TaskTick_T; /* 运行时间统计（单位：ms） */
#endif

/* 任务控制块结构体 */
typedef struct tagTask_T
{
    /* 用户定义区域 */
    TaskCallback callback; /* 任务回调函数指针 */

    /* 系统变量区域 */
    uint16_t tickCount;  /* 当前计时值（单位：ms） */
    uint16_t realPeriod; /* 实际触发周期 */
#if TASK_USE_USER_STATE
    uint32_t userState; /* 用户自定义状态（由回调函数维护） */
#endif
#if TASK_USE_TASK_TIMER
    TaskTick_T lastRunTick;  /* 上次任务运行时间（单位：us） */
    TaskTick_T totalRunTick; /* 总共任务运行时间（单位：us） */
#endif
#if TASK_USE_TASK_NAME
    const char *taskName; /* 任务名称 */
#endif
#if TASK_USE_USER_PARAM
    void *userParam; /* 用户参数 */
#endif

    /* 标志位 */
    volatile uint8_t isUsed;                /* 任务是否已分配 (1-已分配, 0-未使用) */
    volatile TaskSystemState_T systemState; /* 任务当前状态（见TaskSystemState_T） */
#if TASK_USE_QUENE
    volatile uint8_t isInMsgQuene; /* 任务是否在就绪队列 */
    uint16_t tickTimeout;          /* 队列超时计数器（单位：次数） */
#endif
} Task_T;

/* 任务错误回调函数类型（队列超时且任务已在队列时触发） */
typedef void (*TaskErrorCallback)(Task_T *taskPtr);

/* 任务列表结构体 */
typedef struct tagTaskList_T
{
    /* 任务基础 */
    Task_T taskPool[TASK_MAX_COUNT]; /* 任务池 */
    uint8_t isIdleFlag;              /* 是否为空闲状态 */
    uint16_t maxTaskId;              /* 任务最大ID编号 */
    TaskTick_T systemTick;           /* 任务调度器运行时间 */
    TaskTick_T realTimeTaskStartTick, realTimeTaskLastTick;
    TaskTick_T realTimeTotalTick;
#if TASK_USE_TASK_TIMER
    TaskTick_T taskRunTick; /* 任务总运行时间 */
    uint8_t taskNestDepth;  /* 任务调用深度 */
#endif
    /* 调度FIFO相关 */
#if TASK_USE_QUENE
    TaskErrorCallback errorCallback;      /* 队列错误回调函数 */
    Task_T *runQuene[TASK_MAX_COUNT + 1]; /* 循环队列 */
    uint8_t runQueneFirst, runQueneRear;  /* 首尾指针 */
#endif
} TaskList_T;

#if TASK_USE_STATE_DISABLE
/* 任务回调返回值 */
#define TASK_RETURN_EXIT 0xFFFF /* 返回时下次禁用任务 */
#endif

/* 系统接口函数声明 */
#if TASK_USE_TASK_NAME && TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, const char *taskName, void *userParam); /* 添加新任务 */
#elif TASK_USE_TASK_NAME
Task_T *Task_Add(TaskCallback callback, const char *taskName); /* 添加新任务 */
#elif TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, void *userParam); /* 添加新任务 */
#else
Task_T *Task_Add(TaskCallback callback); /* 添加新任务 */
#endif

void Task_Delete(Task_T *taskPtr); /* 删除任务 */

void Task_Init(void);     /* 初始化任务调度器 */
void Task_Tick(void);     /* 系统节拍处理（定时器中断调用） */
uint16_t Task_Loop(void); /* 任务调度主循环（主循环调用） */

#if TASK_USE_REAL_TASK_TIMER
void Task_ReadTimeTaskBegin(void);            /* 开始实时任务 */
void Task_ReadTimeTaskEnd(void);              /* 关闭实时任务 */
TaskTick_T Task_GetReadTimeTaskRunTick(void); /* 获取实时任务运行周期 */
#endif

#if TASK_USE_USER_STATE
void Task_SetUserState(Task_T *taskPtr, uint16_t state); /* 设置任务用户状态 */
uint16_t Task_GetUserState(Task_T *taskPtr);             /* 获取任务用户状态 */
#endif

#if TASK_USE_USER_PARAM
void Task_SetUserParam(Task_T *taskPtr, void *userParam); /* 设置任务用户参数 */
void *Task_GetUserParam(Task_T *taskPtr);                 /* 获取任务用户参数 */
#endif

#if TASK_USE_TASK_NAME
void Task_SetTaskName(Task_T *taskPtr, const char *taskName); /* 设置任务名称 */
const char *Task_GetTaskName(Task_T *taskPtr);                /* 获取任务名称 */
#endif

TaskTick_T Task_GetSystemTick(void); /* 获取系统周期 */
#if TASK_USE_TASK_TIMER
TaskTick_T Task_GetTaskRunTick(void);           /* 获取任务运行周期 */
float Task_GetIndividualUsage(Task_T *taskPtr); /* 获取任务个体使用率 */
#endif

char *Task_GetTaskStateString(Task_T *taskPtr); /* 获取当前任务运行状态标签字符串 */
void Task_RunImmediately(Task_T *taskPtr);      /* 立即执行任务 */

#if TASK_USE_STATE_DISABLE
void Task_Enable(Task_T *taskPtr);  /* 启用任务 */
void Task_Disable(Task_T *taskPtr); /* 禁用任务 */
#if TASK_USE_STATE_ONCE
void Task_RunOnce(Task_T *taskPtr); /* 单次执行任务 */
#endif
#endif

uint16_t Task_GetList(Task_T *taskListPtr, uint16_t count); /* 获取任务列表 */

TaskRet_T Task_Return(uint32_t state, uint16_t nextPeriod); /* 返回用户状态和下次调用周期 */
TaskRet_T Task_Delay(uint16_t nextPeriod);                  /* 仅返回下次调用周期 */
#if TASK_USE_STATE_DISABLE
TaskRet_T Task_Exit(void); /* 退出任务运行 */
#endif

#if TASK_USE_IDLE_FLAG
uint8_t Task_IsIdle(void); /* 判断调度是否空闲 */
#endif

#if TASK_USE_ADVANCE_FUNCTION
void Task_DelayUntil(uint32_t delayMs, TimerDelayCallback callback);        /* 任务调度延时 */
uint8_t Task_DelayInterrupt(uint32_t delayMs, TimerDelayCallback callback); /* 任务调度延时（带打断） */
#endif

void Task_Schedule(void); /* 开启任务调度 */

void Task_GetInfo(char *info, uint32_t len);

#if TASK_USE_QUENE
void Task_SetErrorCallback(TaskErrorCallback callback); /* 设置队列错误回调 */
#endif

#endif /* __FF_TASK_H */
