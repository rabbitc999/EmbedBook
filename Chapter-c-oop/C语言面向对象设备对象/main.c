#include <stdio.h>
#include <string.h>
#include "device.h"
#include "timer.h"

static int s_FailCount = 0;

static void Check(int ok, const char *pName)
{
    if (ok) {
        printf("[PASS] %s\n", pName);
    } else {
        printf("[FAIL] %s\n", pName);
        s_FailCount++;
    }
}

/* ---------- LED 设备 ---------- */
typedef struct __LedPriv_T
{
    uint8_t state;      /* 0=off, 1=on */
    uint8_t initCount;
} LedPriv_T;

static void Led_Init(Device_T *pSelf)
{
    LedPriv_T *pPriv = (LedPriv_T *)pSelf->pPriv;
    pPriv->state = 0u;
    pPriv->initCount++;
}

static void Led_Read(Device_T *pSelf, uint8_t *pBuffer, uint16_t length)
{
    LedPriv_T *pPriv = (LedPriv_T *)pSelf->pPriv;
    if ((length > 0u) && (pBuffer != NULL)) {
        pBuffer[0] = pPriv->state;
    }
}

static void Led_Write(Device_T *pSelf, const uint8_t *pBuffer, uint16_t length)
{
    LedPriv_T *pPriv = (LedPriv_T *)pSelf->pPriv;
    if ((length > 0u) && (pBuffer != NULL)) {
        pPriv->state = (pBuffer[0] != 0u) ? 1u : 0u;
    }
}

static void LedDevice_Create(Device_T *pDevice, LedPriv_T *pPriv)
{
    pDevice->pfnInit = Led_Init;
    pDevice->pfnRead = Led_Read;
    pDevice->pfnWrite = Led_Write;
    pDevice->pPriv = pPriv;
}

/* ---------- UART 设备 ---------- */
typedef struct __UartPriv_T
{
    uint8_t lastByte;
    uint16_t writeCount;
} UartPriv_T;

static void Uart_Init(Device_T *pSelf)
{
    UartPriv_T *pPriv = (UartPriv_T *)pSelf->pPriv;
    pPriv->lastByte = 0u;
    pPriv->writeCount = 0u;
}

static void Uart_Read(Device_T *pSelf, uint8_t *pBuffer, uint16_t length)
{
    UartPriv_T *pPriv = (UartPriv_T *)pSelf->pPriv;
    if ((length > 0u) && (pBuffer != NULL)) {
        pBuffer[0] = pPriv->lastByte;
    }
}

static void Uart_Write(Device_T *pSelf, const uint8_t *pBuffer, uint16_t length)
{
    UartPriv_T *pPriv = (UartPriv_T *)pSelf->pPriv;
    if ((length > 0u) && (pBuffer != NULL)) {
        pPriv->lastByte = pBuffer[length - 1u];
        pPriv->writeCount += length;
    }
}

static void UartDevice_Create(Device_T *pDevice, UartPriv_T *pPriv)
{
    pDevice->pfnInit = Uart_Init;
    pDevice->pfnRead = Uart_Read;
    pDevice->pfnWrite = Uart_Write;
    pDevice->pPriv = pPriv;
}

/* 应用层：只依赖 Device_T，不关心具体设备 */
static void App_TransferData(Device_T *pDevice,
                             const uint8_t *pData,
                             uint16_t length)
{
    Device_Write(pDevice, pData, length);
}

static void Test_Device(void)
{
    LedPriv_T ledPriv;
    UartPriv_T uartPriv;
    Device_T ledDevice;
    Device_T uartDevice;
    uint8_t data[4] = { 1u, 2u, 3u, 0u };
    uint8_t value = 0u;

    LedDevice_Create(&ledDevice, &ledPriv);
    UartDevice_Create(&uartDevice, &uartPriv);

    Device_Init(&ledDevice);
    Device_Init(&uartDevice);

    App_TransferData(&ledDevice, data, 1u);
    Device_Read(&ledDevice, &value, 1u);
    Check(value == 1u, "设备对象：LED 通过统一接口被点亮");

    App_TransferData(&uartDevice, data, sizeof(data));
    Check(uartPriv.writeCount == 4u, "设备对象：UART 统一接口写入 4 字节");

    App_TransferData(&ledDevice, data + 3u, 1u);
    Device_Read(&ledDevice, &value, 1u);
    Check(value == 0u, "设备对象：LED 通过统一接口被关闭");
}

static void Test_Timer(void)
{
    Timer_T *pTimer = Timer_Create();

    Timer_SetPeriod(pTimer, 500u);
    Check(Timer_IsExpired(pTimer) == 0u, "定时器对象：初始未超时");

    Timer_Tick(pTimer, 300u);
    Check(Timer_IsExpired(pTimer) == 0u, "定时器对象：300ms 未超时");

    Timer_Tick(pTimer, 200u);
    Check(Timer_IsExpired(pTimer) == 1u, "定时器对象：500ms 超时");
    Check(Timer_IsExpired(pTimer) == 0u, "定时器对象：标志查询后自动清除");
}

int main(void)
{
    Test_Device();
    Test_Timer();

    if (s_FailCount == 0) {
        printf("\n第5章代码示例测试全部通过\n");
        return 0;
    }
    printf("\n第5章代码示例测试失败 %d 项\n", s_FailCount);
    return 1;
}
