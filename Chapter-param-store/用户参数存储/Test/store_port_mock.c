/**
 * @file    store_port_mock.c
 * @brief   第12章 store.c 的 PC 测试桩
 * @details 用静态数组模拟 QSPI Flash 与 FatFS 文件，使 store.c
 *          可以在没有单片机外设的电脑上被测试。
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "bsp_system.h"
#include "bsp_qspi_w25q64.h"
#include "car.h"
#include "imu.h"
#include "cmd.h"
#include "bsp_memory.h"
#include "file.h"

/* 全局对象 */
CarControl_T g_Car = { 0 };
LidarState_T g_LidarState = { 0 };
CmdControl_T g_CMD = { 0 };

/* 模拟 QSPI Flash：地址只支持 QSPI_ADDR_PARAM 区 */
#define MOCK_FLASH_SIZE (4096u)
static uint8_t s_Flash[MOCK_FLASH_SIZE];

/* 模拟 FatFS 文件 */
static uint8_t s_FileBuffer[MOCK_FLASH_SIZE];
static uint32_t s_FileLength = 0u;

void String_Copy(char *pDst, const char *pSrc, uint32_t maxLen)
{
    if ((pDst == NULL) || (pSrc == NULL) || (maxLen == 0u)) {
        return;
    }
    strncpy(pDst, pSrc, maxLen - 1u);
    pDst[maxLen - 1u] = '\0';
}

int W25QXX_ReadBuffer(uint8_t *pBuf, uint32_t addr, uint32_t len)
{
    if ((pBuf == NULL) || (addr < QSPI_ADDR_PARAM) ||
        ((addr - QSPI_ADDR_PARAM) + len > MOCK_FLASH_SIZE)) {
        return W25QXX_ERR;
    }
    memcpy(pBuf, s_Flash + (addr - QSPI_ADDR_PARAM), len);
    return W25QXX_OK;
}

int W25QXX_WriteBuffer(const uint8_t *pBuf, uint32_t addr, uint32_t len)
{
    if ((pBuf == NULL) || (addr < QSPI_ADDR_PARAM) ||
        ((addr - QSPI_ADDR_PARAM) + len > MOCK_FLASH_SIZE)) {
        return W25QXX_ERR;
    }
    memcpy(s_Flash + (addr - QSPI_ADDR_PARAM), pBuf, len);
    return W25QXX_OK;
}

void UART_SetBaudRate(uint32_t comIndex, uint32_t baudRate)
{
    (void)comIndex;
    (void)baudRate;
}

void UART_SetParity(uint32_t comIndex, uint32_t parity)
{
    (void)comIndex;
    (void)parity;
}

void UART_SetStopBits(uint32_t comIndex, uint32_t stopBits)
{
    (void)comIndex;
    (void)stopBits;
}

uint32_t FatFS_ReadFile(const char *pFilePath,
                        uint8_t *pBuffer,
                        uint32_t bufferSize)
{
    uint32_t length = s_FileLength;

    (void)pFilePath;
    if (pBuffer == NULL) {
        return 0u;
    }
    if (length > bufferSize) {
        length = bufferSize;
    }
    memcpy(pBuffer, s_FileBuffer, length);
    return length;
}

uint32_t FatFS_WriteFile(const char *pFilePath,
                         const uint8_t *pBuffer,
                         uint32_t bufferSize)
{
    (void)pFilePath;
    if ((pBuffer == NULL) || (bufferSize > MOCK_FLASH_SIZE)) {
        return 0u;
    }
    memcpy(s_FileBuffer, pBuffer, bufferSize);
    s_FileLength = bufferSize;
    return bufferSize;
}
