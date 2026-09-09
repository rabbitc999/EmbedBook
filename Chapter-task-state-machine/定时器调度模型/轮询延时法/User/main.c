#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LCD.h"
#include "LED.h"
#include "Serial.h"

uint32_t LED1_Counter, LED2_Counter;

int main(void)
{
    LED_Init();
	
	while (1)
	{
        if (++LED1_Counter >= 4)
        {
            LED1_Turn();
            LED1_Counter = 0;
        }
        
        if (++LED2_Counter >= 5)
        {
            LED2_Turn();
            LED2_Counter = 0;
        }
        
        Delay_ms(100);
	}
}
