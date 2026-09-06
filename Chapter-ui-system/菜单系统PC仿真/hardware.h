#ifndef __HARDWARE_H
#define __HARDWARE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void LCD_Init(void);
void LCD_Clear(void);
void LCD_ShowString(uint16_t x, uint16_t y, const char* str);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t rgb565);
void LCD_ShowRGB565Image(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height,
                         const uint16_t *pImage);
uint8_t Key_Scan(void);
uint8_t Key_GetNum(void);
void Key_Clear(void);

#ifdef __cplusplus
};
#endif

#endif
