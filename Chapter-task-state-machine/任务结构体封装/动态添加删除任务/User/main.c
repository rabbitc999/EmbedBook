#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Key.h"
#include "Timer.h"
#include "Serial.h"
#include "tasks.h"

Task_H *TCB_LED1;

int main(void)
{
    LED_Init();
    LCD_Init();
    KEY_Init();
    Timer_Init();
    Task_Init();
    
    TCB_LED1 = Task_Add(LED1_Turn, 400);
    Task_Add(LED2_Turn, 500);
    
	while (1)
	{
        Task_Loop();
        
        if (KEY1_Get())
        {
            Task_Disable(TCB_LED1);
        }
        
        if (KEY2_Get())
        {
            Task_Enable(TCB_LED1);
        }
        
        if (KEY3_Get())
        {
            Task_Delete(TCB_LED1);
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
