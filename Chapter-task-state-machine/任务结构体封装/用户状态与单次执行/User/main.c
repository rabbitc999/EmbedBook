#include "stm32f10x.h"
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Serial.h"
#include "Timer.h"
#include "task.h"
#include "key.h"

enum { S1, S2, S3, S4 };

uint32_t LED1_Task(uint16_t State, uint16_t TickCount);
uint32_t LED2_Task(uint16_t State, uint16_t TickCount);

uint32_t LED1_Task(uint16_t State, uint16_t TickCount)
{
    switch(State)
    {
    case S1:
        {
            LED1_ON();
            return TASK_RENTUN(S2, 400); // osTaskDelay(400)
        }
    case S2:
        {
            LED1_OFF();
            return TASK_RENTUN(S3, 200); // osTaskDelay(200)
        }
    case S3:
        {
            LED1_ON();
            return TASK_RENTUN(S4, 800);
        }
    case S4:
        {
            LED1_OFF();
            return TASK_RENTUN(S1, 100);
        }
    default:
        {
            return TASK_RENTUN(S1, 100);
        }
    }
}

uint32_t LED2_Task(uint16_t State, uint16_t TickCount)
{
    LED2_Turn();
    return 0;
}

uint32_t Key_Task(uint16_t State, uint16_t TickCount)
{
    uint16_t CurrentState = KEY1_Get(), LastState = State;
    if (CurrentState == KEY_VOL_PRESSED && LastState == KEY_VOL_RELEASED)
    {
        Task_RunOnce(TASK_LED2);
    }
    return TASK_RENTUN(CurrentState, TASK_RET_INTERVAL);
}

Task_T TaskList[MAX_TASK_COUNT] = {
    { LED1_Task, 400, TASK_DEFAULT_INIT },
    { LED2_Task, 500, TASK_DEFAULT_INIT },
    { Key_Task, 20, TASK_DEFAULT_INIT }
};

int main(void)
{
    LED_Init();
    Key_Init();
    Timer_Init();
    Task_Init(TaskList);
    
    Task_RunOnce(TASK_LED2);
    
    while(1)
    {
        Task_Loop();
    }
}

void TIM6_IRQHandler(void)  // 1ms÷–∂œ
{
	if (TIM_GetITStatus(TIM6, TIM_IT_Update) == SET)
	{
        Task_Tick();
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
	}
}
