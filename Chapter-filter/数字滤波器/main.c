#include "digital_filter.h"

#include <stdio.h>

/**
 * @brief  演示四种滤波器处理同一组含噪采样数据。
 * @retval 0 表示示例执行结束，负数表示初始化失败。
 * @note   输出采用逗号分隔格式，便于复制到绘图软件观察曲线。
 */
int main(void)
{
    static const float samples[] = {10.0f, 10.4f, 9.8f, 24.0f, 10.2f, 9.9f, 10.3f, 10.1f};
    MovingAverageFilter_T movingAverage;
    FirstOrderLowPassFilter_T lowPass;
    MedianFilter_T median;
    KalmanFilter_T kalman;
    uint16_t index;

    /* 任一初始化失败都停止运行，避免使用未配置的滤波器状态 */
    if ((Filter_MovingAverageInit(&movingAverage, 5u) != 0)
        || (Filter_LowPassInit(&lowPass, 0.25f) != 0)
        || (Filter_MedianInit(&median, 5u) != 0)
        || (Filter_KalmanInit(&kalman, 0.02f, 0.5f, samples[0], 1.0f) != 0))
    {
        return -1;
    }

    /* 将原始数据和四种滤波结果输出到同一行，方便横向比较 */
    printf("序号,原始值,滑动平均,一阶低通,中值,卡尔曼\n");
    for (index = 0u; index < (uint16_t)(sizeof(samples) / sizeof(samples[0])); index++)
    {
        printf("%u,%.3f,%.3f,%.3f,%.3f,%.3f\n",
               index,
               samples[index],
               Filter_MovingAverageUpdate(&movingAverage, samples[index]),
               Filter_LowPassUpdate(&lowPass, samples[index]),
               Filter_MedianUpdate(&median, samples[index]),
               Filter_KalmanUpdate(&kalman, samples[index]));
    }

    return 0;
}
