#include "tasks.h"
#include "LED.h"

Task_H Tasks[MAX_TASK] = {
    { 0, 400, LED1_Turn, 0 },
    { 0, 500, LED2_Turn, 0 },
};

void Task_Loop(void)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (Tasks[i].Flag != 0)
		{
			Tasks[i].Callback();
			Tasks[i].Flag = 0;
		}
	}
}

void Task_Tick(void)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (++Tasks[i].Counter >= Tasks[i].MaxCounter)
		{
			Tasks[i].Flag = 1;
			Tasks[i].Counter = 0;
		}
	}
}
