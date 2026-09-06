/**
  **********************************************************************************************************************
  * @file    task_scheduler.c
  * @brief   协作式任务调度器实现（有栈协程）
  * @author  const_zpc (adapted)
  * @date    2026-09-05
  *
  * @details 基于 setjmp/longjmp 实现，支持独立栈与共享栈。
  **********************************************************************************************************************
  */

#include <stdbool.h>
#include <setjmp.h>
#include "task_scheduler_config.h"
#include "task_scheduler.h"

/* ========================================= 私有宏 ========================================= */
#define TASK_MAX_TOTAL  (TASK_MAX_UNIQUE_TASK + TASK_MAX_SHARED_TASK)

#if (TASK_MAX_TOTAL > 32)
#error "Total task count cannot exceed 32"
#endif

#if (TASK_MAX_TOTAL == 0)
#error "At least one task must be defined"
#endif

#define TASK_COMMON_INIT        0
#define TASK_COMMON_RUN         1

#define TASK_MAIN_INIT          0
#define TASK_MAIN_EXIT          1
#define TASK_MAIN_JUMP_SHARED   2
#define TASK_MAIN_START_TASK    3

#define TASK_STATUS_INIT        0
#define TASK_STATUS_READY       1
#define TASK_STATUS_RUNNING     2
#define TASK_STATUS_SUSPEND     3
#define TASK_STATUS_DELETED     4

#define TCB_MAGIC_WORD          0xDEADBEEF

/* ========================================= 私有类型 ========================================= */
typedef enum __TaskStackType_T {
    TASK_STACK_UNIQUE = 0,
    TASK_STACK_SHARED
} TaskStackType_T;

typedef struct __TaskControlBlock_T {
    char name[TASK_NAME_LENGTH];
    uint16_t pid;
    uint8_t state;
    uint8_t joinWaitCount;
    TaskCondition_T condition;
    size_t stackTop;                 /* 栈顶地址（高地址） */
    size_t stackSize;                /* 栈总大小（仅独立栈有效） */
    TaskStackType_T stackType;
    uint32_t nextRunTime;            /* 下次唤醒的绝对时间（ms） */
    int entryArg;
    jmp_buf env;
    TaskEntry_f pfnEntry;
    TaskCondition_T *pWaitCondition;
#if (TASK_MAX_SHARED_TASK > 0)
    uint8_t *pBackupStack;           /* 共享栈备份缓冲区 */
    size_t stackUsed;                /* 实际使用的栈大小 */
#endif
    struct __TaskControlBlock_T *pNext;
} TaskControlBlock_T;

typedef struct __TaskScheduler_T {
    TaskGetTime_f pfnGetTime;        /* 获取系统时间的回调 */
    TaskControlBlock_T *pCurrent;
    TaskControlBlock_T *pTaskList;
#if (TASK_MAX_SHARED_TASK > 0)
    uint32_t stackMask;              /* 共享栈缓冲区使用位图 */
#endif
    uint32_t tcbMask;                /* TCB 使用位图 */
    jmp_buf envMain;                 /* 主调度循环的跳转点 */
    uint16_t pidCounter;

    /* 防内联函数指针（保证栈帧完整） */
    void (*pfnRunTask)(uint32_t magic);
    size_t (*pfnGetStackTop)(void);
    size_t (*pfnCalcStackUsage)(TaskControlBlock_T *pTCB);
} TaskScheduler_T;

/* ========================================= 私有变量 ========================================= */
static TaskScheduler_T s_TaskScheduler;
static TaskControlBlock_T s_TcbPool[TASK_MAX_TOTAL];
#if (TASK_MAX_SHARED_TASK > 0)
static uint8_t s_SharedStackBuffers[TASK_MAX_SHARED_TASK][TASK_SHARED_BAK_STACK_SIZE];
#endif

/* ========================================= 私有函数声明 ========================================= */
static void Task_StrCopy(char *pOut, const char *pIn, size_t maxLen);
static void Task_MemCopy(uint8_t *pDst, const uint8_t *pSrc, size_t len);
static size_t Task_GetStackTop(void);
static size_t Task_CalcStackUsage(TaskControlBlock_T *pTCB);
static void Task_BackupSharedStack(TaskControlBlock_T *pTCB);
static void Task_RestoreSharedStack(TaskControlBlock_T *pTCB);
static TaskControlBlock_T *Task_AllocTcb(TaskScheduler_T *pObj);
static void Task_FreeTcb(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB);
static uint8_t *Task_AllocSharedStack(TaskScheduler_T *pObj);
static void Task_FreeSharedStack(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB);
static void Task_AddToList(TaskScheduler_T *pObj, TaskControlBlock_T *pNew);
static void Task_RemoveFromList(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB);
static uint8_t Task_CountActive(TaskScheduler_T *pObj);
static void Task_Destroy(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB);
static void Task_RunEntry(uint32_t magic);
static void Task_StartShared(void);
static void Task_JumpNext(TaskScheduler_T *pObj);
static bool Task_CheckStackOverlap(TaskScheduler_T *pObj, void *pStack, size_t stackSize);

/* ========================================= 私有函数实现 ========================================= */

static void Task_StrCopy(char *pOut, const char *pIn, size_t maxLen)
{
    while (*pIn != '\0' && maxLen > 1) {
        *pOut++ = *pIn++;
        maxLen--;
    }
    *pOut = '\0';
}

static void Task_MemCopy(uint8_t *pDst, const uint8_t *pSrc, size_t len)
{
    volatile uint8_t *d = (volatile uint8_t *)pDst;
    volatile const uint8_t *s = (volatile const uint8_t *)pSrc;
    while (len--) {
        *d++ = *s++;
    }
}

static size_t Task_GetStackTop(void)
{
    volatile char anchor;
    size_t top = (size_t)&anchor + sizeof(anchor);
    return (top + 15) & ~15UL;  /* 16字节对齐 */
}

#if (TASK_MAX_SHARED_TASK > 0)
static size_t Task_CalcStackUsage(TaskControlBlock_T *pTCB)
{
    volatile char anchor;
    size_t currentSp = (size_t)&anchor;
    size_t used = pTCB->stackTop - currentSp + 32;  /* 保留32字节安全区 */
    return (used + 15) & ~15UL;
}

static void Task_BackupSharedStack(TaskControlBlock_T *pTCB)
{
    pTCB->stackUsed = s_TaskScheduler.pfnCalcStackUsage(pTCB);
    /* 若备份空间不足，死循环提示用户增大 TASK_SHARED_BAK_STACK_SIZE */
    while (pTCB->stackUsed > TASK_SHARED_BAK_STACK_SIZE);
    Task_MemCopy(pTCB->pBackupStack,
                 (uint8_t *)(pTCB->stackTop - pTCB->stackUsed),
                 pTCB->stackUsed);
}

static void Task_RestoreSharedStack(TaskControlBlock_T *pTCB)
{
    /* 预留256字节安全区，若空间不足则死循环提示增大 TASK_SHARED_JMP_STACK_SIZE */
    while (s_TaskScheduler.pfnGetStackTop() - 256 < pTCB->stackTop);
    Task_MemCopy((uint8_t *)(pTCB->stackTop - pTCB->stackUsed),
                 pTCB->pBackupStack,
                 pTCB->stackUsed);
}
#endif /* TASK_MAX_SHARED_TASK */

static TaskControlBlock_T *Task_AllocTcb(TaskScheduler_T *pObj)
{
    for (int i = 0; i < TASK_MAX_TOTAL; i++) {
        if (!((pObj->tcbMask >> i) & 0x01)) {
            pObj->tcbMask |= (1u << i);
            return &s_TcbPool[i];
        }
    }
    return NULL;
}

static void Task_FreeTcb(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB)
{
    for (int i = 0; i < TASK_MAX_TOTAL; i++) {
        if (&s_TcbPool[i] == pTCB) {
            pObj->tcbMask &= ~(1u << i);
            break;
        }
    }
}

#if (TASK_MAX_SHARED_TASK > 0)
static uint8_t *Task_AllocSharedStack(TaskScheduler_T *pObj)
{
    for (int i = 0; i < TASK_MAX_SHARED_TASK; i++) {
        if (!((pObj->stackMask >> i) & 0x01)) {
            pObj->stackMask |= (1u << i);
            return s_SharedStackBuffers[i];
        }
    }
    return NULL;
}

static void Task_FreeSharedStack(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB)
{
    for (int i = 0; i < TASK_MAX_SHARED_TASK; i++) {
        if (s_SharedStackBuffers[i] == pTCB->pBackupStack) {
            pObj->stackMask &= ~(1u << i);
            break;
        }
    }
}
#endif /* TASK_MAX_SHARED_TASK */

static void Task_AddToList(TaskScheduler_T *pObj, TaskControlBlock_T *pNew)
{
    if (pObj->pTaskList == NULL) {
        pObj->pTaskList = pNew;
        pNew->pNext = pNew;
    } else {
        TaskControlBlock_T *p = pObj->pTaskList;
        while (p->pNext != NULL && p->pNext != pObj->pTaskList) {
            p = p->pNext;
        }
        pNew->pNext = p->pNext;
        p->pNext = pNew;
    }
}

static void Task_RemoveFromList(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB)
{
    TaskControlBlock_T *p = pObj->pTaskList;
    if (p->pNext == pObj->pTaskList) {
        pObj->pTaskList = NULL;
    } else {
        while (p->pNext != pObj->pTaskList) {
            if (p->pNext == pTCB) break;
            p = p->pNext;
        }
        p->pNext = pTCB->pNext;
        if (pTCB == pObj->pTaskList) {
            pObj->pTaskList = pTCB->pNext;
        }
    }
}

static uint8_t Task_CountActive(TaskScheduler_T *pObj)
{
    uint8_t count = 0;
    TaskControlBlock_T *p = pObj->pTaskList;
    if (p == NULL) return 0;
    do {
        if (p->state != TASK_STATUS_DELETED) count++;
        p = p->pNext;
    } while (p != pObj->pTaskList);
    return count;
}

static void Task_Destroy(TaskScheduler_T *pObj, TaskControlBlock_T *pTCB)
{
    if (pTCB->state == TASK_STATUS_DELETED && pTCB->joinWaitCount == 0) {
#if (TASK_MAX_SHARED_TASK > 0)
        Task_FreeSharedStack(pObj, pTCB);
#endif
        Task_RemoveFromList(pObj, pTCB);
        Task_FreeTcb(pObj, pTCB);
    }
}

static void Task_RunEntry(uint32_t magic)
{
    TASK_ENABLE_IRQ();
    s_TaskScheduler.pCurrent->state = TASK_STATUS_RUNNING;
    s_TaskScheduler.pCurrent->pfnEntry(s_TaskScheduler.pCurrent->entryArg);
    while (magic != TCB_MAGIC_WORD);  /* 栈损坏检测 */
    s_TaskScheduler.pCurrent->state = TASK_STATUS_DELETED;
    Task_ConditionNotify(&s_TaskScheduler.pCurrent->condition);
    Task_Destroy(&s_TaskScheduler, s_TaskScheduler.pCurrent);

    if (Task_CountActive(&s_TaskScheduler) > 0) {
        Task_JumpNext(&s_TaskScheduler);
    } else {
        longjmp(s_TaskScheduler.envMain, TASK_MAIN_EXIT);
    }
}

static bool Task_CheckStackOverlap(TaskScheduler_T *pObj, void *pStack, size_t stackSize)
{
    if (pObj->pTaskList == NULL) return true;
    TaskControlBlock_T *p = pObj->pTaskList;
    size_t newTop = (size_t)pStack + stackSize;
    size_t newBase = (size_t)pStack;
    do {
        size_t top = p->stackTop;
        size_t base = p->stackTop - p->stackSize;
        if (!(newTop <= base || newBase >= top)) {
            return false;
        }
        p = p->pNext;
    } while (p != pObj->pTaskList);
    return true;
}

#if (TASK_MAX_SHARED_TASK > 0)
static void Task_StartShared(void)
{
    volatile char reserve[TASK_SHARED_JMP_STACK_SIZE];
    reserve[0] = 0;  /* 防止优化 */
    s_TaskScheduler.pCurrent->stackTop = s_TaskScheduler.pfnGetStackTop();
    if (setjmp(s_TaskScheduler.pCurrent->env) == TASK_COMMON_INIT) {
        s_TaskScheduler.pfnRunTask(TCB_MAGIC_WORD);
    }
}
#endif /* TASK_MAX_SHARED_TASK */

static void Task_JumpNext(TaskScheduler_T *pObj)
{
    if (pObj->pTaskList == NULL) return;

    while (1) {
        TaskControlBlock_T *p = pObj->pTaskList;

        /* 检查所有任务是否应变为 READY（时间或条件触发） */
        do {
            if (p->state != TASK_STATUS_DELETED && p->state != TASK_STATUS_INIT) {
                if ((p->nextRunTime != 0 && pObj->pfnGetTime() > p->nextRunTime) ||
                    (p->pWaitCondition != NULL && p->pWaitCondition->flag == 1)) {
                    p->state = TASK_STATUS_READY;
                }
            }
            p = p->pNext;
        } while (p != pObj->pTaskList);

        /* 清除已触发的条件标志（由等待者消费） */
        do {
            if (p->state == TASK_STATUS_READY && p->pWaitCondition != NULL) {
                p->pWaitCondition->flag = 0;
            }
            p = p->pNext;
        } while (p != pObj->pTaskList);

        /* 寻找下一个可运行的任务 */
        p = pObj->pCurrent->pNext;
        do {
            if (p->state == TASK_STATUS_READY) {
                pObj->pCurrent = p;
                pObj->pCurrent->state = TASK_STATUS_RUNNING;
#if (TASK_MAX_SHARED_TASK > 0)
                if (pObj->pCurrent->pBackupStack != NULL) {
                    TASK_DISABLE_IRQ();
                    longjmp(s_TaskScheduler.envMain, TASK_MAIN_JUMP_SHARED);
                } else
#endif
                {
                    longjmp(pObj->pCurrent->env, TASK_COMMON_RUN);
                }
            } else if (p->state == TASK_STATUS_INIT) {
                s_TaskScheduler.pCurrent = p;
                TASK_DISABLE_IRQ();
                longjmp(s_TaskScheduler.envMain, TASK_MAIN_START_TASK);
            }
            p = p->pNext;
        } while (p != pObj->pCurrent->pNext);
    }
}

/* ========================================= 公共 API 实现 ========================================= */

/**
  * @brief  初始化任务调度器
  * @param  pfnGetTime 获取系统毫秒时间的回调函数（不可为 NULL）
  * @retval 无
  */
void Task_Init(TaskGetTime_f pfnGetTime)
{
    s_TaskScheduler.pfnGetTime = pfnGetTime;
    s_TaskScheduler.pTaskList = NULL;
    s_TaskScheduler.pCurrent = NULL;
#if (TASK_MAX_SHARED_TASK > 0)
    s_TaskScheduler.stackMask = 0;
#endif
    s_TaskScheduler.tcbMask = 0;
    s_TaskScheduler.pidCounter = 0;

    /* 防内联函数指针初始化 */
    s_TaskScheduler.pfnRunTask = Task_RunEntry;
    s_TaskScheduler.pfnGetStackTop = Task_GetStackTop;
    s_TaskScheduler.pfnCalcStackUsage = Task_CalcStackUsage;
}

/**
  * @brief  创建独立栈任务
  * @param  pfnEntry   任务入口函数
  * @param  pName      任务名称（可选，可为 NULL）
  * @param  pStack     栈内存起始地址
  * @param  stackSize  栈大小（字节）
  * @param  arg        入口函数参数
  * @retval 任务 ID（>=0），失败返回负错误码
  */
TaskId_t Task_CreateWithStack(TaskEntry_f pfnEntry, const char *pName,
                              void *pStack, size_t stackSize, int arg)
{
    if (s_TaskScheduler.pfnGetTime == NULL) return TASK_ERROR_NOT_INIT;
    if (pStack == NULL || stackSize == 0) return TASK_ERROR_STACK_IS_NULL;
    if (!Task_CheckStackOverlap(&s_TaskScheduler, pStack, stackSize)) {
        return TASK_ERROR_STACK_OVERLAPPING;
    }

    TaskControlBlock_T *pNew = Task_AllocTcb(&s_TaskScheduler);
    if (pNew == NULL) return TASK_ERROR_TCB_MEM_FAIL;

    Task_StrCopy(pNew->name, (pName != NULL) ? pName : "", TASK_NAME_LENGTH);
    pNew->pid = s_TaskScheduler.pidCounter++;
    pNew->pfnEntry = pfnEntry;
    pNew->entryArg = arg;
    pNew->stackTop = (size_t)pStack + stackSize;
    pNew->stackSize = stackSize;
    pNew->stackType = TASK_STACK_UNIQUE;
    pNew->state = TASK_STATUS_INIT;
    pNew->pWaitCondition = NULL;
#if (TASK_MAX_SHARED_TASK > 0)
    pNew->pBackupStack = NULL;
#endif
    pNew->nextRunTime = 0;
    pNew->joinWaitCount = 0;
    Task_ConditionInit(&pNew->condition);
    Task_AddToList(&s_TaskScheduler, pNew);
    return pNew->pid;
}

/**
  * @brief  创建共享栈任务（多个任务共用一个执行栈）
  * @param  pfnEntry   任务入口函数
  * @param  pName      任务名称（可选）
  * @param  arg        入口函数参数
  * @retval 任务 ID（>=0），失败返回负错误码
  * @note   禁止在入口函数内部的嵌套函数中使用 Task_Wait / Task_ConditionWait
  */
TaskId_t Task_Create(TaskEntry_f pfnEntry, const char *pName, int arg)
{
    if (s_TaskScheduler.pfnGetTime == NULL) return TASK_ERROR_NOT_INIT;

    TaskControlBlock_T *pNew = Task_AllocTcb(&s_TaskScheduler);
    if (pNew == NULL) return TASK_ERROR_TCB_MEM_FAIL;

    Task_StrCopy(pNew->name, (pName != NULL) ? pName : "", TASK_NAME_LENGTH);
    pNew->pid = s_TaskScheduler.pidCounter++;
    pNew->pfnEntry = pfnEntry;
    pNew->entryArg = arg;
    pNew->stackTop = 0;
    pNew->stackType = TASK_STACK_SHARED;
    pNew->state = TASK_STATUS_INIT;
    pNew->pWaitCondition = NULL;
#if (TASK_MAX_SHARED_TASK > 0)
    pNew->pBackupStack = Task_AllocSharedStack(&s_TaskScheduler);
    if (pNew->pBackupStack == NULL) {
        Task_FreeTcb(&s_TaskScheduler, pNew);
        return TASK_ERROR_TCB_MEM_STACK_FAIL;
    }
#endif
    pNew->nextRunTime = 0;
    pNew->joinWaitCount = 0;
    Task_ConditionInit(&pNew->condition);
    Task_AddToList(&s_TaskScheduler, pNew);
    return pNew->pid;
}

/**
  * @brief  启动调度器（主循环，不会返回）
  * @retval 0 成功，-1 失败（无任务或未初始化）
  */
int Task_Start(void)
{
    if (s_TaskScheduler.pTaskList == NULL || s_TaskScheduler.pfnGetTime == NULL) {
        return -1;
    }

    int ret = setjmp(s_TaskScheduler.envMain);
    if (ret == TASK_MAIN_INIT) {
        s_TaskScheduler.pCurrent = s_TaskScheduler.pTaskList;
        longjmp(s_TaskScheduler.envMain, TASK_MAIN_START_TASK);
    } else if (ret == TASK_MAIN_START_TASK) {
        if (s_TaskScheduler.pCurrent->stackType == TASK_STACK_UNIQUE) {
#if (TASK_MAX_UNIQUE_TASK > 0)
            TASK_SET_STACK(s_TaskScheduler.pCurrent->stackTop);
            s_TaskScheduler.pfnRunTask(TCB_MAGIC_WORD);
#endif
        } else {
#if (TASK_MAX_SHARED_TASK > 0)
            Task_StartShared();
#endif
        }
    }
#if (TASK_MAX_SHARED_TASK > 0)
    else if (ret == TASK_MAIN_JUMP_SHARED) {
        Task_RestoreSharedStack(s_TaskScheduler.pCurrent);
        longjmp(s_TaskScheduler.pCurrent->env, TASK_COMMON_RUN);
    }
#endif
    return 0;
}

/**
  * @brief  当前任务延时（毫秒）
  * @param  timeMs 延时时间（毫秒）
  * @retval 无
  */
void Task_Wait(uint32_t timeMs)
{
    s_TaskScheduler.pCurrent->nextRunTime = s_TaskScheduler.pfnGetTime() + timeMs;
    s_TaskScheduler.pCurrent->pWaitCondition = NULL;
    s_TaskScheduler.pCurrent->state = TASK_STATUS_SUSPEND;

    if (setjmp(s_TaskScheduler.pCurrent->env) != TASK_COMMON_RUN) {
#if (TASK_MAX_SHARED_TASK > 0)
        if (s_TaskScheduler.pCurrent->pBackupStack != NULL) {
            Task_BackupSharedStack(s_TaskScheduler.pCurrent);
        }
#endif
        Task_JumpNext(&s_TaskScheduler);
    }
    TASK_ENABLE_IRQ();
}

/**
  * @brief  等待指定任务退出
  * @param  taskId 任务 ID（由 Task_Create* 返回）
  * @retval 无
  */
void Task_Join(TaskId_t taskId)
{
    TaskControlBlock_T *p = s_TaskScheduler.pTaskList;
    do {
        if (p->pid == taskId && p->state != TASK_STATUS_DELETED) {
            p->joinWaitCount++;
            Task_ConditionWait(&p->condition);
            p->joinWaitCount--;
            Task_Destroy(&s_TaskScheduler, p);
            break;
        }
        p = p->pNext;
    } while (p != s_TaskScheduler.pTaskList);
}

/**
  * @brief  获取当前任务 PID
  * @retval 当前任务的 PID
  */
uint16_t Task_GetPid(void)
{
    return s_TaskScheduler.pCurrent->pid;
}

/**
  * @brief  条件变量初始化
  * @param  pCondition 条件变量指针（不可为 NULL）
  * @retval 无
  */
void Task_ConditionInit(TaskCondition_T *pCondition)
{
    if (pCondition != NULL) {
        pCondition->flag = 0;
    }
}

/**
  * @brief  等待条件变量（挂起当前任务）
  * @param  pCondition 条件变量指针（不可为 NULL）
  * @retval 无
  */
void Task_ConditionWait(TaskCondition_T *pCondition)
{
    if (pCondition == NULL) return;

    s_TaskScheduler.pCurrent->nextRunTime = 0;
    s_TaskScheduler.pCurrent->pWaitCondition = pCondition;
    s_TaskScheduler.pCurrent->state = TASK_STATUS_SUSPEND;

    if (setjmp(s_TaskScheduler.pCurrent->env) != TASK_COMMON_RUN) {
#if (TASK_MAX_SHARED_TASK > 0)
        if (s_TaskScheduler.pCurrent->pBackupStack != NULL) {
            Task_BackupSharedStack(s_TaskScheduler.pCurrent);
        }
#endif
        Task_JumpNext(&s_TaskScheduler);
    }
    TASK_ENABLE_IRQ();
}

/**
  * @brief  触发条件变量（唤醒等待该条件的任务）
  * @param  pCondition 条件变量指针（不可为 NULL）
  * @retval 无
  */
void Task_ConditionNotify(TaskCondition_T *pCondition)
{
    if (pCondition != NULL) {
        pCondition->flag = 1;
    }
}

/* 可选调试函数（需取消注释并实现 printf） */
#if 0
#include <stdio.h>
#define GET_STATE_STR(s)  ((s)==TASK_STATUS_READY?"R":((s)==TASK_STATUS_RUNNING?"X":((s)==TASK_STATUS_SUSPEND?"S":"D")))
void Task_Top(void)
{
    printf("PID  NAME             STATE STACK_TOP\n");
    for (int i=0; i<TASK_MAX_TOTAL; i++) {
        if ((s_TaskScheduler.tcbMask >> i) & 0x01) {
            TaskControlBlock_T *p = &s_TcbPool[i];
            printf("%3d  %-16s %-6s 0x%08lx\n",
                   p->pid, p->name, GET_STATE_STR(p->state), p->stackTop);
        }
    }
}
#endif