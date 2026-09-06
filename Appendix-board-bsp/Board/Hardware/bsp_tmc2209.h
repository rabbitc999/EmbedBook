#ifndef BSP_TMC2209_H
#define BSP_TMC2209_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 细分 */
#define TMC2209_SUB_DIV_8  0  // 1/8步
#define TMC2209_SUB_DIV_16 1  // 1/16步
#define TMC2209_SUB_DIV_32 2  // 1/32步
#define TMC2209_SUB_DIV_64 3  // 1/64步

/* 方向控制 */
#define TMC2209_DIR_CW     0  // 顺时针
#define TMC2209_DIR_CCW    1  // 逆时针

void TMC2209_Init(void);
void TMC2209_Enable(void);
void TMC2209_Disable(void);
void TMC2209_SetSubdivision(uint8_t i);
void TMC2209_MoveStep(uint8_t Dir, uint32_t Step);
void TMC2209_MoveOneStep(uint8_t Dir);

#ifdef __cplusplus
}
#endif

#endif
