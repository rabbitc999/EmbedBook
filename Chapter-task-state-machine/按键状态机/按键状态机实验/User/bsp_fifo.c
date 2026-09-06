#include "bsp_fifo.h"

/**
 * @brief 初始化FIFO
 * @param fifo FIFO结构体指针
 * @param Buffer 数据缓冲区指针
 * @param Size 缓冲区大小
 */
void FIFO_Init(FIFO_T *fifo, uint8_t *Buffer, uint32_t Size)
{
    fifo->Buffer = Buffer;
    atomic_store(&fifo->Start, 0);
    atomic_store(&fifo->End, 0);
    fifo->Size = Size;
}

/**
 * @brief 获取FIFO总大小
 * @param fifo FIFO结构体指针
 * @return FIFO总大小
 */
uint32_t FIFO_GetSize(FIFO_T *fifo)
{
    return fifo->Size;
}

/**
 * @brief 获取FIFO中元素数量
 * @param fifo FIFO结构体指针
 * @return 当前元素数量
 */
uint32_t FIFO_GetElementCount(FIFO_T *fifo)
{
    uint32_t start = atomic_load(&fifo->Start);
    uint32_t end = atomic_load(&fifo->End);
    // 计算循环缓冲区中的元素数量
    return (end - start + fifo->Size) % fifo->Size;
}

/**
 * @brief 检查FIFO是否为空
 * @param fifo FIFO结构体指针
 * @return 1-空，0-非空
 */
uint32_t FIFO_IsEmpty(FIFO_T *fifo)
{
    uint32_t start = atomic_load(&fifo->Start);
    uint32_t end = atomic_load(&fifo->End);
    return end == start;
}

/**
 * @brief 获取FIFO剩余空间
 * @param fifo FIFO结构体指针
 * @return 剩余可存储元素数量
 */
uint32_t FIFO_GetRemain(FIFO_T *fifo)
{
    // 保留一个位置用于区分空和满状态
    return fifo->Size - FIFO_GetElementCount(fifo) - 1;
}

/**
 * @brief 检查FIFO是否已满
 * @param fifo FIFO结构体指针
 * @return 1-满，0-未满
 */
uint32_t FIFO_IsFull(FIFO_T *fifo)
{
    return FIFO_GetRemain(fifo) == 0;
}

/**
 * @brief 使用CAS操作向FIFO添加单个元素（支持多生产者）
 * @param fifo FIFO结构体指针
 * @param Element 要添加的元素
 * @return 操作状态码
 */
uint8_t FIFO_AddElement(FIFO_T *fifo, uint8_t Element)
{
    uint32_t currentEnd, nextEnd, currentStart;
    
    do {
        // 原子加载当前结束位置和起始位置
        currentEnd = atomic_load(&fifo->End);
        currentStart = atomic_load(&fifo->Start);
        // 计算下一个结束位置（循环）
        nextEnd = (currentEnd + 1) % fifo->Size;
        
        // 检查队列是否已满（下一个结束位置等于起始位置）
        if (nextEnd == currentStart) {
            return FIFO_ERROR_FULL;
        }
        
        // 尝试CAS更新End指针
        // 如果成功，说明我们获得了这个位置的写入权
    } while (!atomic_compare_exchange_weak(&fifo->End, &currentEnd, nextEnd));
    
    // CAS成功，安全地写入数据
    fifo->Buffer[currentEnd] = Element;
    
    return FIFO_OK;
}

/**
 * @brief 使用CAS操作从FIFO获取单个元素（支持多消费者）
 * @param fifo FIFO结构体指针
 * @param Element 存储获取元素的指针
 * @return 操作状态码
 */
uint8_t FIFO_GetElement(FIFO_T *fifo, uint8_t *Element)
{
    uint32_t currentStart, nextStart, currentEnd;
    
    do {
        // 原子加载当前起始位置和结束位置
        currentStart = atomic_load(&fifo->Start);
        currentEnd = atomic_load(&fifo->End);
        
        // 检查队列是否为空
        if (currentStart == currentEnd) {
            return FIFO_ERROR_EMPTY;
        }
        
        // 读取数据
        *Element = fifo->Buffer[currentStart];
        // 计算下一个起始位置（循环）
        nextStart = (currentStart + 1) % fifo->Size;
        
        // 尝试CAS更新Start指针
        // 如果成功，说明我们成功获取了这个元素
    } while (!atomic_compare_exchange_weak(&fifo->Start, &currentStart, nextStart));
    
    return FIFO_OK;
}

/**
 * @brief 使用CAS操作向FIFO添加多个元素（批量操作）
 * @param fifo FIFO结构体指针
 * @param Element 要添加的元素数组指针
 * @param Count 要添加的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_AddBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count)
{
    // 如果数量为0，直接返回成功
    if (Count == 0) { return FIFO_OK; }
    
    uint32_t currentEnd, currentStart;
    uint32_t requiredEnd;
    
    do {
        // 原子加载当前结束位置和起始位置
        currentEnd = atomic_load(&fifo->End);
        currentStart = atomic_load(&fifo->Start);
        
        // 计算需要的结束位置
        requiredEnd = (currentEnd + Count) % fifo->Size;
        
        // 检查是否有足够空间
        // 使用模运算处理循环边界
        if ((currentStart - requiredEnd - 1 + fifo->Size * 2) % fifo->Size < Count) {
            return FIFO_ERROR_SIZE;
        }
        
        // 尝试CAS更新End指针
    } while (!atomic_compare_exchange_weak(&fifo->End, &currentEnd, requiredEnd));
    
    // CAS成功，批量写入数据
    for (uint32_t i = 0; i < Count; i++) {
        fifo->Buffer[(currentEnd + i) % fifo->Size] = Element[i];
    }
    
    return FIFO_OK;
}

/**
 * @brief 使用CAS操作从FIFO获取多个元素（批量操作）
 * @param fifo FIFO结构体指针
 * @param Element 存储获取元素的数组指针
 * @param Count 要获取的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_GetBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count)
{
    // 如果数量为0，直接返回成功
    if (Count == 0) { return FIFO_OK; }
    
    uint32_t currentStart, currentEnd;
    uint32_t requiredStart;
    
    do {
        // 原子加载当前起始位置和结束位置
        currentStart = atomic_load(&fifo->Start);
        currentEnd = atomic_load(&fifo->End);
        
        // 计算可用元素数量
        uint32_t available = (currentEnd - currentStart + fifo->Size) % fifo->Size;
        if (available < Count) {
            return FIFO_ERROR_SIZE;
        }
        
        // 批量读取数据
        for (uint32_t i = 0; i < Count; i++) {
            Element[i] = fifo->Buffer[(currentStart + i) % fifo->Size];
        }
        
        // 计算新的起始位置
        requiredStart = (currentStart + Count) % fifo->Size;
        
        // 尝试CAS更新Start指针
    } while (!atomic_compare_exchange_weak(&fifo->Start, &currentStart, requiredStart));
    
    return FIFO_OK;
}

/**
 * @brief 查看FIFO中的元素（不移除，不需要CAS）
 * @param fifo FIFO结构体指针
 * @param Element 存储查看元素的指针
 * @return 操作状态码
 */
uint8_t FIFO_FindElement(FIFO_T *fifo, uint8_t *Element)
{
    // 非原子操作，仅用于查看
    uint32_t start = atomic_load(&fifo->Start);
    uint32_t end = atomic_load(&fifo->End);
    
    // 检查是否为空
    if (start == end) {
        return FIFO_ERROR_EMPTY;
    }
    
    // 读取第一个元素但不移动指针
    *Element = fifo->Buffer[start];
    return FIFO_OK;
}

/**
 * @brief 查看FIFO中的多个元素（不移除，不需要CAS）
 * @param fifo FIFO结构体指针
 * @param Element 存储查看元素的数组指针
 * @param Count 要查看的元素数量
 * @return 操作状态码
 */
uint8_t FIFO_FindBuffer(FIFO_T *fifo, uint8_t *Element, uint8_t Count)
{
    // 如果数量为0，直接返回成功
    if (Count == 0) { return FIFO_OK; }
    
    uint32_t start = atomic_load(&fifo->Start);
    uint32_t end = atomic_load(&fifo->End);
    // 计算可用元素数量
    uint32_t available = (end - start + fifo->Size) % fifo->Size;
    
    // 检查是否有足够元素
    if (available < Count) {
        return FIFO_ERROR_SIZE;
    }
    
    // 遍历读取元素但不移动指针
    uint32_t ptr = start;
    for (uint32_t i = 0; i < Count; i++) {
        Element[i] = fifo->Buffer[ptr];
        ptr = (ptr + 1) % fifo->Size;
    }
    
    return FIFO_OK;
}

/**
 * @brief 清空FIFO
 * @param fifo FIFO结构体指针
 */
void FIFO_Clear(FIFO_T *fifo)
{
    // 原子操作重置指针
    atomic_store(&fifo->Start, 0);
    atomic_store(&fifo->End, 0);
}
