#ifndef __BSP_PERIPH_H
#define __BSP_PERIPH_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_AF_NONE  0      /* 不使用复用模式 */
#define GPIO_PIN_ALL  0xFFFF /* 全部引脚 */
#define GPIO_PIN_LOW  0x00FF /* 低8位引脚 */
#define GPIO_PIN_HIGH 0xFF00 /* 高8位引脚 */

typedef enum __GPIO_PORT_T
{
    GPIO_A = 0x00010000,
    GPIO_B = 0x00020000,
    GPIO_C = 0x00040000,
    GPIO_D = 0x00080000,
    GPIO_E = 0x00100000,
    GPIO_F = 0x00200000,
    GPIO_G = 0x00400000,
    GPIO_H = 0x00800000,
    GPIO_I = 0x01000000,
    GPIO_J = 0x02000000,
    GPIO_K = 0x04000000
} GPIO_PORT_T;

typedef enum __GPIO_IO_T
{
    PA_ALL = GPIO_A | GPIO_PIN_ALL,
    PA_LOW = GPIO_A | GPIO_PIN_LOW,
    PA_HIGH = GPIO_A | GPIO_PIN_HIGH,
    PA0 = GPIO_A | GPIO_PIN_0,
    PA1 = GPIO_A | GPIO_PIN_1,
    PA2 = GPIO_A | GPIO_PIN_2,
    PA3 = GPIO_A | GPIO_PIN_3,
    PA4 = GPIO_A | GPIO_PIN_4,
    PA5 = GPIO_A | GPIO_PIN_5,
    PA6 = GPIO_A | GPIO_PIN_6,
    PA7 = GPIO_A | GPIO_PIN_7,
    PA8 = GPIO_A | GPIO_PIN_8,
    PA9 = GPIO_A | GPIO_PIN_9,
    PA10 = GPIO_A | GPIO_PIN_10,
    PA11 = GPIO_A | GPIO_PIN_11,
    PA12 = GPIO_A | GPIO_PIN_12,
    PA13 = GPIO_A | GPIO_PIN_13,
    PA14 = GPIO_A | GPIO_PIN_14,
    PA15 = GPIO_A | GPIO_PIN_15,
    PB_ALL = GPIO_B | GPIO_PIN_ALL,
    PB_LOW = GPIO_B | GPIO_PIN_LOW,
    PB_HIGH = GPIO_B | GPIO_PIN_HIGH,
    PB0 = GPIO_B | GPIO_PIN_0,
    PB1 = GPIO_B | GPIO_PIN_1,
    PB2 = GPIO_B | GPIO_PIN_2,
    PB3 = GPIO_B | GPIO_PIN_3,
    PB4 = GPIO_B | GPIO_PIN_4,
    PB5 = GPIO_B | GPIO_PIN_5,
    PB6 = GPIO_B | GPIO_PIN_6,
    PB7 = GPIO_B | GPIO_PIN_7,
    PB8 = GPIO_B | GPIO_PIN_8,
    PB9 = GPIO_B | GPIO_PIN_9,
    PB10 = GPIO_B | GPIO_PIN_10,
    PB11 = GPIO_B | GPIO_PIN_11,
    PB12 = GPIO_B | GPIO_PIN_12,
    PB13 = GPIO_B | GPIO_PIN_13,
    PB14 = GPIO_B | GPIO_PIN_14,
    PB15 = GPIO_B | GPIO_PIN_15,
    PC_ALL = GPIO_C | GPIO_PIN_ALL,
    PC_LOW = GPIO_C | GPIO_PIN_LOW,
    PC_HIGH = GPIO_C | GPIO_PIN_HIGH,
    PC0 = GPIO_C | GPIO_PIN_0,
    PC1 = GPIO_C | GPIO_PIN_1,
    PC2 = GPIO_C | GPIO_PIN_2,
    PC3 = GPIO_C | GPIO_PIN_3,
    PC4 = GPIO_C | GPIO_PIN_4,
    PC5 = GPIO_C | GPIO_PIN_5,
    PC6 = GPIO_C | GPIO_PIN_6,
    PC7 = GPIO_C | GPIO_PIN_7,
    PC8 = GPIO_C | GPIO_PIN_8,
    PC9 = GPIO_C | GPIO_PIN_9,
    PC10 = GPIO_C | GPIO_PIN_10,
    PC11 = GPIO_C | GPIO_PIN_11,
    PC12 = GPIO_C | GPIO_PIN_12,
    PC13 = GPIO_C | GPIO_PIN_13,
    PC14 = GPIO_C | GPIO_PIN_14,
    PC15 = GPIO_C | GPIO_PIN_15,
    PD_ALL = GPIO_D | GPIO_PIN_ALL,
    PD_LOW = GPIO_D | GPIO_PIN_LOW,
    PD_HIGH = GPIO_D | GPIO_PIN_HIGH,
    PD0 = GPIO_D | GPIO_PIN_0,
    PD1 = GPIO_D | GPIO_PIN_1,
    PD2 = GPIO_D | GPIO_PIN_2,
    PD3 = GPIO_D | GPIO_PIN_3,
    PD4 = GPIO_D | GPIO_PIN_4,
    PD5 = GPIO_D | GPIO_PIN_5,
    PD6 = GPIO_D | GPIO_PIN_6,
    PD7 = GPIO_D | GPIO_PIN_7,
    PD8 = GPIO_D | GPIO_PIN_8,
    PD9 = GPIO_D | GPIO_PIN_9,
    PD10 = GPIO_D | GPIO_PIN_10,
    PD11 = GPIO_D | GPIO_PIN_11,
    PD12 = GPIO_D | GPIO_PIN_12,
    PD13 = GPIO_D | GPIO_PIN_13,
    PD14 = GPIO_D | GPIO_PIN_14,
    PD15 = GPIO_D | GPIO_PIN_15,
    PE_ALL = GPIO_E | GPIO_PIN_ALL,
    PE_LOW = GPIO_E | GPIO_PIN_LOW,
    PE_HIGH = GPIO_E | GPIO_PIN_HIGH,
    PE0 = GPIO_E | GPIO_PIN_0,
    PE1 = GPIO_E | GPIO_PIN_1,
    PE2 = GPIO_E | GPIO_PIN_2,
    PE3 = GPIO_E | GPIO_PIN_3,
    PE4 = GPIO_E | GPIO_PIN_4,
    PE5 = GPIO_E | GPIO_PIN_5,
    PE6 = GPIO_E | GPIO_PIN_6,
    PE7 = GPIO_E | GPIO_PIN_7,
    PE8 = GPIO_E | GPIO_PIN_8,
    PE9 = GPIO_E | GPIO_PIN_9,
    PE10 = GPIO_E | GPIO_PIN_10,
    PE11 = GPIO_E | GPIO_PIN_11,
    PE12 = GPIO_E | GPIO_PIN_12,
    PE13 = GPIO_E | GPIO_PIN_13,
    PE14 = GPIO_E | GPIO_PIN_14,
    PE15 = GPIO_E | GPIO_PIN_15,
    PF_ALL = GPIO_F | GPIO_PIN_ALL,
    PF_LOW = GPIO_F | GPIO_PIN_LOW,
    PF_HIGH = GPIO_F | GPIO_PIN_HIGH,
    PF0 = GPIO_F | GPIO_PIN_0,
    PF1 = GPIO_F | GPIO_PIN_1,
    PF2 = GPIO_F | GPIO_PIN_2,
    PF3 = GPIO_F | GPIO_PIN_3,
    PF4 = GPIO_F | GPIO_PIN_4,
    PF5 = GPIO_F | GPIO_PIN_5,
    PF6 = GPIO_F | GPIO_PIN_6,
    PF7 = GPIO_F | GPIO_PIN_7,
    PF8 = GPIO_F | GPIO_PIN_8,
    PF9 = GPIO_F | GPIO_PIN_9,
    PF10 = GPIO_F | GPIO_PIN_10,
    PF11 = GPIO_F | GPIO_PIN_11,
    PF12 = GPIO_F | GPIO_PIN_12,
    PF13 = GPIO_F | GPIO_PIN_13,
    PF14 = GPIO_F | GPIO_PIN_14,
    PF15 = GPIO_F | GPIO_PIN_15,
    PG_ALL = GPIO_G | GPIO_PIN_ALL,
    PG_LOW = GPIO_G | GPIO_PIN_LOW,
    PG_HIGH = GPIO_G | GPIO_PIN_HIGH,
    PG0 = GPIO_G | GPIO_PIN_0,
    PG1 = GPIO_G | GPIO_PIN_1,
    PG2 = GPIO_G | GPIO_PIN_2,
    PG3 = GPIO_G | GPIO_PIN_3,
    PG4 = GPIO_G | GPIO_PIN_4,
    PG5 = GPIO_G | GPIO_PIN_5,
    PG6 = GPIO_G | GPIO_PIN_6,
    PG7 = GPIO_G | GPIO_PIN_7,
    PG8 = GPIO_G | GPIO_PIN_8,
    PG9 = GPIO_G | GPIO_PIN_9,
    PG10 = GPIO_G | GPIO_PIN_10,
    PG11 = GPIO_G | GPIO_PIN_11,
    PG12 = GPIO_G | GPIO_PIN_12,
    PG13 = GPIO_G | GPIO_PIN_13,
    PG14 = GPIO_G | GPIO_PIN_14,
    PG15 = GPIO_G | GPIO_PIN_15,
    PH_ALL = GPIO_H | GPIO_PIN_ALL,
    PH_LOW = GPIO_H | GPIO_PIN_LOW,
    PH_HIGH = GPIO_H | GPIO_PIN_HIGH,
    PH0 = GPIO_H | GPIO_PIN_0,
    PH1 = GPIO_H | GPIO_PIN_1,
    PH2 = GPIO_H | GPIO_PIN_2,
    PH3 = GPIO_H | GPIO_PIN_3,
    PH4 = GPIO_H | GPIO_PIN_4,
    PH5 = GPIO_H | GPIO_PIN_5,
    PH6 = GPIO_H | GPIO_PIN_6,
    PH7 = GPIO_H | GPIO_PIN_7,
    PH8 = GPIO_H | GPIO_PIN_8,
    PH9 = GPIO_H | GPIO_PIN_9,
    PH10 = GPIO_H | GPIO_PIN_10,
    PH11 = GPIO_H | GPIO_PIN_11,
    PH12 = GPIO_H | GPIO_PIN_12,
    PH13 = GPIO_H | GPIO_PIN_13,
    PH14 = GPIO_H | GPIO_PIN_14,
    PH15 = GPIO_H | GPIO_PIN_15,
    PI_ALL = GPIO_I | GPIO_PIN_ALL,
    PI_LOW = GPIO_I | GPIO_PIN_LOW,
    PI_HIGH = GPIO_I | GPIO_PIN_HIGH,
    PI0 = GPIO_I | GPIO_PIN_0,
    PI1 = GPIO_I | GPIO_PIN_1,
    PI2 = GPIO_I | GPIO_PIN_2,
    PI3 = GPIO_I | GPIO_PIN_3,
    PI4 = GPIO_I | GPIO_PIN_4,
    PI5 = GPIO_I | GPIO_PIN_5,
    PI6 = GPIO_I | GPIO_PIN_6,
    PI7 = GPIO_I | GPIO_PIN_7,
    PI8 = GPIO_I | GPIO_PIN_8,
    PI9 = GPIO_I | GPIO_PIN_9,
    PI10 = GPIO_I | GPIO_PIN_10,
    PI11 = GPIO_I | GPIO_PIN_11,
    PI12 = GPIO_I | GPIO_PIN_12,
    PI13 = GPIO_I | GPIO_PIN_13,
    PI14 = GPIO_I | GPIO_PIN_14,
    PI15 = GPIO_I | GPIO_PIN_15,
    PJ_ALL = GPIO_J | GPIO_PIN_ALL,
    PJ_LOW = GPIO_J | GPIO_PIN_LOW,
    PJ_HIGH = GPIO_J | GPIO_PIN_HIGH,
    PJ0 = GPIO_J | GPIO_PIN_0,
    PJ1 = GPIO_J | GPIO_PIN_1,
    PJ2 = GPIO_J | GPIO_PIN_2,
    PJ3 = GPIO_J | GPIO_PIN_3,
    PJ4 = GPIO_J | GPIO_PIN_4,
    PJ5 = GPIO_J | GPIO_PIN_5,
    PJ6 = GPIO_J | GPIO_PIN_6,
    PJ7 = GPIO_J | GPIO_PIN_7,
    PJ8 = GPIO_J | GPIO_PIN_8,
    PJ9 = GPIO_J | GPIO_PIN_9,
    PJ10 = GPIO_J | GPIO_PIN_10,
    PJ11 = GPIO_J | GPIO_PIN_11,
    PJ12 = GPIO_J | GPIO_PIN_12,
    PJ13 = GPIO_J | GPIO_PIN_13,
    PJ14 = GPIO_J | GPIO_PIN_14,
    PJ15 = GPIO_J | GPIO_PIN_15,
    PK0 = GPIO_K | GPIO_PIN_0,
    PK1 = GPIO_K | GPIO_PIN_1,
    PK2 = GPIO_K | GPIO_PIN_2,
    PK3 = GPIO_K | GPIO_PIN_3,
    PK4 = GPIO_K | GPIO_PIN_4,
    PK5 = GPIO_K | GPIO_PIN_5,
    PK6 = GPIO_K | GPIO_PIN_6,
    PK7 = GPIO_K | GPIO_PIN_7,
    PK8 = GPIO_K | GPIO_PIN_8,
    PK9 = GPIO_K | GPIO_PIN_9,
    PK10 = GPIO_K | GPIO_PIN_10,
    PK11 = GPIO_K | GPIO_PIN_11,
    PK12 = GPIO_K | GPIO_PIN_12,
    PK13 = GPIO_K | GPIO_PIN_13,
    PK14 = GPIO_K | GPIO_PIN_14,
    PK15 = GPIO_K | GPIO_PIN_15,
} GPIO_IO_T;

typedef enum __GPIO_MODE_T
{
    INPUT_U = 0,         /* 上拉输入 */
    INPUT_D = 1,         /* 下拉输入 */
    INPUT_F = 2,         /* 浮空输入 */
    OUTPUT_PP = 3,       /* 推挽输出 */
    OUTPUT_OD_U = 4,     /* 开漏输出，上拉 */
    OUTPUT_OD_D = 5,     /* 开漏输出，下拉 */
    OUTPUT_OD_F = 6,     /* 开漏输出，浮空 */
    OUTPUT_AF_PP = 7,    /* 复用推挽输出 */
    OUTPUT_AF_OD_U = 8,  /* 复用开漏输出，上拉 */
    OUTPUT_AF_OD_D = 9,  /* 复用开漏输出，下拉 */
    OUTPUT_AF_OD_F = 10, /* 复用开漏输出，浮空 */
    ANALOG = 11          /* 模拟输入 */
} GPIO_MODE_T;

void BSP_GPIO_ClkEnable(GPIO_TypeDef *port);
void BSP_TIM_ClkEnable(TIM_TypeDef *timInstance);
void BSP_I2C_ClkEnable(I2C_TypeDef *i2cInstance);
void BSP_SPI_ClkEnable(SPI_TypeDef *spiInstance);
void BSP_USART_ClkEnable(USART_TypeDef *usartInstance);
void BSP_DMA_ClkEnable(DMA_TypeDef *dmaInstance);

void BSP_GPIO_Init(GPIO_IO_T IO, GPIO_MODE_T mode);
void BSP_GPIO_InitEx(GPIO_IO_T IO, GPIO_MODE_T mode, uint8_t alternate);
void BSP_GPIO_WritePin(GPIO_IO_T IO, uint8_t state);
uint8_t BSP_GPIO_ReadPin(GPIO_IO_T IO);
static inline void BSP_GPIO_TogglePin(GPIO_IO_T IO);
uint16_t BSP_GPIO_ReadPinEx(GPIO_TypeDef *PORT, uint16_t MASK);
void BSP_GPIO_WritePinEx(GPIO_TypeDef *PORT, uint16_t MASK, uint16_t STATE);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_PERIPH_H */