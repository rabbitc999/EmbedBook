#ifndef __DEVICE_H
#define __DEVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct __Device_T Device_T;

/* 设备操作函数指针（相当于虚函数表） */
typedef void (*DeviceInit_F)(Device_T *pSelf);
typedef void (*DeviceRead_F)(Device_T *pSelf,
                             uint8_t *pBuffer,
                             uint16_t length);
typedef void (*DeviceWrite_F)(Device_T *pSelf,
                              const uint8_t *pBuffer,
                              uint16_t length);

/* 设备“类” */
typedef struct __Device_T
{
    DeviceInit_F pfnInit;
    DeviceRead_F pfnRead;
    DeviceWrite_F pfnWrite;
    void *pPriv;                 /* 私有数据 */
} Device_T;

void Device_Init(Device_T *pDevice);
void Device_Read(Device_T *pDevice, uint8_t *pBuffer, uint16_t length);
void Device_Write(Device_T *pDevice, const uint8_t *pBuffer, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICE_H */
