#ifndef __BSP_MEMORY_H
#define __BSP_MEMORY_H

#include <stdint.h>

/* PC 测试桩：串口参数设置接口 */
void UART_SetBaudRate(uint32_t comIndex, uint32_t baudRate);
void UART_SetParity(uint32_t comIndex, uint32_t parity);
void UART_SetStopBits(uint32_t comIndex, uint32_t stopBits);

#define USART_PARITY_NONE   0u
#define USART_STOPBITS_1    1u

#endif /* __BSP_MEMORY_H */
