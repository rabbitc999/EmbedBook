#ifndef BSP_HX711_H
#define BSP_HX711_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif
    
void HX711_Init(void);
int32_t HX711_ReadData(void);
uint32_t HX711_GetTare(void);
int32_t HX711_GetWeight(void);

#ifdef __cplusplus
}
#endif

#endif
