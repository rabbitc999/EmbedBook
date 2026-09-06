#ifndef __MYRTC_H
#define __MYRTC_H

#include <stdint.h>

typedef struct tagMyTime_T
{
	uint16_t year;   /* 年份 */
	uint8_t month;   /* 月份 */
	uint8_t day;     /* 日期 */
	uint8_t hour;    /* 小时 */
	uint8_t minute;  /* 分钟 */
	uint8_t second;  /* 秒钟 */
} MyTime_T;

void RTC_Tick(void);
void RTC_Init(void);
void RTC_SetTime(MyTime_T *time);
MyTime_T* RTC_ReadTime(void);
void RTC_SetTimestamp(uint32_t Timestamp);

#endif
