#include "bsp_system.h"
#include <time.h>

static MyTime_T s_Time = { 2000, 1, 1, 0, 0, 0 };  // 年、月、日、时、分、秒

#define RTC_INIT_FLAG 0x2333

/**
  * @brief  Enters the RTC Initialization mode.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval HAL status
  */
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef *hrtc)
{
  uint32_t tickstart = 0U;

  tickstart = HAL_GetTick();
  /* Wait till RTC is in INIT state and if Time out is reached exit */
  while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if ((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
      return HAL_TIMEOUT;
    }
  }

  /* Disable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);


  return HAL_OK;
}

/**
  * @brief  Exit the RTC Initialization mode.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval HAL status
  */
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef *hrtc)
{
  uint32_t tickstart = 0U;

  /* Disable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

  tickstart = HAL_GetTick();
  /* Wait till RTC is in INIT state and if Time out is reached exit */
  while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if ((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
      return HAL_TIMEOUT;
    }
  }

  return HAL_OK;
}


/**
  * @brief  Read the time counter available in RTC_CNT registers.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval Time counter
  */
static uint32_t RTC_ReadTimeCounter(RTC_HandleTypeDef *hrtc)
{
  uint16_t high1 = 0U, high2 = 0U, low = 0U;
  uint32_t timecounter = 0U;

  high1 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
  low   = READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
  high2 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);

  if (high1 != high2)
  {
    /* In this case the counter roll over during reading of CNTL and CNTH registers,
       read again CNTL register then return the counter value */
    timecounter = (((uint32_t) high2 << 16U) | READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT));
  }
  else
  {
    /* No counter roll over during reading of CNTL and CNTH registers, counter
       value is equal to first value of CNTL and CNTH */
    timecounter = (((uint32_t) high1 << 16U) | low);
  }

  return timecounter;
}

/**
  * @brief  Write the time counter in RTC_CNT registers.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @param  TimeCounter: Counter to write in RTC_CNT registers
  * @retval HAL status
  */
static HAL_StatusTypeDef RTC_WriteTimeCounter(RTC_HandleTypeDef *hrtc, uint32_t TimeCounter)
{
  HAL_StatusTypeDef status = HAL_OK;

  /* Set Initialization mode */
  if (RTC_EnterInitMode(hrtc) != HAL_OK)
  {
    status = HAL_ERROR;
  }
  else
  {
    /* Set RTC COUNTER MSB word */
    WRITE_REG(hrtc->Instance->CNTH, (TimeCounter >> 16U));
    /* Set RTC COUNTER LSB word */
    WRITE_REG(hrtc->Instance->CNTL, (TimeCounter & RTC_CNTL_RTC_CNT));

    /* Wait for synchro */
    if (RTC_ExitInitMode(hrtc) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }

  return status;
}

RTC_HandleTypeDef hrtc = { 0 };

/**
  * @brief RTC初始化
  */
void RTC_Init(void)
{
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKP_CLK_ENABLE();
    __HAL_RCC_RTC_ENABLE();
    
	hrtc.Instance = RTC;
    hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
    hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
    
	uint32_t initFlag = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
	if (initFlag == RTC_INIT_FLAG) return;

	if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
		Assert_Failed(__FILE__, __LINE__);
	}
    
    RTC_SetTime(&s_Time);
    
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_INIT_FLAG);
}

/**
  * @brief RTC设置时间
  */
void RTC_SetTime(MyTime_T *time)
{
    struct tm s_time = { 0 };    // 定义日期时间数据类型
    
    /* 1. 将数组的时间赋值给日期时间结构体 */
    s_time.tm_year = time->year - 1900;  // 年份（从1900开始）
    s_time.tm_mon = time->month - 1;      // 月份（0-11）
    s_time.tm_mday = time->day;         // 日
    s_time.tm_hour = time->hour;         // 时
    s_time.tm_min = time->minute;          // 分
    s_time.tm_sec = time->second;          // 秒
    
    /* 3. 将秒计数器写入RTC的CNT寄存器 */
    RTC_WriteTimeCounter(&hrtc, mktime(&s_time));
}

/**
  * @brief RTC设置时间戳
  * @param Timestamp 时间戳（秒数）
  */
void RTC_SetTimestamp(uint32_t Timestamp)
{
    RTC_WriteTimeCounter(&hrtc, Timestamp);
}

/**
  * @brief RTC读取时间
  * @return MyTime_T结构体指针，包含当前时间信息
  */
MyTime_T* RTC_ReadTime(void)
{
    struct tm time = { 0 };    // 定义日期时间数据类型
    
    /* 1. 读取RTC计数器 */
    /* 2. 转换为本地时间 */
    uint32_t unixTime = RTC_ReadTimeCounter(&hrtc);
    time = *localtime(&unixTime);
    
    /* 3. 将日期时间结构体赋值给MyTime_T结构体 */
    s_Time.year = time.tm_year + 1900;  // 年
    s_Time.month = time.tm_mon + 1;      // 月
    s_Time.day = time.tm_mday;         // 日
    s_Time.hour = time.tm_hour;         // 时
    s_Time.minute = time.tm_min;          // 分
    s_Time.second = time.tm_sec;          // 秒
    return &s_Time;
}
