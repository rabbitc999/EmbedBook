#include "bsp_system.h"

/**
 * @brief 系统时钟配置函数
 */
void System_ClockConfig(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

    // 等待LSI就绪
    __HAL_RCC_LSI_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
}

/**
 * @brief 断言失败处理函数
 * @param file 发生断言失败的文件名，以字符串形式传入
 * @param line 发生断言失败的代码行号，无符号32位整数
 * @retval 无
 */
void Assert_Failed(char *file, uint32_t line)
{
    OLED_Clear();
    OLED_ShowString(0, 0, "Assert Failed !!!", OLED_6X8);
    OLED_Printf(0, 8, OLED_6X8, "File: %s", file + 19);
    OLED_Printf(0, 16, OLED_6X8, "Line: %d", line, 3);
    OLED_Update();

    while (1);
}

/**
 * @brief 板级初始化
 */
void Board_Init(void)
{
    /* 顶层初始化 */
    HAL_Init();           /* 初始化HAL库 */
    System_ClockConfig(); /* 配置系统时钟 */

    /* 禁用JTAG，释放PB3、PB4、PA15等引脚 */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    /* 板级组件初始化 */
    OLED_Init();
    Key_Init();
    Timer_Init();
    OLED_Clear();
    RTC_Init();
    DWT_Init();
    DHT11_Init();
    Beep_Init();
    TMC2209_Init();
    HX711_Init();
    AD_Init();
    LED_Init();
    Hot_Init();
    
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
}
