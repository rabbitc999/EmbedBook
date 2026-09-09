#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include "stm32f10x.h"

#include <stdint.h>

extern const uint8_t LCD_F16X08[][16];
extern const uint8_t LCD_F24X12[][48];
extern const uint8_t LCD_F32X16[][64];

typedef struct LCD_CH1616
{
    uint8_t Data[32];
    char Index[3];
} LCD_CH1616;

extern const LCD_CH1616 LCD_CF16X16[];

#endif
