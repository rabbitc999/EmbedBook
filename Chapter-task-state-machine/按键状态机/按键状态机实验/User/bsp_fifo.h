#ifndef __BSP_FIFO_H
#define __BSP_FIFO_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// FIFO操作状态码
#define FIFO_OK          0  // 操作成功
#define FIFO_ERROR_FULL  1  // FIFO已满错误
#define FIFO_ERROR_EMPTY 2  // FIFO为空错误
#define FIFO_ERROR_SIZE  3  // 大小错误（批量操作时空间不足）

// 是否支持原子操作头文件
#define FIFO_ATOMIC_SUPPORT 0

// 如果不支持原子操作，提供基本实现
#if defined(FIFO_ATOMIC_SUPPORT) && FIFO_ATOMIC_SUPPORT
#include <stdatomic.h>
#else
// 简单的原子操作模拟（非线程安全，仅用于单线程环境）
#define _Atomic volatile
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_load(ptr) (*(ptr))
#define atomic_compare_exchange_weak(ptr, expected, desired) \
    (*(ptr) == *(expected) ? (*(ptr) = (desired), 1) : (*(expected) = *(ptr), 0))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FIFO结构体定义
 * 
 * 使用循环缓冲区实现的线程安全FIFO，支持多生产者和多消费者
 */
typedef struct _FIFO_T
{
    _Atomic uint32_t Start;  // 起始位置指针（原子操作）
    _Atomic uint32_t End;    // 结束位置指针（原子操作）
    uint32_t Size;           // 缓冲区总大小
    uint8_t *Buffer;         // 数据缓冲区指针
} FIFO_T;

/**
 * @brief 初始化FIFO
 * @param fifo FIFO结构体指针
 * @param Buffer 数据缓冲区指针
 * @param Size 缓冲区大小
 */
void FIFO_Init(FIFO_T *fifo, uint8_t *Buffer, uint32_t Size);

/**
 * @brief 获取FIFO总大小
 * @param fifo FIFO结构体指针
 * @return FIFO总大小
 */
uint32_t FIFO_GetSize(FIFO_T *fifo);

/**
 * @brief 获取FIFO中元素数量
 * @param fifo FIFO结构体指针
 * @return 当前元素数量
 */
uint32_t FIFO_GetElementCount(FIFO_T *fifo);

/**
 * @brief 检查FIFO是否为空
 * @param fifo FIFO结构体指针
 * @return 1-空，0-非空
 */
uint32_t FIFO_IsEmpty(FIFO_T *fifo);

/**
 * @brief 获取FIFO剩余空间
 * @param fifo FIFO结构体指针
 * @return 剩余可存储元素数量
 */
uint32_t FIFO_GetRemain(FIFO_T *fifo);

/**
 * @brief 检查FIFO是否已满
 * @param fifo FIFO结构体指针
 * @return 1-满，0-未满
 */
uint32_t FIFO_IsFull(FIFO_T *fifo);

/**
 * @brief 向FIFO添加单个元素（支持多生产者）
 * @param fifo FIFO结构体指针
 * @param Element 要添加的元素
 * @return 操作状态码
 */
uint8_t FIFO_AddElement(FIFO_T *fifo, uint8_t Element);

/**
 * @brief 从FIFO获取单个元素（支持多消费者）
 * @param fifo FIFO结构体指针
 * @param Element 存储获取元素的指针
 * @return 操作状态码
 */
uint8_t FIFO_GetElement(FIFO_T *fifo, uint8_t *Element);

/**
 * @brief 向FIFO添加多个元素（批量操作）
 * @param fifo FIFO结构体指针
 * @param Element 要添加的元素数组指针
 * @param Count 要添加的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_AddBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count);

/**
 * @brief 从FIFO获取多个元素（批量操作）
 * @param fifo FIFO结构体指针
 * @param Element 存储获取元素的数组指针
 * @param Count 要获取的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_GetBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count);

/**
 * @brief 查看FIFO中的元素（不移除）
 * @param fifo FIFO结构体指针
 * @param Element 存储查看元素的指针
 * @return 操作状态码
 */
uint8_t FIFO_FindElement(FIFO_T *fifo, uint8_t *Element);

/**
 * @brief 查看FIFO中的多个元素（不移除）
 * @param fifo FIFO结构体指针
 * @param Element 存储查看元素的数组指针
 * @param Count 要查看的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_FindBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count);

/**
 * @brief 清空FIFO
 * @param fifo FIFO结构体指针
 */
void FIFO_Clear(FIFO_T *fifo);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_FIFO_H */
