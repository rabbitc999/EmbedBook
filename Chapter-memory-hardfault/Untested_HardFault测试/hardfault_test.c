/**
 * @file    hardfault_test.c
 * @brief   人为制造 HardFault 的测试用例
 * @details 用于配合 SEGGER HardFault Handler 观察故障寄存器：
 *          1. 调用非法函数指针（PC 跳到非法地址）
 *          2. 非对齐内存访问（UNALIGNED）
 *          3. 使用非法地址指针（BusFault / BFAR）
 *
 * @warning 这些测试会故意让 MCU 进入 HardFault，只能在调试阶段单条执行；
 *          不要在正式固件中直接调用。
 */

#include <stdint.h>
#include <stddef.h>

/* 测试 1：调用非法函数指针 */
static void HardFault_TestBadFunctionPointer(void)
{
    typedef void (*BadFunction_T)(void);
    volatile BadFunction_T pBadFunction = (BadFunction_T)0xDEADBEEFu;

    pBadFunction();      /* PC 会跳转到 0xDEADBEEF，触发取指错误/UsageFault */
}

/* 测试 2：非对齐内存访问 */
static void HardFault_TestUnalignedAccess(void)
{
    uint8_t buffer[8];
    uint32_t *pUnaligned = NULL;

    buffer[0] = 0u;

    /* 让 32 位指针指向非 4 字节对齐地址 */
    pUnaligned = (uint32_t *)(void *)(buffer + 1u);
    *pUnaligned = 0x12345678u;  /* UNALIGNED 位会置位 */
}

/* 测试 3：非法地址指针 */
static void HardFault_TestInvalidAddress(void)
{
    /* 0xA0000000 不属于 STM32H743 有效地址映射 */
    volatile uint32_t *pInvalid = (volatile uint32_t *)0xA0000000u;

    *pInvalid = 0x12345678u;    /* BusFault，BFAR 指向 0xA0000000 */
}

/**
 * @brief  运行指定的 HardFault 测试用例
 * @param  caseIndex 1-非法函数指针；2-非对齐访问；3-非法地址
 * @retval 无
 * @note   本函数不会返回，MCU 会停在 HardFault_Handler
 */
void HardFault_TestRun(uint8_t caseIndex)
{
    switch (caseIndex) {
        case 1u:
            HardFault_TestBadFunctionPointer();
            break;
        case 2u:
            HardFault_TestUnalignedAccess();
            break;
        case 3u:
            HardFault_TestInvalidAddress();
            break;
        default:
            break;
    }
}
