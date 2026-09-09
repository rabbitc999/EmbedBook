#include <stdlib.h>
#include "stm32f10x.h"                  // Device header

#include "tasks.h"
#include "LED.h"

Task_H Tasks[MAX_TASK] = { 0 };

void Task_Init(void)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
        Tasks[i].UseFlag = TASK_USE_STATE_UNUSED;
    }
}

void Task_Loop(void)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (Tasks[i].UseFlag == TASK_USE_STATE_USED && Tasks[i].Flag == TASK_FLAG_READY 
            && Tasks[i].Callback != NULL)
		{
			Tasks[i].Callback();
			Tasks[i].Flag = TASK_FLAG_WAIT;
			Tasks[i].Tick = 0;
		}
	}
}

void Task_Tick(void)  // 定时器调用
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (Tasks[i].UseFlag == TASK_USE_STATE_USED && ++Tasks[i].Tick >= Tasks[i].Period 
            && Tasks[i].Flag == TASK_FLAG_WAIT)
		{
			Tasks[i].Flag = TASK_FLAG_READY;
			Tasks[i].Tick = 0;
		}
	}
}

void Task_Enable(Task_H *TCB)
{
    NVIC_DisableIRQ(TIM6_IRQn);
    if (TCB == NULL || TCB->UseFlag == TASK_USE_STATE_UNUSED)
    {
        NVIC_EnableIRQ(TIM6_IRQn);
        return;
    }
    TCB->Flag = TASK_FLAG_WAIT;
    NVIC_EnableIRQ(TIM6_IRQn);
}

void Task_Disable(Task_H *TCB)
{
    NVIC_DisableIRQ(TIM6_IRQn);
    if (TCB == NULL || TCB->UseFlag == TASK_USE_STATE_UNUSED) { NVIC_EnableIRQ(TIM6_IRQn); return; }
    TCB->Flag = TASK_FLAG_DISABLE;
    NVIC_EnableIRQ(TIM6_IRQn);
}

Task_H *Task_Add(TaskCallback Callback, uint32_t Period)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (Tasks[i].UseFlag == TASK_USE_STATE_UNUSED)
		{
            Tasks[i].UseFlag = TASK_USE_STATE_USED;
            Tasks[i].Callback = Callback;
            Tasks[i].Period = Period;
            Tasks[i].Tick = 0;
            Tasks[i].Flag = TASK_FLAG_WAIT;
            
            return &Tasks[i];
		}
	}
    
    return NULL;
}

// 更安全的做法：屏蔽定时器中断
void Task_Delete(Task_H *TCB)
{
    NVIC_DisableIRQ(TIM6_IRQn);
    if (TCB == NULL || TCB->UseFlag == TASK_USE_STATE_UNUSED) { NVIC_EnableIRQ(TIM6_IRQn); return; }
    TCB->UseFlag = TASK_USE_STATE_UNUSED;
    NVIC_EnableIRQ(TIM6_IRQn);
}
