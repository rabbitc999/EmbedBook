#ifndef __FFT_TRANSFORM_H
#define __FFT_TRANSFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FFT 支持的窗函数类型 */
typedef enum __FFTWindowType_T
{
    FFT_WINDOW_RECTANGULAR = 0,
    FFT_WINDOW_HANN,
    FFT_WINDOW_HAMMING,
    FFT_WINDOW_BLACKMAN
} FFTWindowType_T;

int8_t FFT_IsPowerOfTwo(uint16_t length);
int8_t FFT_ApplyWindow(float *samples, uint16_t length, FFTWindowType_T windowType);
int8_t FFT_Transform(float *realPart, float *imaginaryPart, uint16_t length);
int8_t FFT_CalcMagnitude(const float *realPart, const float *imaginaryPart, float *magnitude, uint16_t length);
float FFT_GetBinFrequency(uint16_t binIndex, float sampleFrequency, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __FFT_TRANSFORM_H */
