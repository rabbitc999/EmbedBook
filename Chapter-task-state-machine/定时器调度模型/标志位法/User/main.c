#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Timer.h"
#include "Serial.h"

uint32_t LED1_Flag, LED2_Flag;

int main(void)
{
    LED_Init();
    Timer_Init();
	
	while (1)
	{
        if (LED1_Flag != 0)
        {
            LED1_Turn();
            LED1_Flag = 0;
        }
        
        if (LED2_Flag != 0)
        {
            LED2_Turn();
            LED2_Flag = 0;
        }
	}
}

void TIM6_IRQHandler(void) // 1ms
{
    static uint32_t LED1_Counter = 0, LED2_Counter = 0;
    
	if (TIM_GetITStatus(TIM6, TIM_IT_Update) == SET)
	{
        if (++LED1_Counter >= 400)
        {
            LED1_Flag = 1;
            LED1_Counter = 0;
        }
        if (++LED2_Counter >= 500)
        {
            LED2_Flag = 1;
            LED2_Counter = 0;
        }
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
	}
}
