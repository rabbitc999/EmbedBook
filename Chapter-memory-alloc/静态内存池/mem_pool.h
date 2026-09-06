#ifndef __MEM_POOL_H
#define __MEM_POOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码 */
#define MEM_POOL_ERR_OK                0
#define MEM_POOL_ERR_NULL_POINTER     -1
#define MEM_POOL_ERR_BAD_ALIGNMENT    -2
#define MEM_POOL_ERR_BAD_BLOCK_SIZE   -3
#define MEM_POOL_ERR_TOO_MANY_BLOCKS  -4
#define MEM_POOL_ERR_NO_FREE_BLOCK    -5
#define MEM_POOL_ERR_INVALID_POINTER  -6

/* 最大块数，元数据放在 MemPool_T 内 */
#define MEM_POOL_MAX_BLOCK_COUNT       64
#define MEM_POOL_MIN_BLOCK_SIZE        4u
#define MEM_POOL_ALIGN_SIZE            4u

#define MEM_POOL_BLOCK_USED            1u
#define MEM_POOL_BLOCK_FREE            0u

/* 静态固定块内存池结构体 */
typedef struct __MemPool_T
{
    uint8_t *pBuffer;              /* 用户提供的连续内存起始地址 */
    uint16_t blockSize;            /* 每个块的大小（字节） */
    uint16_t blockCount;           /* 块数量 */
    uint16_t freeCount;            /* 当前剩余空闲块数 */
    uint8_t  used[MEM_POOL_MAX_BLOCK_COUNT]; /* 每个块的空闲/占用标记 */
} MemPool_T;

/* 初始化内存池 */
int8_t Mem_PoolCreate(MemPool_T *pPool, void *pBuffer,
                      uint16_t blockSize, uint16_t blockCount);

/* 从内存池申请一块内存，失败返回 NULL */
void *Mem_BlockAlloc(MemPool_T *pPool);

/* 释放由 Mem_BlockAlloc 返回的块 */
int8_t Mem_BlockFree(MemPool_T *pPool, void *pBlock);

/* 查询剩余空闲块数 */
uint16_t Mem_GetFreeCount(const MemPool_T *pPool);

#ifdef __cplusplus
}
#endif

#endif /* __MEM_POOL_H */
