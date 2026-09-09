#ifndef __TASK_H
#define __TASK_H

#include <stdint.h>

#define MAX_TASK 2

typedef struct tagTask_H
{
	volatile uint32_t Counter;		// 计数器变量
	volatile uint32_t MaxCounter;    // 最大计数值
	void (*Callback)(void); // 回调函数
	volatile uint8_t Flag;			// 标志位
} Task_H;

// 一种机制：到指定时间置标志位（定时器）
// 主循环：检测标志位，如果该标志位不为0，则执行这个回调函数

void Task_Loop(void);  // 主循环
void Task_Tick(void);

#endif
