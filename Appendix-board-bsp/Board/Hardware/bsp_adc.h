#ifndef __AD_H
#define __AD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void AD_Init(void);
uint16_t AD_GetValue(uint8_t ADC_Channel);

#ifdef __cplusplus
}
#endif

#endif
