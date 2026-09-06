/**
 * @file simple_task.h
 * @brief 轻量级协作式任务调度器（支持硬实时任务扩展）
 * @version 2.0
 * @date 2026-08-20
 * 
 * 本调度器采用时间触发、协作运行模式，适用于资源受限的MCU。
 * 支持普通任务（在主循环中调度）和硬实时任务（在定时器中断中直接执行）。
 */

#ifndef __SIMPLE_TASK_H
#define __SIMPLE_TASK_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* =================================================================================== */
/* 系统配置（用户可根据需要修改） */
/* =================================================================================== */

/**
 * @defgroup TaskConfig 任务调度器配置宏
 * @{
 */

/** @brief 用户板级支持包头文件，提供底层硬件接口（定时器、中断、DWT等） */
#include "bsp_system.h"

/** @brief 系统支持的最大普通任务数量（静态分配） */
#define TASK_MAX_COUNT            16

/** @brief 系统节拍周期（单位：ms），即 Task_Tick() 的调用间隔 */
#define TASK_TICK_PERIOD          1

/** @brief 任务在队列中等待的超时时间（单位：ms），超时后触发错误回调 */
#define TASK_QUEUE_TIMEOUT        1000

/** @brief 毫秒级时间差值阈值（单位：ms），超过此值切换计时基准 */
#define TASK_MS_THRESHOLD         1000

/** @brief 调试模式开关（1=启用，0=禁用），影响空闲时CPU睡眠行为 */
#define TASK_DEBUG_MODE           1

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

/**
 * @brief 是否启用硬实时任务（在中断中执行，保证严格周期性）
 * @note 启用后需配置 TASK_HARD_REAL_TIME_MAX_COUNT
 */
#define TASK_USE_HARD_REAL_TIME_TASK    1

/** @brief 最大硬实时任务数量（推荐1~2，过多会增加中断处理时间） */
#define TASK_HARD_REAL_TIME_MAX_COUNT   2

/** @} */ /* End of TaskConfig */

/* =================================================================================== */
/* 底层硬件抽象（需根据MCU移植） */
/* =================================================================================== */

/**
 * @defgroup HardwareAbstraction 硬件抽象层
 * @{
 */

/** @brief 使能定时器中断（解锁临界区） */
#define TASK_ENABLE_TIMER_IRQ()   LL_TIM_EnableIT_UPDATE(TIM6)

/** @brief 禁用定时器中断（锁定临界区） */
#define TASK_DISABLE_TIMER_IRQ()  LL_TIM_DisableIT_UPDATE(TIM6)

/** @brief CPU睡眠指令（WFI），空闲时进入低功耗 */
#define TASK_CPU_SLEEP()          __WFI()

/** @} */ /* End of HardwareAbstraction */

/* =================================================================================== */
/* 用户高精度定时器接口（如DWT） */
/* =================================================================================== */

#if TASK_USE_USER_TIMER
/**
 * @brief 获取当前高精度定时器计数值（单位：tick）
 * @note 需由用户实现，通常使用DWT或硬件定时器
 */
#define TASK_TIMER_GET_TICK()               DWT_GetTick()

/**
 * @brief 计算两个时间点的差值（单位：微秒）
 * @param start 起始计数值
 * @param end   结束计数值
 * @return 时间差（us）
 */
#define TASK_TIMER_CALC_DIFF_US(start, end) DWT_GetTimeDiffUs(start, end)
#endif

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/* =================================================================================== */
/* 类型定义 */
/* =================================================================================== */

/** @brief 任务标签类型（保留，未使用） */
typedef uint8_t TaskListTag_T;

/**
 * @brief 任务控制块结构体（前向声明）
 */
typedef struct tagTask_T Task_T;

/**
 * @brief 任务系统状态枚举
 */
typedef enum tagTaskSystemState_T {
    TASK_SYSTEM_STATE_RUN,     /**< 正在运行（内部使用） */
    TASK_SYSTEM_STATE_WAIT,    /**< 等待下一次调度（计时中） */
    TASK_SYSTEM_STATE_READY,   /**< 就绪（可立即执行） */
#if TASK_USE_STATE_DISABLE
    TASK_SYSTEM_STATE_DISABLE, /**< 禁用（不参与调度） */
#endif
#if TASK_USE_STATE_ONCE
    TASK_SYSTEM_STATE_ONCE     /**< 单次执行（执行后自动禁用） */
#endif
} TaskSystemState_T;

/**
 * @brief 任务返回值结构体
 * @note 由任务回调函数返回，决定下一次行为
 */
typedef struct tagTaskRet_T {
    uint32_t state;      /**< 用户自定义状态（下次回调时传入） */
    uint16_t nextPeriod; /**< 下次执行周期（单位：ms），设为 TASK_RETURN_EXIT 可禁用任务 */
} TaskRet_T;

/**
 * @brief 任务回调函数参数结构体
 */
typedef struct tagTaskParam_T {
    Task_T *currentTask;    /**< 当前任务控制块指针 */
    uint16_t currentPeriod; /**< 本次执行的实际周期（ms） */
#if TASK_USE_TASK_NAME
    const char *taskName;   /**< 任务名称 */
#endif
#if TASK_USE_USER_STATE
    uint32_t userState;     /**< 用户状态（由上次返回值传入） */
#endif
#if TASK_USE_USER_PARAM
    void *userParam;        /**< 用户参数指针 */
#endif
} TaskParam_T;

/**
 * @brief 任务回调函数类型
 * @param param 任务参数（包含状态、名称、参数等）
 * @return 任务返回值（包含下次状态和周期）
 */
typedef TaskRet_T (*TaskCallback)(TaskParam_T *param);

/**
 * @brief 延时心跳回调函数类型（用于 Task_DelayInterrupt）
 * @return 0=继续延时，非0=打断延时
 */
typedef uint8_t (*TimerDelayCallback)(void);

/**
 * @brief 系统时间类型（根据配置自动选择64位或32位）
 */
#if TASK_USE_USER_TIMER && TASK_USE_TASK_TIMER
typedef uint64_t TaskTick_T; /**< 微秒级时间戳（64位） */
#else
typedef uint32_t TaskTick_T; /**< 毫秒级时间戳（32位） */
#endif

/* =================================================================================== */
/* 普通任务控制块 */
/* =================================================================================== */

/**
 * @brief 普通任务控制块结构体
 * @note 所有任务静态分配，在任务池中管理
 */
struct tagTask_T {
    /* 用户配置 */
    TaskCallback callback;          /**< 任务回调函数指针 */

    /* 调度参数 */
    volatile uint16_t tickCount;             /**< 当前累计计数值（ms） */
    volatile uint16_t realPeriod;            /**< 当前实际周期（ms），由回调返回更新 */

#if TASK_USE_USER_STATE
    uint32_t userState;             /**< 用户自定义状态 */
#endif

#if TASK_USE_TASK_TIMER
    TaskTick_T lastRunTick;         /**< 上次运行耗时（us） */
    TaskTick_T totalRunTick;        /**< 累计运行总耗时（us） */
#endif

#if TASK_USE_TASK_NAME
    const char *taskName;           /**< 任务名称（字符串常量） */
#endif

#if TASK_USE_USER_PARAM
    void *userParam;                /**< 用户参数指针 */
#endif

    /* 状态标志 */
    volatile uint8_t isUsed;                /**< 任务是否已分配（1=有效） */
    volatile TaskSystemState_T systemState; /**< 当前系统状态 */
#if TASK_USE_QUENE
    volatile uint8_t isInMsgQuene;          /**< 是否在就绪队列中 */
    volatile uint16_t tickTimeout;                   /**< 队列超时计数器 */
#endif
};

/* =================================================================================== */
/* 硬实时任务控制块（新增） */
/* =================================================================================== */

#if TASK_USE_HARD_REAL_TIME_TASK
/**
 * @brief 硬实时任务控制块结构体
 * @note 在中断上下文中执行，保证严格周期性
 */
typedef struct tagHardRealTimeTask_T {
    TaskCallback callback;          /**< 任务回调函数指针 */
    uint16_t period;                /**< 当前周期（ms） */
    volatile uint16_t tickCount;             /**< 累计计数值 */
    volatile uint8_t isUsed;        /**< 是否已分配 */

#if TASK_USE_USER_STATE
    uint32_t userState;             /**< 用户状态 */
#endif

#if TASK_USE_USER_PARAM
    void *userParam;                /**< 用户参数指针 */
#endif

#if TASK_USE_TASK_NAME
    const char *taskName;           /**< 任务名称 */
#endif
} HardRealTimeTask_T;
#endif /* TASK_USE_HARD_REAL_TIME_TASK */

/* =================================================================================== */
/* 错误回调类型 */
/* =================================================================================== */

/**
 * @brief 任务错误回调函数类型（队列超时时触发）
 * @param taskPtr 发生超时的任务控制块指针
 */
typedef void (*TaskErrorCallback)(Task_T *taskPtr);

/* =================================================================================== */
/* 调度器全局数据结构 */
/* =================================================================================== */

/**
 * @brief 任务列表结构体（包含所有任务池和调度状态）
 * @note 全局唯一实例 s_TaskList
 */
typedef struct tagTaskList_T {
    /* 普通任务池 */
    Task_T taskPool[TASK_MAX_COUNT];    /**< 静态任务池 */
    volatile uint8_t isIdleFlag;                 /**< 空闲标志（1=无任务执行） */
    volatile uint16_t maxTaskId;                 /**< 当前最大任务ID（用于遍历） */
    volatile TaskTick_T systemTick;              /**< 系统运行时间（ms或us） */

    /* 实时任务计时（用于测量代码段执行时间） */
    TaskTick_T realTimeTaskStartTick;   /**< 实时任务开始时间戳 */
    TaskTick_T realTimeTaskLastTick;    /**< 上次实时任务耗时 */
    TaskTick_T realTimeTotalTick;       /**< 实时任务累计耗时 */

#if TASK_USE_TASK_TIMER
    TaskTick_T taskRunTick;             /**< 所有任务累计运行时间 */
    uint8_t taskNestDepth;              /**< 任务嵌套深度（用于统计隔离） */
#endif

#if TASK_USE_QUENE
    TaskErrorCallback errorCallback;           /**< 队列超时错误回调 */
    Task_T *runQuene[TASK_MAX_COUNT + 1];      /**< 循环队列缓冲区 */
    volatile uint8_t runQueneFirst;
    volatile uint8_t runQueneRear;       /**< 队列头尾指针 */
#endif

#if TASK_USE_HARD_REAL_TIME_TASK
    HardRealTimeTask_T hardRealTimePool[TASK_HARD_REAL_TIME_MAX_COUNT]; /**< 硬实时任务池 */
#endif
} TaskList_T;

/* =================================================================================== */
/* 特殊常量 */
/* =================================================================================== */

#if TASK_USE_STATE_DISABLE
/**
 * @brief 任务返回值中的特殊周期值，表示禁用任务
 */
#define TASK_RETURN_EXIT 0xFFFF
#endif

/* =================================================================================== */
/* 普通任务 API 声明 */
/* =================================================================================== */

/**
 * @defgroup TaskAPI 普通任务管理接口
 * @{
 */

/**
 * @brief 添加新任务
 * @param callback 任务回调函数
 * @param ...（可选参数：任务名称、用户参数，取决于配置宏）
 * @return 任务控制块指针，失败返回 NULL
 * @note 任务添加后默认处于 READY 状态，需设置周期才会参与调度
 */
#if TASK_USE_TASK_NAME && TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, const char *taskName, void *userParam);
#elif TASK_USE_TASK_NAME
Task_T *Task_Add(TaskCallback callback, const char *taskName);
#elif TASK_USE_USER_PARAM
Task_T *Task_Add(TaskCallback callback, void *userParam);
#else
Task_T *Task_Add(TaskCallback callback);
#endif

/**
 * @brief 删除任务
 * @param taskPtr 任务控制块指针
 * @note 删除后不再参与调度，任务槽可复用
 */
void Task_Delete(Task_T *taskPtr);

/**
 * @brief 初始化调度器
 * @note 必须在所有其他 API 之前调用
 */
void Task_Init(void);

/**
 * @brief 系统节拍处理（在定时器中断中调用）
 * @note 更新任务计时，将到期任务置为就绪状态并加入队列
 */
void Task_Tick(void);

/**
 * @brief 任务调度主循环（在主循环中反复调用）
 * @return 所有就绪任务中最小的周期值（用于动态调频）
 * @note 执行所有就绪任务，并更新任务状态
 */
uint16_t Task_Loop(void);

/**
 * @brief 立即执行指定任务（强制变为就绪状态）
 * @param taskPtr 任务控制块指针
 * @note 任务将在下次 Task_Loop 中被调度
 */
void Task_RunImmediately(Task_T *taskPtr);

/**
 * @brief 启用任务（仅当支持 DISABLE 状态时）
 * @param taskPtr 任务控制块指针
 */
#if TASK_USE_STATE_DISABLE
void Task_Enable(Task_T *taskPtr);

/**
 * @brief 禁用任务（不参与调度）
 * @param taskPtr 任务控制块指针
 */
void Task_Disable(Task_T *taskPtr);

#if TASK_USE_STATE_ONCE
/**
 * @brief 单次执行任务（执行后自动禁用）
 * @param taskPtr 任务控制块指针
 */
void Task_RunOnce(Task_T *taskPtr);
#endif
#endif

/**
 * @brief 获取当前系统运行时间
 * @return 系统运行时间（单位：ms 或 us，取决于配置）
 */
TaskTick_T Task_GetSystemTick(void);

#if TASK_USE_TASK_TIMER
/**
 * @brief 获取所有任务的累计运行时间
 * @return 累计时间（单位：us）
 */
TaskTick_T Task_GetTaskRunTick(void);

/**
 * @brief 获取指定任务的个体 CPU 占用率（百分比，扩大100倍）
 * @param taskPtr 任务控制块指针
 * @return 占用率（单位：%），例如 12.34 表示 12.34%
 */
float Task_GetIndividualUsage(Task_T *taskPtr);
#endif

/**
 * @brief 获取任务状态字符串（用于调试输出）
 * @param taskPtr 任务控制块指针
 * @return 状态字符串（如 "等待"、"就绪"）
 */
char *Task_GetTaskStateString(Task_T *taskPtr);

/**
 * @brief 获取任务列表（拷贝到用户缓冲区）
 * @param taskListPtr 用户缓冲区指针
 * @param count 缓冲区大小（最多可存放的任务数）
 * @return 实际拷贝的任务数量
 */
uint16_t Task_GetList(Task_T *taskListPtr, uint16_t count);

/**
 * @brief 构造完整返回值（同时设置状态和周期）
 * @param state 用户状态
 * @param nextPeriod 下次执行周期（ms）
 * @return 任务返回值结构体
 */
TaskRet_T Task_Return(uint32_t state, uint16_t nextPeriod);

/**
 * @brief 构造仅含周期的返回值（状态置0）
 * @param nextPeriod 下次执行周期（ms）
 * @return 任务返回值结构体
 */
TaskRet_T Task_Delay(uint16_t nextPeriod);

#if TASK_USE_STATE_DISABLE
/**
 * @brief 构造退出返回值（禁用任务）
 * @return 任务返回值（nextPeriod = TASK_RETURN_EXIT）
 */
TaskRet_T Task_Exit(void);
#endif

#if TASK_USE_IDLE_FLAG
/**
 * @brief 判断系统是否空闲（无任务执行）
 * @return 1=空闲，0=忙碌
 */
uint8_t Task_IsIdle(void);
#endif

#if TASK_USE_ADVANCE_FUNCTION
/**
 * @brief 协程式延时（阻塞当前任务，但允许其他任务执行）
 * @param delayMs 延时时间（单位：ms）
 * @param callback 每次循环调用的心跳回调（可选）
 * @note 仅限在普通任务回调中使用，硬实时任务中禁止调用
 */
void Task_DelayUntil(uint32_t delayMs, TimerDelayCallback callback);

/**
 * @brief 协程式延时（带打断功能）
 * @param delayMs 延时时间（ms）
 * @param callback 心跳回调，返回非0则立即退出延时
 * @return 回调返回的值（0=正常完成，非0=被打断）
 */
uint8_t Task_DelayInterrupt(uint32_t delayMs, TimerDelayCallback callback);
#endif

/**
 * @brief 启动调度器（无限循环执行 Task_Loop）
 * @note 调用后不会返回
 */
void Task_Schedule(void);

/**
 * @brief 获取系统状态信息（格式化字符串）
 * @param info 输出缓冲区
 * @param len 缓冲区大小
 * @note 输出任务列表、CPU占用率、运行时间等
 */
void Task_GetInfo(char *info, uint32_t len);

#if TASK_USE_QUENE
/**
 * @brief 设置队列超时错误回调
 * @param callback 错误回调函数指针
 */
void Task_SetErrorCallback(TaskErrorCallback callback);
#endif

#if TASK_USE_USER_STATE
/**
 * @brief 设置任务用户状态
 */
void Task_SetUserState(Task_T *taskPtr, uint16_t state);
uint16_t Task_GetUserState(Task_T *taskPtr);
#endif

#if TASK_USE_USER_PARAM
void Task_SetUserParam(Task_T *taskPtr, void *userParam);
void *Task_GetUserParam(Task_T *taskPtr);
#endif

#if TASK_USE_TASK_NAME
void Task_SetTaskName(Task_T *taskPtr, const char *taskName);
const char *Task_GetTaskName(Task_T *taskPtr);
#endif

#if TASK_USE_REAL_TASK_TIMER
void Task_ReadTimeTaskBegin(void);
void Task_ReadTimeTaskEnd(void);
TaskTick_T Task_GetReadTimeTaskRunTick(void);
#endif

/** @} */ /* End of TaskAPI */

/* =================================================================================== */
/* 硬实时任务 API 声明（新增） */
/* =================================================================================== */

#if TASK_USE_HARD_REAL_TIME_TASK
/**
 * @defgroup HardRealTimeAPI 硬实时任务管理接口
 * @brief 在中断中执行的任务，保证严格周期性，适合PWM、ADC触发等
 * @{
 */

/**
 * @brief 添加硬实时任务
 * @param callback 任务回调函数（必须在中断中快速执行）
 * @param ... （可选参数：名称、用户参数）
 * @return 硬实时任务控制块指针，失败返回 NULL
 * @note 回调中禁止调用 Task_DelayUntil 等阻塞函数
 */
#if TASK_USE_TASK_NAME && TASK_USE_USER_PARAM
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, const char *taskName, void *userParam);
#elif TASK_USE_TASK_NAME
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, const char *taskName);
#elif TASK_USE_USER_PARAM
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback, void *userParam);
#else
HardRealTimeTask_T* Task_AddHardRealTime(TaskCallback callback);
#endif

/**
 * @brief 删除硬实时任务
 * @param task 硬实时任务控制块指针
 */
void Task_DeleteHardRealTime(HardRealTimeTask_T *task);

/**
 * @brief 设置硬实时任务周期
 * @param task 任务指针
 * @param period 周期（单位：ms，最小为1）
 * @note 立即生效（重置 tickCount）
 */
void Task_SetHardRealTimePeriod(HardRealTimeTask_T *task, uint16_t period);

#if TASK_USE_USER_STATE
void Task_SetHardRealTimeUserState(HardRealTimeTask_T *task, uint32_t state);
uint32_t Task_GetHardRealTimeUserState(HardRealTimeTask_T *task);
#endif

#if TASK_USE_USER_PARAM
void Task_SetHardRealTimeUserParam(HardRealTimeTask_T *task, void *userParam);
void *Task_GetHardRealTimeUserParam(HardRealTimeTask_T *task);
#endif

#if TASK_USE_TASK_NAME
const char *Task_GetHardRealTimeTaskName(HardRealTimeTask_T *task);
#endif

/** @} */ /* End of HardRealTimeAPI */

#endif /* TASK_USE_HARD_REAL_TIME_TASK */

#endif /* __SIMPLE_TASK_H */