#include "tasks.h"
#include "LED.h"

Task_H Tasks[MAX_TASK] = {
    { 0, 400, LED1_Turn, TASK_FLAG_WAIT },
    { 0, 500, LED2_Turn, TASK_FLAG_WAIT },
};

void Task_Loop(void)
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (Tasks[i].Flag == TASK_FLAG_READY)
		{
			Tasks[i].Callback();
			Tasks[i].Flag = TASK_FLAG_WAIT;
			Tasks[i].Counter = 0;
		}
	}
}

void Task_Tick(void)  // 定时器调用
{
	uint16_t i = 0;

	for (i = 0; i < MAX_TASK; i++)
	{
		if (++Tasks[i].Counter >= Tasks[i].MaxCounter && Tasks[i].Flag == TASK_FLAG_WAIT)
		{
			Tasks[i].Flag = TASK_FLAG_READY;
			Tasks[i].Counter = 0;
		}
	}
}
