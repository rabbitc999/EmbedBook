/**
 * @file    device.c
 * @brief   用函数指针实现的“设备对象基类”
 * @details 调用方只依赖 Device_T 的三个操作，具体实现由子类填入。
 */

#include "device.h"

void Device_Init(Device_T *pDevice)
{
    if ((pDevice != NULL) && (pDevice->pfnInit != NULL)) {
        pDevice->pfnInit(pDevice);
    }
}

void Device_Read(Device_T *pDevice, uint8_t *pBuffer, uint16_t length)
{
    if ((pDevice != NULL) && (pDevice->pfnRead != NULL)) {
        pDevice->pfnRead(pDevice, pBuffer, length);
    }
}

void Device_Write(Device_T *pDevice, const uint8_t *pBuffer, uint16_t length)
{
    if ((pDevice != NULL) && (pDevice->pfnWrite != NULL)) {
        pDevice->pfnWrite(pDevice, pBuffer, length);
    }
}
