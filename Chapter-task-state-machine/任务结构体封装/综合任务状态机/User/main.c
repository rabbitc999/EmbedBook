#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Key.h"
#include "Timer.h"
#include "Serial.h"
#include "tasks.h"

Task_H *TCB_LED1, *TCB_LED2;

enum {
    LED1_STATE_ON1,
    LED1_STATE_OFF1,
    LED1_STATE_ON2,
    LED1_STATE_OFF2,
};

uint32_t LED_StateMachine(uint16_t UserState, uint16_t DeltaTick)
{
    switch(UserState)
    {
        case LED1_STATE_ON1:
        {
            LED1_ON();
            return TASK_RETURN(LED1_STATE_OFF1, 400);
        }
        case LED1_STATE_OFF1:
        {
            LED1_OFF();
            return TASK_RETURN(LED1_STATE_ON2, 200);
        }
        case LED1_STATE_ON2:
        {
            LED1_ON();
            return TASK_RETURN(LED1_STATE_OFF2, 800);
        }
        case LED1_STATE_OFF2:
        {
            LED1_OFF();
            return TASK_RETURN(LED1_STATE_ON1, 100);
        }
        default:
        {
            return LED1_STATE_ON1;
        }
    }
}

uint32_t LED2_Task(uint16_t UserState, uint16_t DeltaTick)
{
    LED2_Turn();
    return 0;
}

uint32_t Key1_Task(uint16_t UserState, uint16_t DeltaTick)
{
    uint16_t CurrentState = KEY1_Get();
    if (CurrentState == 0 && UserState != 0)
    {
        Task_Disable(TCB_LED1);
    }
    return CurrentState;
}

uint32_t Key2_Task(uint16_t UserState, uint16_t DeltaTick)
{
    uint16_t CurrentState = KEY2_Get();
    if (CurrentState == 0 && UserState != 0)
    {
        Task_Enable(TCB_LED1);
    }
    return CurrentState;
}

uint32_t Key3_Task(uint16_t UserState, uint16_t DeltaTick)
{
    uint16_t CurrentState = KEY3_Get();
    if (CurrentState == 0 && UserState != 0)
    {
        Task_Delete(TCB_LED1);
    }
    return CurrentState;
}

uint32_t Key4_Task(uint16_t UserState, uint16_t DeltaTick)
{
    uint16_t CurrentState = KEY4_Get();
    if (CurrentState == 0 && UserState != 0)
    {
        Task_RunOnce(TCB_LED2);
    }
    return CurrentState;
}

int main(void)
{
    LED_Init();
    LCD_Init();
    KEY_Init();
    Timer_Init();
    Task_Init();
    
    TCB_LED1 = Task_Add(LED_StateMachine, 100);
    TCB_LED2 = Task_Add(LED2_Task, 500);
    Task_Add(Key1_Task, 20);
    Task_Add(Key2_Task, 20);
    Task_Add(Key3_Task, 20);
    Task_Add(Key4_Task, 20);
    
    Task_Disable(TCB_LED2);
    
	while (1)
	{
        Task_Loop();
	}
}

void TIM6_IRQHandler(void) // 1ms
{
	if (TIM_GetITStatus(TIM6, TIM_IT_Update) == SET)
	{
        Task_Tick();
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
	}
}
