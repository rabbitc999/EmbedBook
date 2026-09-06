#ifndef _LCD_H
#define _LCD_H

#include "stm32f10x.h"

#include <stdint.h>

// 定义LCD方向枚举
typedef enum LCD_Direction
{
    LCD_Horizontal = 0,     // 水平
    LCD_RevHorizontal = 1,  // 水平反转
    LCD_Vertical = 2,       // 垂直
    LCD_RevVertical = 3     // 垂直反转
} LCD_Direction;

// 定义LCD方向枚举
typedef enum LCD_FontSize
{
    LCD_FontSize16 = 16,  // 1608，支持中文1616
    LCD_FontSize24 = 24,  // 2412
    LCD_FontSize32 = 32,  // 3216
} LCD_FontSize;

// 定义LCD方向枚举
typedef enum LCD_MonoFill
{
    LCD_Cover = 0,        // 覆盖模式
    LCD_Transparent = 1,  // 透明模式
} LCD_MonoFill;

// 定义LCD设备的宽度和高度
#define LCD_DEVICE 4  // 1为114屏幕（ST7789），2为154屏幕（ST7789），3为200屏幕（ST7789），4为240屏幕（ST7789），5为180屏幕（ST7735）

#if LCD_DEVICE == 1
#define LCD_DEVICE_HEIGHT 240
#define LCD_DEVICE_WIDTH  135
#elif LCD_DEVICE == 2
#define LCD_DEVICE_HEIGHT 240
#define LCD_DEVICE_WIDTH  240
#elif LCD_DEVICE == 3
#define LCD_DEVICE_HEIGHT 240
#define LCD_DEVICE_WIDTH  320
#elif LCD_DEVICE == 4
#define LCD_DEVICE_HEIGHT 240
#define LCD_DEVICE_WIDTH  320
#else
#define LCD_DEVICE_HEIGHT 128
#define LCD_DEVICE_WIDTH  160
#endif

// 定义常用颜色宏
#define LCD_WHITE       0xFFFF
#define LCD_BLACK       0x0000
#define LCD_BLUE        0x001F
#define LCD_BRED        0XF81F
#define LCD_RED         0xF800
#define LCD_MAGENTA     0xF81F
#define LCD_GREEN       0x07E0
#define LCD_CYAN        0x7FFF
#define LCD_YELLOW      0xFFE0
#define LCD_BROWN       0XBC40  // 棕色
#define LCD_GRAY        0X8430  // 灰色
#define LCD_DARK_BLUE   0X01CF  // 深蓝色
#define LCD_LIGHT_BLUE  0X7D7C  // 浅蓝色
#define LCD_GRAY_BLUE   0X5458  // 灰蓝色
#define LCD_LIGHT_GREEN 0X841F  // 浅绿色

// 函数声明
void LCD_Init(void);
void LCD_SetDirection(LCD_Direction Direction);
void LCD_SetBackColor(uint16_t Color);
void LCD_SetPaintColor(uint16_t Color);
void LCD_SetFontSize(LCD_FontSize Size);
void LCD_SetMonoFill(LCD_MonoFill Fill);
void LCD_DrawPoint(uint16_t X, uint16_t Y);
void LCD_ShowChar(uint16_t X, uint16_t Y, uint8_t ShowChar);
void LCD_ShowString(uint16_t X, uint16_t Y, const char *String);
void LCD_ShowNumber(uint16_t X, uint16_t Y, uint32_t Number, uint8_t Length);
void LCD_ShowSignedNumber(uint16_t X, uint16_t Y, int32_t Number, uint8_t Length);
void LCD_ShowHexNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length);
void LCD_ShowBinNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length);
void LCD_ShowFloatNumber(int16_t X, int16_t Y, float Number, uint8_t IntLength, uint8_t FraLength);
void LCD_ShowImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image);
void LCD_ShowMonoImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image);
void LCD_Fill(uint16_t StartX, uint16_t StartY, uint16_t EndX, uint16_t EndY, uint16_t Color);
void LCD_Printf(uint16_t X, uint16_t Y, const char *String, ...);
void LCD_Clear(void);

// 绘图函数
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1);
void LCD_DrawRectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height);
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2);
void LCD_DrawPoly(uint16_t vx[], uint16_t vy[], int n);
void LCD_DrawCircle(uint16_t X, uint16_t Y, uint8_t Radius);
void LCD_DrawEllipse(uint16_t X, uint16_t Y, uint8_t A, uint8_t B);
void LCD_DrawArc(uint16_t X, uint16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle);

// 定义一个结构体，用于存储LCD设备的相关参数
struct _LCD
{
    LCD_Direction Direction;  // 显示方向
    LCD_FontSize FontSize;    // 字体大小
    LCD_MonoFill MonoFill;    // 单色覆盖
    uint16_t Height;          // 显示高度
    uint16_t Width;           // 显示宽度
    uint16_t BackColor;       // 背景颜色
    uint16_t PaintColor;      // 绘图颜色
};

extern struct _LCD LCD;

#endif  // _LCD_H
