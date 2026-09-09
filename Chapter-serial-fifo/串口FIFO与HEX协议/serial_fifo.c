/**
 * @file    serial_fifo.c
 * @brief   串口循环队列（FIFO）缓冲区
 * @details 头尾下标都允许回绕；写入/读取跨过缓冲区末尾时分两段拷贝。
 */

#include <stddef.h>
#include "serial_fifo.h"

/**
 * @brief  内存拷贝
 * @param  pDst 目标地址
 * @param  pSrc 源地址
 * @param  len  长度
 */
static void Serial_MemoryCopy(uint8_t *pDst,
                              const uint8_t *pSrc,
                              uint32_t len)
{
    while (len-- > 0u) {
        *pDst++ = *pSrc++;
    }
}

/**
 * @brief  初始化 FIFO
 * @param  pFifo    FIFO 句柄
 * @param  pBuffer  缓冲区
 * @param  size     缓冲区大小
 */
void Serial_FifoInit(SerialFifo_T *pFifo,
                     uint8_t *pBuffer,
                     uint16_t size)
{
    if ((pFifo == NULL) || (pBuffer == NULL) || (size == 0u)) {
        return;
    }
    pFifo->Buffer = pBuffer;
    pFifo->Size = size;
    pFifo->StartIndex = 0u;
    pFifo->EndIndex = 0u;
    pFifo->TempBuffer = NULL;
    pFifo->TempBufferSize = 0u;
}

/**
 * @brief  获取 FIFO 中待读数据长度
 * @param  pFifo FIFO 句柄
 * @retval 待读长度
 */
uint32_t Serial_GetFifoLength(SerialFifo_T *pFifo)
{
    return (uint32_t)((pFifo->EndIndex + pFifo->Size - pFifo->StartIndex)
                      % pFifo->Size);
}

/**
 * @brief  向 FIFO 写入数据
 * @param  pFifo  FIFO 句柄
 * @param  pData  数据
 * @param  length 数据长度
 * @retval 实际写入长度，空间不足返回 0
 */
uint8_t Serial_FifoWriteBuffer(SerialFifo_T *pFifo,
                               uint8_t *pData,
                               uint8_t length)
{
    if ((pFifo == NULL) || (pData == NULL)) {
        return 0u;
    }

    /* 牺牲一个槽位区分“空/满”，最多存放 Size-1 字节 */
    if (((uint32_t)pFifo->Size - 1u - Serial_GetFifoLength(pFifo)) < length) {
        return 0u;
    }

    /* 无需分段：直接写入 */
    if (pFifo->EndIndex + length < pFifo->Size) {
        Serial_MemoryCopy(pFifo->Buffer + pFifo->EndIndex, pData, length);
        pFifo->EndIndex += length;
    } else {
        /* 先写到缓冲区末尾 */
        uint8_t firstLength = pFifo->Size - pFifo->EndIndex;
        Serial_MemoryCopy(pFifo->Buffer + pFifo->EndIndex, pData, firstLength);

        /* 再从头部写剩余部分 */
        Serial_MemoryCopy(pFifo->Buffer, pData + firstLength,
                          length - firstLength);
        pFifo->EndIndex = length - firstLength;
    }
    return length;
}

/**
 * @brief  从 FIFO 读出数据
 * @param  pFifo  FIFO 句柄
 * @param  pData  输出缓冲区
 * @param  length 期望读取长度
 * @retval 实际读出长度
 */
uint32_t Serial_FifoReadBuffer(SerialFifo_T *pFifo,
                               uint8_t *pData,
                               uint32_t length)
{
    uint32_t readLength = 0u;
    uint32_t firstLength = 0u;

    if ((pFifo == NULL) || (pData == NULL)) {
        return 0u;
    }

    /* 缓冲区为空 */
    if (pFifo->EndIndex == pFifo->StartIndex) {
        return 0u;
    }

    readLength = Serial_GetFifoLength(pFifo);
    if (readLength > length) {
        readLength = length;
    }

    if ((uint32_t)pFifo->StartIndex + readLength <= pFifo->Size) {
        Serial_MemoryCopy(pData, pFifo->Buffer + pFifo->StartIndex, readLength);
        pFifo->StartIndex += (uint16_t)readLength;
    } else {
        firstLength = pFifo->Size - pFifo->StartIndex;
        Serial_MemoryCopy(pData, pFifo->Buffer + pFifo->StartIndex, firstLength);
        Serial_MemoryCopy(pData + firstLength, pFifo->Buffer,
                          readLength - firstLength);
        pFifo->StartIndex = (uint16_t)(readLength - firstLength);
    }
    return readLength;
}
