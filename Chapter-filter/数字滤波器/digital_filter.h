#ifndef __DIGITAL_FILTER_H
#define __DIGITAL_FILTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FILTER_MOVING_AVERAGE_MAX_LENGTH 32u
#define FILTER_MEDIAN_MAX_LENGTH          9u

/* 滑动平均滤波器状态 */
typedef struct __MovingAverageFilter_T
{
    float buffer[FILTER_MOVING_AVERAGE_MAX_LENGTH];
    float sum;
    uint16_t length;
    uint16_t index;
    uint16_t count;
} MovingAverageFilter_T;

/* 一阶低通滤波器状态 */
typedef struct __FirstOrderLowPassFilter_T
{
    float coefficient;
    float output;
    uint8_t initialized;
} FirstOrderLowPassFilter_T;

/* 中值滤波器状态 */
typedef struct __MedianFilter_T
{
    float buffer[FILTER_MEDIAN_MAX_LENGTH];
    uint16_t length;
    uint16_t index;
    uint16_t count;
} MedianFilter_T;

/* 一维卡尔曼滤波器状态 */
typedef struct __KalmanFilter_T
{
    float processNoise;
    float measurementNoise;
    float estimate;
    float errorCovariance;
    uint8_t initialized;
} KalmanFilter_T;

int8_t Filter_MovingAverageInit(MovingAverageFilter_T *filter, uint16_t length);
float Filter_MovingAverageUpdate(MovingAverageFilter_T *filter, float input);
int8_t Filter_LowPassInit(FirstOrderLowPassFilter_T *filter, float coefficient);
float Filter_LowPassUpdate(FirstOrderLowPassFilter_T *filter, float input);
int8_t Filter_MedianInit(MedianFilter_T *filter, uint16_t length);
float Filter_MedianUpdate(MedianFilter_T *filter, float input);
int8_t Filter_KalmanInit(KalmanFilter_T *filter, float processNoise, float measurementNoise, float initialEstimate, float initialCovariance);
float Filter_KalmanUpdate(KalmanFilter_T *filter, float measurement);

#ifdef __cplusplus
}
#endif

#endif /* __DIGITAL_FILTER_H */
