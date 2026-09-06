#include "bsp_system.h"
#include "bsp_periph.h"

#define ADC_CHANNEL_COUNT 1

typedef struct
{
    uint32_t Pin;
    uint32_t Channel;
} ADC_ChannelGPIOConfig;

static const ADC_ChannelGPIOConfig s_AdcChannelGpioConfig[ADC_CHANNEL_COUNT] = {
    { PA1, ADC_CHANNEL_1 },  // ADC_CHANNEL_1
};

static ADC_HandleTypeDef hadc1 = { 0 };
static ADC_ChannelConfTypeDef sConfig[ADC_CHANNEL_COUNT];


/**
  * @brief ADC1 初始化函数
  * @param None
  * @retval None
  */
void AD_Init(void)
{
    /* 1. 使能 ADC1 时钟 */
    __HAL_RCC_ADC1_CLK_ENABLE();
    
    /* 2. 配置 GPIO 为模拟输入 */
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
    {
        BSP_GPIO_Init(s_AdcChannelGpioConfig[i].Pin, ANALOG);
    }
    
    /* 3. 配置 ADC 时钟分频（6分频） */
    /* STM32F1 系列的 ADC 时钟分频需要在 RCC 中设置 */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;  // 6分频
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    
    /* 4. 配置 ADC1 */
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;          // 单通道模式
    hadc1.Init.ContinuousConvMode = DISABLE;             // 单次转换
    hadc1.Init.DiscontinuousConvMode = DISABLE;          // 禁用不连续转换
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;    // 软件触发
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;          // 数据右对齐
    hadc1.Init.NbrOfConversion = 1;                      // 1个转换
    hadc1.Init.NbrOfDiscConversion = 0;                  // 不连续转换通道数为0
    
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    
    /* 5. 预配置所有通道（提高切换速度） */
    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
    {
        sConfig[i].Channel = s_AdcChannelGpioConfig[i].Channel;
        sConfig[i].Rank = ADC_REGULAR_RANK_1;
        sConfig[i].SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
        if (HAL_ADC_ConfigChannel(&hadc1, &sConfig[i]) != HAL_OK)
        {
            Assert_Failed(__FILE__, __LINE__);
        }
    }
    
    /* 6. 执行校准 */
    HAL_ADCEx_Calibration_Start(&hadc1);
}

/**
  * @brief 获取 ADC 转换值
  * @param channel: 通道索引
  * @retval ADC 转换结果
  */
uint16_t AD_GetValue(uint8_t channel)
{
    /* 检查通道范围 */
    if (channel >= ADC_CHANNEL_COUNT)
    {
        return 0;
    }
    
    /* 1. 配置通道 */
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig[channel]) != HAL_OK)
    {
        return 0;
    }
    
    /* 2. 启动转换并等待完成 */
    uint16_t adc_value = 0;
    HAL_ADC_Start(&hadc1);
    
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)  // 10ms超时
    {
        adc_value = HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);
    
    return adc_value;
}