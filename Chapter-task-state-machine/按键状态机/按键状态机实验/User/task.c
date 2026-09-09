#include "task.h"

static Task_T *s_TaskList = NULL;  // 任务列表指针（由Task_Init初始化）

/**
 * @brief 检查任务状态是否有效
 * @param TaskTag 任务标签
 * @return 1-无效状态, 0-有效状态
 * @note 有效状态包括：WAIT/RUN/DISABLE/ONCE
 */
static uint8_t Task_IsNotNormalFlag(TaskListTag_T TaskTag)
{
    return s_TaskList[TaskTag].SystemState != TASK_SYSTEM_STATE_WAIT && 
           s_TaskList[TaskTag].SystemState != TASK_SYSTEM_STATE_RUN && 
           s_TaskList[TaskTag].SystemState != TASK_SYSTEM_STATE_DISABLE && 
           s_TaskList[TaskTag].SystemState != TASK_SYSTEM_STATE_ONCE;
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
    TaskListTag_T TaskTag;
    
    if (s_TaskList == NULL) { return; }  // 未初始化则直接返回
    
    for (TaskTag = 0; TaskTag < MAX_TASK_COUNT; TaskTag++)
    {
        // 处理等待/运行中的任务
        if (s_TaskList[TaskTag].SystemState == TASK_SYSTEM_STATE_WAIT || 
            s_TaskList[TaskTag].SystemState == TASK_SYSTEM_STATE_RUN)
        {
            s_TaskList[TaskTag].TickCount++;  // 更新计时值
            
            // 检查是否达到触发周期
            if (s_TaskList[TaskTag].TickCount >= s_TaskList[TaskTag].RealPeriod)
            {
                s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_RUN;  // 标记为可执行
            }
        }
        // 自动修复异常状态
        else if (Task_IsNotNormalFlag(TaskTag))
        {
            s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_WAIT;  // 重置为等待状态
        }
    }
}

/**
 * @brief 任务调度主循环
 * @note  需在主循环中持续调用
 * 功能：
 * 1. 执行处于运行状态的任务
 * 2. 处理任务回调返回值
 * 3. 管理任务状态转换
 */
void Task_Loop(void)
{
    TaskListTag_T TaskTag;
    
    if (s_TaskList == NULL) { return; }  // 未初始化则直接返回
    
    for (TaskTag = 0; TaskTag < MAX_TASK_COUNT; TaskTag++)
    {
        // 处理可执行任务（运行状态/单次执行）
        if (s_TaskList[TaskTag].SystemState == TASK_SYSTEM_STATE_RUN || 
            s_TaskList[TaskTag].SystemState == TASK_SYSTEM_STATE_ONCE)
        {
            uint32_t Temp = 0;
            
            if (s_TaskList[TaskTag].Callback != NULL)
            {
                // 执行任务回调并获取返回值
                Temp = s_TaskList[TaskTag].Callback(s_TaskList[TaskTag].UserState, s_TaskList[TaskTag].TickCount);
            }
                
            // 解析返回值：高16位为用户状态，低16位为执行周期
            s_TaskList[TaskTag].UserState = (Temp & 0xFFFF0000) >> 16;
            s_TaskList[TaskTag].RealPeriod = Temp & 0x0000FFFF;
            s_TaskList[TaskTag].TickCount = 0;  // 清零计时器
            
            // 特殊返回值处理
            if (s_TaskList[TaskTag].RealPeriod == TASK_RET_INTERVAL) { 
                s_TaskList[TaskTag].RealPeriod = s_TaskList[TaskTag].DefaultPeriod;  // 恢复默认周期
            }
            if (s_TaskList[TaskTag].RealPeriod == TASK_RET_EXIT) { 
                s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_DISABLE;  // 禁用任务
                continue;  // 跳过后续处理
            }
            
            // 状态转换处理
            if (s_TaskList[TaskTag].SystemState == TASK_SYSTEM_STATE_ONCE)
            {
                s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_DISABLE;  // 单次执行后禁用
            }
            else
            {
                s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_WAIT;  // 重置为等待状态
            }
        }
        // 自动修复异常状态
        else if (Task_IsNotNormalFlag(TaskTag))
        {
            s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_WAIT;  // 重置为等待状态
        }
    }
}

/**
 * @brief 启用指定任务
 * @param TaskTag 任务标签
 * @note 将任务置为等待状态并恢复默认周期
 */
void Task_Enable(TaskListTag_T TaskTag)
{
    if (s_TaskList == NULL) { return; }
    
    s_TaskList[TaskTag].RealPeriod = s_TaskList[TaskTag].DefaultPeriod;  // 恢复默认周期
    s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_WAIT;           // 置为等待状态
}

/**
 * @brief 禁用指定任务
 * @param TaskTag 任务标签
 * @note 任务将不再参与调度
 */
void Task_Disable(TaskListTag_T TaskTag)
{
    if (s_TaskList == NULL) { return; }
    s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_DISABLE;
}

/**
 * @brief 设置任务用户状态
 * @param TaskTag 任务标签
 * @param State 新的用户状态值
 * @note 用户状态由任务回调函数维护使用
 */
void Task_SetState(TaskListTag_T TaskTag, uint16_t State)
{
    if (s_TaskList == NULL) { return; }
    s_TaskList[TaskTag].UserState = State;
}

/**
 * @brief 获取任务用户状态
 * @param TaskTag 任务标签
 * @return 用户状态值
 * @note 用户状态由任务回调函数维护使用
 */
uint16_t Task_GetState(TaskListTag_T TaskTag)
{
    if (s_TaskList == NULL) { return 0; }
    return s_TaskList[TaskTag].UserState;
}

/**
 * @brief 立即执行指定任务
 * @param TaskTag 任务标签
 * @note 任务将在下次Task_Loop时执行
 */
void Task_RunImmediately(TaskListTag_T TaskTag)
{
    if (s_TaskList == NULL) { return; }
    s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_RUN;
}

/**
 * @brief 单次执行指定任务
 * @param TaskTag 任务标签
 * @note 任务执行后自动禁用
 */
void Task_RunOnce(TaskListTag_T TaskTag)
{
    if (s_TaskList == NULL) { return; }
    s_TaskList[TaskTag].SystemState = TASK_SYSTEM_STATE_ONCE;
}

/**
 * @brief 初始化任务调度器
 * @param TaskList 任务数组指针
 * @note 必须在调用其他任务接口前初始化
 */
void Task_Init(Task_T *TaskList)
{
    s_TaskList = TaskList;  // 绑定任务列表
}
