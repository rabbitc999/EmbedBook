#ifndef __CAR_H
#define __CAR_H

#include <stdint.h>

typedef enum __CarIndex_T
{
    CAR_INDEX_M1 = 0,
    CAR_INDEX_M2,
    CAR_INDEX_M3,
    CAR_INDEX_M4,
    CAR_MAX_INDEX /* 占位 */
} CarIndex_T;

typedef enum __CarMapper_T
{
    CAR_MAPPER_NONE = 0,
    CAR_MAPPER_MA_POS,
    CAR_MAPPER_MA_NEG,
    CAR_MAPPER_MB_POS,
    CAR_MAPPER_MB_NEG,
    CAR_MAPPER_MC_POS,
    CAR_MAPPER_MC_NEG,
    CAR_MAPPER_MD_POS,
    CAR_MAPPER_MD_NEG,
    CAR_MAX_MAPPER_INDEX
} CarMapper_T;

typedef enum __ChassisType_T
{
    CHASSIS_TYPE_DIFFERENTIAL_2 = 0,
    CHASSIS_TYPE_DIFFERENTIAL_4,
    CHASSIS_TYPE_MECANUM,
    CHASSIS_TYPE_OMNI_4,
    CHASSIS_TYPE_OMNI_3,
    CHASSIS_TYPE_ACKERMANN,
    CHASSIS_TYPE_COUNT
} ChassisType_T;

/* PID 参数（与真实工程一致） */
typedef struct __PidParam_T
{
    float Kp;
    float Ki;
    float Kd;
    float OutputLimit;
    float IntegralLimit;
} PidParam_T;

/* 测试桩仅包含 store.c 引用的成员 */
typedef struct __CarControl_T
{
    struct {
        float wheelBase;
        float remoteMaxSpeedX;
        float remoteMaxSpeedT;
        float wheelDiameter;
        PidParam_T pidLocationX;
        PidParam_T pidLocationY;
        PidParam_T pidYaw;
        PidParam_T pidMotorParam;
        uint32_t encoderPerRev;
        float slipK1;
        float slipK2;
        float slipTheta;
    } remote;

    struct {
        float frontAngleRange;
        float ultraSonicThreshold;
        float ctrlSpeedTGain;
    } avoidance;
} CarControl_T;

extern CarControl_T g_Car;

#endif /* __CAR_H */
