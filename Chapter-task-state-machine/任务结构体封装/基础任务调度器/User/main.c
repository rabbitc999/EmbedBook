#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Timer.h"
#include "tasks.h"
#include "Serial.h"

int main(void)
{
    LED_Init();
    Timer_Init();
	
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
