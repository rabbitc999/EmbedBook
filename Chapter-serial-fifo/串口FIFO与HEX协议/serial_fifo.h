#ifndef __SERIAL_FIFO_H
#define __SERIAL_FIFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 循环队列结构体 */
typedef struct __SerialFifo_T
{
    uint8_t  *Buffer;         /* 接收缓冲区数组 */
    volatile uint16_t StartIndex;  /* 读指针 */
    volatile uint16_t EndIndex;    /* 写指针 */
    uint16_t Size;            /* 缓冲区大小 */
    uint8_t  *TempBuffer;     /* 临时缓冲区（可选） */
    uint16_t TempBufferSize;  /* 临时缓冲区大小 */
} SerialFifo_T;

/* 初始化 FIFO */
void Serial_FifoInit(SerialFifo_T *pFifo,
                     uint8_t *pBuffer,
                     uint16_t size);

/* 获取待读长度 */
uint32_t Serial_GetFifoLength(SerialFifo_T *pFifo);

/* 写入，返回实际写入长度 */
uint8_t Serial_FifoWriteBuffer(SerialFifo_T *pFifo,
                               uint8_t *pData,
                               uint8_t length);

/* 读出，返回实际读出长度 */
uint32_t Serial_FifoReadBuffer(SerialFifo_T *pFifo,
                               uint8_t *pData,
                               uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* __SERIAL_FIFO_H */
