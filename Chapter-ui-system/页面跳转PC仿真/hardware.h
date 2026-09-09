#ifndef __PAGE_SIM_HARDWARE_H
#define __PAGE_SIM_HARDWARE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LCD 尺寸与字符尺寸（与菜单系统仿真保持一致） */
#define PAGE_SIM_WIDTH   240
#define PAGE_SIM_HEIGHT  300
#define PAGE_CHAR_W      8
#define PAGE_CHAR_H      16

void LCD_Init(void);
void LCD_Clear(void);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t rgb565);
void LCD_ShowRGB565Image(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height,
                         const uint16_t *pImage);

#ifdef __cplusplus
}
#endif

#endif /* __PAGE_SIM_HARDWARE_H */
