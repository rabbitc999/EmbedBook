#ifndef __IMU_H
#define __IMU_H

#include <stdint.h>

/* store.c 引用的激光雷达状态 */
typedef struct __LidarState_T
{
    float frontBackAngleCheckThreshold;
    float sideAngleCheckThreshold;
} LidarState_T;

extern LidarState_T g_LidarState;

#endif /* __IMU_H */
