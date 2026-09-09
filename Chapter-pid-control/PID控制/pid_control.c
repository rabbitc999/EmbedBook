#include "pid_control.h"

#include <stddef.h>

/* PID 模块统一返回码 */
#define PID_RESULT_OK       0
#define PID_RESULT_INVALID -1

/**
 * @brief  将浮点数限制在给定区间内。
 * @param  value 待限制的数值。
 * @param  minimum 区间下限。
 * @param  maximum 区间上限。
 * @retval 限幅后的数值。
 * @note   调用前应保证下限小于上限。
 */
static float PID_Clamp(float value, float minimum, float maximum)
{
    /* 依次处理上限与下限，避免控制量越过执行器能力范围 */
    if (value > maximum)
    {
        return maximum;
    }
    if (value < minimum)
    {
        return minimum;
    }

    return value;
}

/**
 * @brief  初始化 PID 控制器并清空历史状态。
 * @param  controller PID 控制器指针。
 * @param  mode 位置式或增量式工作模式。
 * @param  sampleTime 控制周期，单位为秒。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   初始化后还需要设置控制参数与限幅范围。
 */
int8_t PID_Init(PIDController_T *controller, PIDMode_T mode, float sampleTime)
{
    if ((controller == NULL) || (sampleTime <= 0.0f))
    {
        return PID_RESULT_INVALID;
    }

    /* 设置一组安全的初值，避免控制器首次运行时产生异常输出 */
    controller->kp = 0.0f;
    controller->ki = 0.0f;
    controller->kd = 0.0f;
    controller->sampleTime = sampleTime;
    controller->derivativeFilterCoefficient = 1.0f;
    controller->integralMin = -1.0f;
    controller->integralMax = 1.0f;
    controller->outputMin = -1.0f;
    controller->outputMax = 1.0f;
    controller->mode = mode;
    PID_Reset(controller);

    return PID_RESULT_OK;
}

/**
 * @brief  清空 PID 控制器的积分量与历史误差。
 * @param  controller PID 控制器指针。
 * @retval 无。
 * @note   切换工作状态或设定值突变时可调用本函数。
 */
void PID_Reset(PIDController_T *controller)
{
    if (controller == NULL)
    {
        return;
    }

    /* 所有动态状态归零，但保留控制参数和限幅配置 */
    controller->integral = 0.0f;
    controller->previousError = 0.0f;
    controller->earlierError = 0.0f;
    controller->previousOutput = 0.0f;
    controller->derivativeState = 0.0f;
}

/**
 * @brief  设置比例、积分与微分参数。
 * @param  controller PID 控制器指针。
 * @param  kp 比例系数。
 * @param  ki 积分系数。
 * @param  kd 微分系数。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   示例限定三个系数均为非负数。
 */
int8_t PID_SetTunings(PIDController_T *controller, float kp, float ki, float kd)
{
    if ((controller == NULL) || (kp < 0.0f) || (ki < 0.0f) || (kd < 0.0f))
    {
        return PID_RESULT_INVALID;
    }

    controller->kp = kp;
    controller->ki = ki;
    controller->kd = kd;

    return PID_RESULT_OK;
}

/**
 * @brief  设置积分项和控制输出的限幅范围。
 * @param  controller PID 控制器指针。
 * @param  integralMin 积分项下限。
 * @param  integralMax 积分项上限。
 * @param  outputMin 控制输出下限。
 * @param  outputMax 控制输出上限。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   下限必须严格小于对应的上限。
 */
int8_t PID_SetLimits(PIDController_T *controller, float integralMin, float integralMax, float outputMin, float outputMax)
{
    if ((controller == NULL) || (integralMin >= integralMax) || (outputMin >= outputMax))
    {
        return PID_RESULT_INVALID;
    }

    controller->integralMin = integralMin;
    controller->integralMax = integralMax;
    controller->outputMin = outputMin;
    controller->outputMax = outputMax;

    return PID_RESULT_OK;
}

/**
 * @brief  设置微分低通滤波系数。
 * @param  controller PID 控制器指针。
 * @param  coefficient 滤波系数，取值范围为零到一。
 * @retval 0 表示成功，负数表示参数无效。
 * @note   系数越小，微分项越平滑，但响应延迟也越明显。
 */
int8_t PID_SetDerivativeFilter(PIDController_T *controller, float coefficient)
{
    if ((controller == NULL) || (coefficient < 0.0f) || (coefficient > 1.0f))
    {
        return PID_RESULT_INVALID;
    }

    controller->derivativeFilterCoefficient = coefficient;

    return PID_RESULT_OK;
}

/**
 * @brief  根据设定值与反馈值计算本周期控制输出。
 * @param  controller PID 控制器指针。
 * @param  setpoint 目标设定值。
 * @param  feedback 被控对象反馈值。
 * @retval 限幅后的控制输出，控制器无效时返回零。
 * @note   本函数应以固定周期调用。
 */
float PID_Calculate(PIDController_T *controller, float setpoint, float feedback)
{
    float error;
    float output;

    if (controller == NULL)
    {
        return 0.0f;
    }

    error = setpoint - feedback;

    if (controller->mode == PID_MODE_INCREMENTAL)
    {
        float increment;

        /* 增量式 PID 只计算本周期相对于上一周期的输出变化量 */
        increment = controller->kp * (error - controller->previousError);
        increment += controller->ki * controller->sampleTime * error;
        increment += controller->kd * (error - 2.0f * controller->previousError + controller->earlierError)
                     / controller->sampleTime;
        output = PID_Clamp(controller->previousOutput + increment, controller->outputMin, controller->outputMax);
    }
    else
    {
        float rawDerivative;

        /* 位置式 PID 对积分项限幅，并对误差微分进行一阶低通滤波 */
        controller->integral += controller->ki * controller->sampleTime * error;
        controller->integral = PID_Clamp(controller->integral, controller->integralMin, controller->integralMax);
        rawDerivative = (error - controller->previousError) / controller->sampleTime;
        controller->derivativeState += controller->derivativeFilterCoefficient
                                       * (rawDerivative - controller->derivativeState);
        output = controller->kp * error + controller->integral + controller->kd * controller->derivativeState;
        output = PID_Clamp(output, controller->outputMin, controller->outputMax);
    }

    /* 保存历史量，为下一控制周期提供差分数据 */
    controller->earlierError = controller->previousError;
    controller->previousError = error;
    controller->previousOutput = output;

    return output;
}
