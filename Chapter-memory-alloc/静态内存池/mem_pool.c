/**
 * @file    mem_pool.c
 * @brief   单片机静态固定块内存池实现
 * @details 把一整块连续内存切成大小相同的块，用 used 数组记录占用情况。
 *          固定块池不会产生外部碎片，适合管理同尺寸对象。
 */

#include <stddef.h>
#include "mem_pool.h"

/**
 * @brief  判断地址是否按 MEM_POOL_ALIGN_SIZE 对齐
 * @param  pAddr 待判断地址
 * @retval 1-已对齐，0-未对齐
 */
static uint8_t Mem_IsAligned(const void *pAddr)
{
    return (((size_t)pAddr) & (MEM_POOL_ALIGN_SIZE - 1u)) == 0u;
}

/**
 * @brief  创建固定块内存池
 * @param  pPool      内存池句柄
 * @param  pBuffer    用户提供的连续内存，需要按 4 字节对齐
 * @param  blockSize  每块大小，必须为 4 的倍数且不小于 4
 * @param  blockCount 块数量
 * @retval 错误码
 */
int8_t Mem_PoolCreate(MemPool_T *pPool, void *pBuffer,
                      uint16_t blockSize, uint16_t blockCount)
{
    uint16_t index = 0;

    if ((pPool == NULL) || (pBuffer == NULL)) {
        return MEM_POOL_ERR_NULL_POINTER;
    }
    if (!Mem_IsAligned(pBuffer)) {
        return MEM_POOL_ERR_BAD_ALIGNMENT;
    }
    if ((blockSize < MEM_POOL_MIN_BLOCK_SIZE) ||
        ((blockSize & (MEM_POOL_ALIGN_SIZE - 1u)) != 0u)) {
        return MEM_POOL_ERR_BAD_BLOCK_SIZE;
    }
    if (blockCount > MEM_POOL_MAX_BLOCK_COUNT) {
        return MEM_POOL_ERR_TOO_MANY_BLOCKS;
    }

    pPool->pBuffer    = (uint8_t *)pBuffer;
    pPool->blockSize  = blockSize;
    pPool->blockCount = blockCount;
    pPool->freeCount  = blockCount;

    for (index = 0; index < blockCount; index++) {
        pPool->used[index] = MEM_POOL_BLOCK_FREE;
    }
    return MEM_POOL_ERR_OK;
}

/**
 * @brief  从内存池申请一块内存
 * @param  pPool 内存池句柄
 * @retval 块地址；池满时返回 NULL
 */
void *Mem_BlockAlloc(MemPool_T *pPool)
{
    uint16_t index = 0;
    uint8_t *pBlock = NULL;

    if (pPool == NULL) {
        return NULL;
    }
    if (pPool->freeCount == 0u) {
        return NULL;
    }

    for (index = 0; index < pPool->blockCount; index++) {
        if (pPool->used[index] == MEM_POOL_BLOCK_FREE) {
            pPool->used[index] = MEM_POOL_BLOCK_USED;
            pPool->freeCount--;
            pBlock = pPool->pBuffer + (size_t)index * pPool->blockSize;
            return (void *)pBlock;
        }
    }
    return NULL;
}

/**
 * @brief  释放由 Mem_BlockAlloc 返回的块
 * @param  pPool  内存池句柄
 * @param  pBlock 需要释放的块地址
 * @retval 错误码
 */
int8_t Mem_BlockFree(MemPool_T *pPool, void *pBlock)
{
    size_t offset = 0;
    uint16_t blockIndex = 0;
    uint8_t *pStart = NULL;

    if ((pPool == NULL) || (pBlock == NULL)) {
        return MEM_POOL_ERR_NULL_POINTER;
    }

    pStart = (uint8_t *)pPool->pBuffer;
    if (((uint8_t *)pBlock < pStart) ||
        ((uint8_t *)pBlock >= pStart + (size_t)pPool->blockCount * pPool->blockSize)) {
        return MEM_POOL_ERR_INVALID_POINTER;
    }

    offset = (size_t)((uint8_t *)pBlock - pStart);
    if ((offset % pPool->blockSize) != 0u) {
        return MEM_POOL_ERR_INVALID_POINTER;
    }

    blockIndex = (uint16_t)(offset / pPool->blockSize);
    if (pPool->used[blockIndex] != MEM_POOL_BLOCK_USED) {
        return MEM_POOL_ERR_INVALID_POINTER;
    }

    pPool->used[blockIndex] = MEM_POOL_BLOCK_FREE;
    pPool->freeCount++;
    return MEM_POOL_ERR_OK;
}

/**
 * @brief  查询剩余空闲块数
 * @param  pPool 内存池句柄
 * @retval 空闲块数量
 */
uint16_t Mem_GetFreeCount(const MemPool_T *pPool)
{
    if (pPool == NULL) {
        return 0u;
    }
    return pPool->freeCount;
}
