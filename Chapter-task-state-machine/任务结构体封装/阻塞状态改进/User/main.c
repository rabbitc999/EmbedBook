#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Key.h"
#include "Timer.h"
#include "tasks.h"
#include "Serial.h"

int main(void)
{
    LED_Init();
    KEY_Init();
    Timer_Init();
	
	while (1)
	{
        Task_Loop();
        
        if (KEY1_Get() != 0)
        {
            Tasks[0].Flag = TASK_FLAG_DISABLE;
        }
        
        if (KEY2_Get() != 0)
        {
            Tasks[0].Flag = TASK_FLAG_WAIT;
        }
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
