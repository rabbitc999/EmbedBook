#include <stdio.h>
#include <stdint.h>
#include "mem_pool.h"

/* 演示缓冲区：联合体保证 4 字节对齐 */
static union __AlignBuffer_T
{
    uint8_t bytes[16 * 32];
    uint32_t alignWord;
} s_Buffer;

typedef struct __Point_T
{
    int16_t x;
    int16_t y;
} Point_T;

int main(void)
{
    MemPool_T pool;
    Point_T *p1 = NULL;
    Point_T *p2 = NULL;
    Point_T *p3 = NULL;

    /* 16 个块，每块 32 字节，可以容纳 Point_T */
    if (Mem_PoolCreate(&pool, s_Buffer.bytes, 32u, 16u) != MEM_POOL_ERR_OK) {
        printf("内存池初始化失败\n");
        return -1;
    }

    p1 = (Point_T *)Mem_BlockAlloc(&pool);
    p2 = (Point_T *)Mem_BlockAlloc(&pool);
    p3 = (Point_T *)Mem_BlockAlloc(&pool);

    if ((p1 == NULL) || (p2 == NULL) || (p3 == NULL)) {
        printf("申请内存失败\n");
        return -1;
    }

    p1->x = 10;
    p1->y = 20;
    p2->x = 30;
    p2->y = 40;
    p3->x = p1->x + p2->x;
    p3->y = p1->y + p2->y;

    printf("p1=(%d,%d) p2=(%d,%d) p3=(%d,%d)\n",
           p1->x, p1->y, p2->x, p2->y, p3->x, p3->y);
    printf("剩余空闲块: %u\n", Mem_GetFreeCount(&pool));

    if (Mem_BlockFree(&pool, p2) != MEM_POOL_ERR_OK) {
        printf("释放 p2 失败\n");
        return -1;
    }
    if (Mem_BlockFree(&pool, p1) != MEM_POOL_ERR_OK) {
        printf("释放 p1 失败\n");
        return -1;
    }
    printf("释放 p1/p2 后空闲块: %u\n", Mem_GetFreeCount(&pool));
    return 0;
}
