#include "pid_control.h"

#include <stdio.h>

/**
 * @brief  演示 PID 控制器驱动一阶惯性对象的过程。
 * @retval 0 表示示例执行结束，负数表示初始化失败。
 * @note   实际单片机中应由固定周期定时任务调用控制函数。
 */
int main(void)
{
    PIDController_T controller;
    float plantOutput = 0.0f;
    float controlOutput;
    uint16_t step;

    if (PID_Init(&controller, PID_MODE_POSITIONAL, 0.01f) != 0)
    {
        return -1;
    }

    /* 设置控制参数、积分限幅、输出限幅和微分滤波强度 */
    (void)PID_SetTunings(&controller, 1.8f, 0.8f, 0.05f);
    (void)PID_SetLimits(&controller, -30.0f, 30.0f, -100.0f, 100.0f);
    (void)PID_SetDerivativeFilter(&controller, 0.2f);

    /* 用简单的一阶惯性模型代替电机或加热器，观察闭环响应 */
    for (step = 0u; step < 200u; step++)
    {
        controlOutput = PID_Calculate(&controller, 50.0f, plantOutput);
        plantOutput += 0.01f * (-plantOutput + controlOutput);
        printf("%u,%.3f,%.3f\n", step, plantOutput, controlOutput);
    }

    return 0;
}
