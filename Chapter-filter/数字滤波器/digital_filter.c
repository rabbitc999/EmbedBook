#include "digital_filter.h"

#include <stddef.h>

/* 数字滤波模块统一返回码 */
#define FILTER_RESULT_OK       0
#define FILTER_RESULT_INVALID -1

/**
 * @brief  初始化滑动平均滤波器。
 * @param  filter 滤波器指针。
 * @param  length 滑动窗口长度。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   窗口越长，平滑能力越强，响应延迟也越明显。
 */
int8_t Filter_MovingAverageInit(MovingAverageFilter_T *filter, uint16_t length)
{
    uint16_t index;

    if ((filter == NULL) || (length == 0u) || (length > FILTER_MOVING_AVERAGE_MAX_LENGTH))
    {
        return FILTER_RESULT_INVALID;
    }

    /* 清空循环缓冲区与累计和，保证第一次更新从确定状态开始 */
    for (index = 0u; index < FILTER_MOVING_AVERAGE_MAX_LENGTH; index++)
    {
        filter->buffer[index] = 0.0f;
    }
    filter->sum = 0.0f;
    filter->length = length;
    filter->index = 0u;
    filter->count = 0u;

    return FILTER_RESULT_OK;
}

/**
 * @brief  输入一个采样值并返回滑动平均结果。
 * @param  filter 滤波器指针。
 * @param  input 当前采样值。
 * @retval 当前窗口的平均值，滤波器无效时返回输入值。
 * @note   未填满窗口时按实际样本数量计算平均值。
 */
float Filter_MovingAverageUpdate(MovingAverageFilter_T *filter, float input)
{
    if ((filter == NULL) || (filter->length == 0u))
    {
        return input;
    }

    /* 移除最旧样本，再把新样本加入循环缓冲区 */
    filter->sum -= filter->buffer[filter->index];
    filter->buffer[filter->index] = input;
    filter->sum += input;
    filter->index = (uint16_t)((filter->index + 1u) % filter->length);
    if (filter->count < filter->length)
    {
        filter->count++;
    }

    return filter->sum / (float)filter->count;
}

/**
 * @brief  初始化一阶低通滤波器。
 * @param  filter 滤波器指针。
 * @param  coefficient 新采样值所占权重，取值范围为零到一。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   权重越小，输出越平滑。
 */
int8_t Filter_LowPassInit(FirstOrderLowPassFilter_T *filter, float coefficient)
{
    if ((filter == NULL) || (coefficient < 0.0f) || (coefficient > 1.0f))
    {
        return FILTER_RESULT_INVALID;
    }

    filter->coefficient = coefficient;
    filter->output = 0.0f;
    filter->initialized = 0u;

    return FILTER_RESULT_OK;
}

/**
 * @brief  输入一个采样值并返回一阶低通滤波结果。
 * @param  filter 滤波器指针。
 * @param  input 当前采样值。
 * @retval 滤波结果，滤波器无效时返回输入值。
 * @note   第一次调用直接使用输入值初始化输出。
 */
float Filter_LowPassUpdate(FirstOrderLowPassFilter_T *filter, float input)
{
    if (filter == NULL)
    {
        return input;
    }

    if (filter->initialized == 0u)
    {
        filter->output = input;
        filter->initialized = 1u;
    }
    else
    {
        /* 旧输出保留一部分，新输入补充剩余权重 */
        filter->output += filter->coefficient * (input - filter->output);
    }

    return filter->output;
}

/**
 * @brief  初始化奇数长度的中值滤波器。
 * @param  filter 滤波器指针。
 * @param  length 中值窗口长度。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   窗口长度必须为奇数。
 */
int8_t Filter_MedianInit(MedianFilter_T *filter, uint16_t length)
{
    uint16_t index;

    if ((filter == NULL) || (length == 0u) || (length > FILTER_MEDIAN_MAX_LENGTH) || ((length % 2u) == 0u))
    {
        return FILTER_RESULT_INVALID;
    }

    /* 清空窗口，后续只对已经写入的有效样本排序 */
    for (index = 0u; index < FILTER_MEDIAN_MAX_LENGTH; index++)
    {
        filter->buffer[index] = 0.0f;
    }
    filter->length = length;
    filter->index = 0u;
    filter->count = 0u;

    return FILTER_RESULT_OK;
}

/**
 * @brief  输入一个采样值并返回窗口中值。
 * @param  filter 滤波器指针。
 * @param  input 当前采样值。
 * @retval 当前窗口中值，滤波器无效时返回输入值。
 * @note   使用局部数组排序，不会改变原循环缓冲区的时间顺序。
 */
float Filter_MedianUpdate(MedianFilter_T *filter, float input)
{
    float sorted[FILTER_MEDIAN_MAX_LENGTH];
    uint16_t outerIndex;
    uint16_t innerIndex;

    if ((filter == NULL) || (filter->length == 0u))
    {
        return input;
    }

    /* 更新循环窗口，并复制当前有效样本到排序缓冲区 */
    filter->buffer[filter->index] = input;
    filter->index = (uint16_t)((filter->index + 1u) % filter->length);
    if (filter->count < filter->length)
    {
        filter->count++;
    }
    for (outerIndex = 0u; outerIndex < filter->count; outerIndex++)
    {
        sorted[outerIndex] = filter->buffer[outerIndex];
    }

    /* 插入排序适合样本数量很少的固定窗口 */
    for (outerIndex = 1u; outerIndex < filter->count; outerIndex++)
    {
        float selected = sorted[outerIndex];
        innerIndex = outerIndex;
        while ((innerIndex > 0u) && (sorted[innerIndex - 1u] > selected))
        {
            sorted[innerIndex] = sorted[innerIndex - 1u];
            innerIndex--;
        }
        sorted[innerIndex] = selected;
    }

    /* 偶数个启动样本取中间两项均值，窗口填满后返回正中元素 */
    if ((filter->count % 2u) == 0u)
    {
        return 0.5f * (sorted[filter->count / 2u - 1u] + sorted[filter->count / 2u]);
    }

    return sorted[filter->count / 2u];
}

/**
 * @brief  初始化用于估计常量状态的一维卡尔曼滤波器。
 * @param  filter 滤波器指针。
 * @param  processNoise 过程噪声协方差。
 * @param  measurementNoise 测量噪声协方差。
 * @param  initialEstimate 初始状态估计。
 * @param  initialCovariance 初始估计误差协方差。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   两种噪声参数和初始协方差都必须为正数。
 */
int8_t Filter_KalmanInit(KalmanFilter_T *filter, float processNoise, float measurementNoise, float initialEstimate, float initialCovariance)
{
    if ((filter == NULL) || (processNoise <= 0.0f) || (measurementNoise <= 0.0f) || (initialCovariance <= 0.0f))
    {
        return FILTER_RESULT_INVALID;
    }

    filter->processNoise = processNoise;
    filter->measurementNoise = measurementNoise;
    filter->estimate = initialEstimate;
    filter->errorCovariance = initialCovariance;
    filter->initialized = 1u;

    return FILTER_RESULT_OK;
}

/**
 * @brief  使用当前测量值完成一次一维卡尔曼更新。
 * @param  filter 滤波器指针。
 * @param  measurement 当前测量值。
 * @retval 更新后的状态估计，滤波器无效时返回测量值。
 * @note   本模型假定相邻采样时刻的真实状态变化较缓慢。
 */
float Filter_KalmanUpdate(KalmanFilter_T *filter, float measurement)
{
    float predictedCovariance;
    float kalmanGain;

    if ((filter == NULL) || (filter->initialized == 0u))
    {
        return measurement;
    }

    /* 预测阶段扩大不确定性，再用测量信息修正状态与协方差 */
    predictedCovariance = filter->errorCovariance + filter->processNoise;
    kalmanGain = predictedCovariance / (predictedCovariance + filter->measurementNoise);
    filter->estimate += kalmanGain * (measurement - filter->estimate);
    filter->errorCovariance = (1.0f - kalmanGain) * predictedCovariance;

    return filter->estimate;
}
