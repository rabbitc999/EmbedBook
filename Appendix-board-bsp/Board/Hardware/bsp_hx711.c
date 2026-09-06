#include "bsp_system.h"
#include "bsp_periph.h"

#include <stdlib.h>

#define HX711_CLK_PIN   PB15   // CLK/SCK
#define HX711_DOUT_PIN  PB14   // 数据输出
#define HX711_SCALE     30700  // 修正系数，根据实际传感器调整
#define HX711_AVGERAGE_LEN  5  // 平均值滤波的滤波长度

static int32_t s_Weight;      // 实际重量值
static int32_t s_TareWeight = 91152;  // 皮重

/**
 * @brief 获取皮重
 */
uint32_t HX711_GetTare(void)  // 获取皮重
{
    uint32_t hx711_dat = 0;
    uint8_t i;
    for (i = 0; i < HX711_AVGERAGE_LEN; i++)
    {
        hx711_dat += HX711_ReadData();  // HX711AD转换数据处理
    }
    s_TareWeight = (uint16_t)(hx711_dat * 0.01f / HX711_AVGERAGE_LEN);
    return s_TareWeight;
}

/**
 * @brief qsort比较函数
 */
static int compare(const void *a, const void *b)
{
    return *(int32_t*)a - *(int32_t*)b;
}

/**
 * @brief 获取重量
 */
int32_t HX711_GetWeight(void)  // 获取被测物体重量
{
    static int32_t rawData[HX711_AVGERAGE_LEN] = { 0 }, midData[HX711_AVGERAGE_LEN] = { 0 }, midIndex = 0; 
    int32_t data = HX711_ReadData();  // HX711数据采集函数
    int32_t weight = data / 100;     // HX711AD转换数据处理，数据缩小100倍

    /* 减去称重的皮重 */
    weight = weight - s_TareWeight;     // HX711AD转换数据处理，数据缩小100倍
    weight = (uint16_t)((float)weight * 0.00001 * HX711_SCALE);  // 重量转换函数，传感器型号不同此函数要适当修改（修改HX711_SCALE的大小）
    
    /* 收集数据 */
    rawData[midIndex++] = weight;
    midIndex %= HX711_AVGERAGE_LEN;
    memcpy(midData, rawData, sizeof(midData));
    
    /* 获取中位数 */
    qsort(midData, HX711_AVGERAGE_LEN, sizeof(int32_t), compare);
    weight = midData[HX711_AVGERAGE_LEN / 2 + 1];
    
    return weight;
}

/**
 * @brief HX711初始化函数
 */
void HX711_Init(void)
{
    /* 配置CLK引脚为推挽输出 */
    BSP_GPIO_Init(HX711_CLK_PIN, OUTPUT_PP);
    
    /* 配置DOUT引脚为输入 */
    BSP_GPIO_Init(HX711_DOUT_PIN, OUTPUT_OD_U);

    /* 初始化CLK为低电平 */
    BSP_GPIO_WritePin(HX711_CLK_PIN, 0);
    
    /* 去皮操作 */
    HX711_GetTare();
}

/**
 * @brief 读取HX711的数据
 * @return 读取到的24位数据，经过符号扩展后返回
 */
int32_t HX711_ReadData(void)
{
    uint8_t i;
    uint32_t value = 0;

    /**
    数据手册写到，当数据输出管脚 DOUT 为高电平时，表明A/D 转换器还未准备好输出数据，此时串口时
    钟输入信号 PD_SCK 应为低电平，所以下面设置引脚状态。
    **/
    BSP_GPIO_WritePin(HX711_DOUT_PIN, 1);  // 初始状态DT引脚为高电平
    DWT_DelayUs(1);
    BSP_GPIO_WritePin(HX711_CLK_PIN, 0);  // 初始状态SCK引脚为低电平
    /**
    等待DT引脚变为高电平
    **/
    while (BSP_GPIO_ReadPin(HX711_DOUT_PIN) == 1)
    {
        DWT_DelayUs(1);
    }

    /**
    当 DOUT 从高电平变低电平后，PD_SCK 应输入 25 至 27 个不等的时钟脉冲
    25个时钟脉冲 ---> 通道A 增益128
    26个时钟脉冲 ---> 通道B 增益32
    27个时钟脉冲 ---> 通道A 增益64
    **/
    for (i = 0; i < 24; i++)  // 24位输出数据从最高位至最低位逐位输出完成
    {
        BSP_GPIO_WritePin(HX711_CLK_PIN, 1);
        DWT_DelayUs(1);
        BSP_GPIO_WritePin(HX711_CLK_PIN, 0);
        if (BSP_GPIO_ReadPin(HX711_DOUT_PIN) == 0)
        {
            value = value << 1;
            value |= 0x00;
        }
        if (BSP_GPIO_ReadPin(HX711_DOUT_PIN) == 1)
        {
            value = value << 1;
            value |= 0x01;
        }
        DWT_DelayUs(1);
    }

    // 第 25至 27 个时钟脉冲用来选择下一次 A/D 转换的输入通道和增益
    BSP_GPIO_WritePin(HX711_CLK_PIN, 1);
    value = value ^ 0x800000;
    DWT_DelayUs(1);
    BSP_GPIO_WritePin(HX711_CLK_PIN, 0);
    DWT_DelayUs(1);
    return value;
}