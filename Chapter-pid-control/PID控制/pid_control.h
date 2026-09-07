#ifndef __PID_CONTROL_H
#define __PID_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PID 控制器工作模式 */
typedef enum __PIDMode_T
{
    PID_MODE_POSITIONAL = 0,
    PID_MODE_INCREMENTAL
} PIDMode_T;

/* PID 控制器运行参数与历史状态 */
typedef struct __PIDController_T
{
    float kp;
    float ki;
    float kd;
    float sampleTime;
    float integral;
    float previousError;
    float earlierError;
    float previousOutput;
    float derivativeState;
    float derivativeFilterCoefficient;
    float integralMin;
    float integralMax;
    float outputMin;
    float outputMax;
    PIDMode_T mode;
} PIDController_T;

int8_t PID_Init(PIDController_T *controller, PIDMode_T mode, float sampleTime);
void PID_Reset(PIDController_T *controller);
int8_t PID_SetTunings(PIDController_T *controller, float kp, float ki, float kd);
int8_t PID_SetLimits(PIDController_T *controller, float integralMin, float integralMax, float outputMin, float outputMax);
int8_t PID_SetDerivativeFilter(PIDController_T *controller, float coefficient);
float PID_Calculate(PIDController_T *controller, float setpoint, float feedback);

#ifdef __cplusplus
}
#endif

#endif /* __PID_CONTROL_H */
