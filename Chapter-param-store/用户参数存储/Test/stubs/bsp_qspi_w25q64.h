#ifndef __BSP_QSPI_W25Q64_H
#define __BSP_QSPI_W25Q64_H

#include <stdint.h>

/* 参数在 QSPI Flash 中的地址 */
#define QSPI_ADDR_PARAM 0x080000u

typedef enum __W25QXXStatus_T
{
    W25QXX_OK = 0,
    W25QXX_ERR = -1
} W25QXXStatus_T;

/* PC 测试桩实现位于 store_port_mock.c */
int W25QXX_ReadBuffer(uint8_t *pBuf, uint32_t addr, uint32_t len);
int W25QXX_WriteBuffer(const uint8_t *pBuf, uint32_t addr, uint32_t len);

#endif /* __BSP_QSPI_W25Q64_H */
