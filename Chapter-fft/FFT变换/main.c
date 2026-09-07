#include "fft_transform.h"

#include <math.h>
#include <stdio.h>

#define FFT_DEMO_LENGTH           128u
#define FFT_DEMO_SAMPLE_FREQUENCY 128.0f
#define FFT_DEMO_PI                3.14159265358979323846f

/**
 * @brief  生成由八赫兹和二十赫兹正弦波组成的测试信号。
 * @param  samples 输出采样数组。
 * @param  length 采样点数。
 * @param  sampleFrequency 采样频率，单位为赫兹。
 * @retval 无。
 * @note   二十赫兹分量的幅值设置为零点五。
 */
static void FFT_GenerateSignal(float *samples, uint16_t length, float sampleFrequency)
{
    uint16_t index;

    /* 两个频率均落在整数频点上，便于检查幅值谱峰值 */
    for (index = 0u; index < length; index++)
    {
        float time = (float)index / sampleFrequency;
        samples[index] = sinf(2.0f * FFT_DEMO_PI * 8.0f * time)
                         + 0.5f * sinf(2.0f * FFT_DEMO_PI * 20.0f * time);
    }
}

/**
 * @brief  演示加窗、FFT 计算和单边幅值谱输出。
 * @retval 0 表示示例执行结束，负数表示算法调用失败。
 * @note   单片机工程可把输入数组替换为 ADC 定时采样缓冲区。
 */
int main(void)
{
    float realPart[FFT_DEMO_LENGTH];
    float imaginaryPart[FFT_DEMO_LENGTH] = {0.0f};
    float magnitude[FFT_DEMO_LENGTH / 2u];
    uint16_t index;

    FFT_GenerateSignal(realPart, FFT_DEMO_LENGTH, FFT_DEMO_SAMPLE_FREQUENCY);

    /* 整数周期信号可直接使用矩形窗，非整数周期信号建议改用汉宁窗 */
    if ((FFT_ApplyWindow(realPart, FFT_DEMO_LENGTH, FFT_WINDOW_RECTANGULAR) != 0)
        || (FFT_Transform(realPart, imaginaryPart, FFT_DEMO_LENGTH) != 0)
        || (FFT_CalcMagnitude(realPart, imaginaryPart, magnitude, FFT_DEMO_LENGTH) != 0))
    {
        return -1;
    }

    /* 输出频率和幅值，只显示单边频谱 */
    printf("频率,幅值\n");
    for (index = 0u; index < (FFT_DEMO_LENGTH / 2u); index++)
    {
        printf("%.2f,%.4f\n",
               FFT_GetBinFrequency(index, FFT_DEMO_SAMPLE_FREQUENCY, FFT_DEMO_LENGTH),
               magnitude[index]);
    }

    return 0;
}
