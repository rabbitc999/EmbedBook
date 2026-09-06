#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif
    
uint8_t DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi);

#ifdef __cplusplus
}
#endif

#endif
