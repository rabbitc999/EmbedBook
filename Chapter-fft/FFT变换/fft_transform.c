#include "fft_transform.h"

#include <math.h>
#include <stddef.h>

/* FFT 模块常量与统一返回码 */
#define FFT_PI             3.14159265358979323846f
#define FFT_RESULT_OK       0
#define FFT_RESULT_INVALID -1

/**
 * @brief  交换两个浮点数。
 * @param  first 第一个浮点数指针。
 * @param  second 第二个浮点数指针。
 * @retval 无。
 * @note   用于位逆序重排阶段。
 */
static void FFT_Swap(float *first, float *second)
{
    float temporary = *first;

    *first = *second;
    *second = temporary;
}

/**
 * @brief  判断数据长度是否为二的整数次幂。
 * @param  length 数据长度。
 * @retval 1 表示满足要求，0 表示不满足要求。
 * @note   基二 FFT 要求数据长度至少为二。
 */
int8_t FFT_IsPowerOfTwo(uint16_t length)
{
    if (length < 2u)
    {
        return 0;
    }

    return ((length & (uint16_t)(length - 1u)) == 0u) ? 1 : 0;
}

/**
 * @brief  对实数采样序列施加指定窗函数。
 * @param  samples 实数采样数组。
 * @param  length 采样点数。
 * @param  windowType 窗函数类型。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   矩形窗保持原始采样值不变。
 */
int8_t FFT_ApplyWindow(float *samples, uint16_t length, FFTWindowType_T windowType)
{
    uint16_t index;

    if ((samples == NULL) || (length < 2u) || (windowType > FFT_WINDOW_BLACKMAN))
    {
        return FFT_RESULT_INVALID;
    }

    /* 按窗函数定义逐点计算权重，并直接作用到输入序列 */
    for (index = 0u; index < length; index++)
    {
        float phase = 2.0f * FFT_PI * (float)index / (float)(length - 1u);
        float weight;

        switch (windowType)
        {
            case FFT_WINDOW_HANN:
                weight = 0.5f - 0.5f * cosf(phase);
                break;

            case FFT_WINDOW_HAMMING:
                weight = 0.54f - 0.46f * cosf(phase);
                break;

            case FFT_WINDOW_BLACKMAN:
                weight = 0.42f - 0.5f * cosf(phase) + 0.08f * cosf(2.0f * phase);
                break;

            case FFT_WINDOW_RECTANGULAR:
            default:
                weight = 1.0f;
                break;
        }
        samples[index] *= weight;
    }

    return FFT_RESULT_OK;
}

/**
 * @brief  对复数序列执行原地基二时间抽取 FFT。
 * @param  realPart 输入输出序列的实部数组。
 * @param  imaginaryPart 输入输出序列的虚部数组。
 * @param  length 复数序列长度。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   输入长度必须为二的整数次幂，正变换结果未做归一化。
 */
int8_t FFT_Transform(float *realPart, float *imaginaryPart, uint16_t length)
{
    uint16_t index;
    uint16_t reverseIndex = 0u;
    uint16_t stageLength;

    if ((realPart == NULL) || (imaginaryPart == NULL) || (FFT_IsPowerOfTwo(length) == 0))
    {
        return FFT_RESULT_INVALID;
    }

    /* 先完成位逆序重排，使每级蝶形运算能够连续访问数组 */
    for (index = 1u; index < length; index++)
    {
        uint16_t bit = (uint16_t)(length >> 1u);

        while ((reverseIndex & bit) != 0u)
        {
            reverseIndex ^= bit;
            bit >>= 1u;
        }
        reverseIndex ^= bit;
        if (index < reverseIndex)
        {
            FFT_Swap(&realPart[index], &realPart[reverseIndex]);
            FFT_Swap(&imaginaryPart[index], &imaginaryPart[reverseIndex]);
        }
    }

    /* 每一级把两个较短的离散傅里叶变换合并为一个更长的结果 */
    for (stageLength = 2u; stageLength <= length; stageLength = (uint16_t)(stageLength << 1u))
    {
        uint16_t halfLength = (uint16_t)(stageLength >> 1u);
        float angleStep = -2.0f * FFT_PI / (float)stageLength;
        uint16_t blockStart;

        for (blockStart = 0u; blockStart < length; blockStart = (uint16_t)(blockStart + stageLength))
        {
            uint16_t butterflyIndex;

            for (butterflyIndex = 0u; butterflyIndex < halfLength; butterflyIndex++)
            {
                float angle = angleStep * (float)butterflyIndex;
                float twiddleReal = cosf(angle);
                float twiddleImaginary = sinf(angle);
                uint16_t upperIndex = (uint16_t)(blockStart + butterflyIndex);
                uint16_t lowerIndex = (uint16_t)(upperIndex + halfLength);
                float temporaryReal = twiddleReal * realPart[lowerIndex] - twiddleImaginary * imaginaryPart[lowerIndex];
                float temporaryImaginary = twiddleReal * imaginaryPart[lowerIndex] + twiddleImaginary * realPart[lowerIndex];

                realPart[lowerIndex] = realPart[upperIndex] - temporaryReal;
                imaginaryPart[lowerIndex] = imaginaryPart[upperIndex] - temporaryImaginary;
                realPart[upperIndex] += temporaryReal;
                imaginaryPart[upperIndex] += temporaryImaginary;
            }
        }

        /* 最大长度完成后立即退出，避免无符号左移发生回绕 */
        if (stageLength == length)
        {
            break;
        }
    }

    return FFT_RESULT_OK;
}

/**
 * @brief  计算实数信号前半频谱的单边幅值。
 * @param  realPart FFT 结果实部数组。
 * @param  imaginaryPart FFT 结果虚部数组。
 * @param  magnitude 单边幅值输出数组，容量至少为数据长度的一半。
 * @param  length FFT 数据长度。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   直流分量不乘二，其余频点转换为单边幅值。
 */
int8_t FFT_CalcMagnitude(const float *realPart, const float *imaginaryPart, float *magnitude, uint16_t length)
{
    uint16_t index;

    if ((realPart == NULL) || (imaginaryPart == NULL) || (magnitude == NULL) || (FFT_IsPowerOfTwo(length) == 0))
    {
        return FFT_RESULT_INVALID;
    }

    /* 实数信号的负频率与正频率共轭对称，因此只保留前半部分 */
    for (index = 0u; index < (uint16_t)(length / 2u); index++)
    {
        float scale = (index == 0u) ? (1.0f / (float)length) : (2.0f / (float)length);
        magnitude[index] = sqrtf(realPart[index] * realPart[index]
                                 + imaginaryPart[index] * imaginaryPart[index]) * scale;
    }

    return FFT_RESULT_OK;
}

/**
 * @brief  根据频点下标计算对应的实际频率。
 * @param  binIndex 频点下标。
 * @param  sampleFrequency 采样频率，单位为赫兹。
 * @param  length FFT 数据长度。
 * @retval 对应频率，参数无效时返回零。
 * @note   频率分辨率等于采样频率除以数据长度。
 */
float FFT_GetBinFrequency(uint16_t binIndex, float sampleFrequency, uint16_t length)
{
    if ((sampleFrequency <= 0.0f) || (length == 0u) || (binIndex >= length))
    {
        return 0.0f;
    }

    return (float)binIndex * sampleFrequency / (float)length;
}
