/**
 * @file    mem_pool_test.c
 * @brief   固定块内存池自动化测试
 * @details 覆盖初始化参数校验、整池分配、越界返回 NULL、
 *          对齐检查、数据保持、释放后复用、重复释放与非法指针拦截。
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mem_pool.h"

/* 测试缓冲区：union 保证 4 字节对齐 */
static union __TestBuffer_T
{
    uint8_t bytes[64u * 64u];
    uint32_t alignWord;
} s_Buffer;

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

static void Test_PoolCreateValidate(void)
{
    MemPool_T pool;
    int8_t ret = MEM_POOL_ERR_OK;

    /* 空指针 */
    ret = Mem_PoolCreate(NULL, s_Buffer.bytes, 32u, 8u);
    Check(ret == MEM_POOL_ERR_NULL_POINTER, "创建：池句柄为空");

    ret = Mem_PoolCreate(&pool, NULL, 32u, 8u);
    Check(ret == MEM_POOL_ERR_NULL_POINTER, "创建：缓冲区为空");

    /* 未对齐 */
    ret = Mem_PoolCreate(&pool, (uint8_t *)s_Buffer.bytes + 1u, 32u, 8u);
    Check(ret == MEM_POOL_ERR_BAD_ALIGNMENT, "创建：缓冲区未 4 字节对齐");

    /* 块大小不合法 */
    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 2u, 8u);
    Check(ret == MEM_POOL_ERR_BAD_BLOCK_SIZE, "创建：块大小小于 4");

    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 6u, 8u);
    Check(ret == MEM_POOL_ERR_BAD_BLOCK_SIZE, "创建：块大小不是 4 的倍数");

    /* 块数超限 */
    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 32u,
                         MEM_POOL_MAX_BLOCK_COUNT + 1u);
    Check(ret == MEM_POOL_ERR_TOO_MANY_BLOCKS, "创建：块数量超限");

    /* 正常创建 */
    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 32u, 16u);
    Check(ret == MEM_POOL_ERR_OK, "创建：合法参数");
    Check(Mem_GetFreeCount(&pool) == 16u, "创建：初始空闲块为 16");
}

static void Test_AllocAllThenFull(void)
{
    MemPool_T pool;
    void *blocks[16];
    uint16_t index = 0;
    int8_t ret = MEM_POOL_ERR_OK;

    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 32u, 16u);
    Check(ret == MEM_POOL_ERR_OK, "整池测试：初始化");

    for (index = 0; index < 16u; index++) {
        blocks[index] = Mem_BlockAlloc(&pool);
        Check(blocks[index] != NULL, "整池测试：逐块申请成功");
        if (blocks[index] == NULL) {
            return;
        }
        Check(((size_t)blocks[index] & 3u) == 0u, "整池测试：返回地址已对齐");
        memset(blocks[index], (int)(index + 1), 32u);
    }

    Check(Mem_GetFreeCount(&pool) == 0u, "整池测试：空闲块为 0");
    Check(Mem_BlockAlloc(&pool) == NULL, "整池测试：池满返回 NULL");

    /* 验证数据没有被后续分配覆盖 */
    for (index = 0; index < 16u; index++) {
        uint8_t *p = (uint8_t *)blocks[index];
        uint8_t expect = (uint8_t)(index + 1);
        uint16_t byteIndex = 0;
        int same = 1;

        for (byteIndex = 0; byteIndex < 32u; byteIndex++) {
            if (p[byteIndex] != expect) {
                same = 0;
                break;
            }
        }
        if (!same) {
            Check(0, "整池测试：块数据保持");
            return;
        }
    }
    Check(1, "整池测试：块数据保持");

    for (index = 0; index < 16u; index++) {
        ret = Mem_BlockFree(&pool, blocks[index]);
        if (ret != MEM_POOL_ERR_OK) {
            Check(0, "整池测试：释放全部块");
            return;
        }
    }
    Check(Mem_GetFreeCount(&pool) == 16u, "整池测试：释放后空闲块恢复 16");
}

static void Test_FreeReuse(void)
{
    MemPool_T pool;
    void *p1 = NULL;
    void *p2 = NULL;
    void *p3 = NULL;
    void *pAgain = NULL;
    int8_t ret = MEM_POOL_ERR_OK;

    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 64u, 4u);
    Check(ret == MEM_POOL_ERR_OK, "复用测试：初始化");

    p1 = Mem_BlockAlloc(&pool);
    p2 = Mem_BlockAlloc(&pool);
    p3 = Mem_BlockAlloc(&pool);
    Check((p1 != NULL) && (p2 != NULL) && (p3 != NULL), "复用测试：申请三块");

    /* 释放中间块后再次申请，应能复用 */
    ret = Mem_BlockFree(&pool, p2);
    Check(ret == MEM_POOL_ERR_OK, "复用测试：释放中间块");

    pAgain = Mem_BlockAlloc(&pool);
    Check(pAgain == p2, "复用测试：释放的块被再次复用");

    /* 双指针指向同一块时会重复释放：第二次应失败 */
    ret = Mem_BlockFree(&pool, pAgain);
    Check(ret == MEM_POOL_ERR_OK, "复用测试：第一次释放有效");
    ret = Mem_BlockFree(&pool, pAgain);
    Check(ret == MEM_POOL_ERR_INVALID_POINTER, "复用测试：重复释放被拒绝");

    ret = Mem_BlockFree(&pool, p1);
    Check(ret == MEM_POOL_ERR_OK, "复用测试：释放 p1");
    ret = Mem_BlockFree(&pool, p3);
    Check(ret == MEM_POOL_ERR_OK, "复用测试：释放 p3");
    Check(Mem_GetFreeCount(&pool) == 4u, "复用测试：空闲块恢复 4");
}

static void Test_InvalidFree(void)
{
    MemPool_T pool;
    uint8_t outside = 0;
    void *p = NULL;
    int8_t ret = MEM_POOL_ERR_OK;

    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 32u, 8u);
    Check(ret == MEM_POOL_ERR_OK, "非法释放测试：初始化");

    p = Mem_BlockAlloc(&pool);
    Check(p != NULL, "非法释放测试：申请一块");

    /* 完全不属于本池的指针 */
    ret = Mem_BlockFree(&pool, &outside);
    Check(ret == MEM_POOL_ERR_INVALID_POINTER, "非法释放测试：池外指针");

    /* 落在池内但不是块起始地址 */
    ret = Mem_BlockFree(&pool, (uint8_t *)p + 1u);
    Check(ret == MEM_POOL_ERR_INVALID_POINTER, "非法释放测试：块内非起始地址");

    /* 空指针 */
    ret = Mem_BlockFree(&pool, NULL);
    Check(ret == MEM_POOL_ERR_NULL_POINTER, "非法释放测试：空指针");

    ret = Mem_BlockFree(&pool, p);
    Check(ret == MEM_POOL_ERR_OK, "非法释放测试：正常释放");
}

static void Test_BoundarySingleBlock(void)
{
    MemPool_T pool;
    void *p = NULL;
    int8_t ret = MEM_POOL_ERR_OK;

    /* 只有一个块时也能正常工作 */
    ret = Mem_PoolCreate(&pool, s_Buffer.bytes, 32u, 1u);
    Check(ret == MEM_POOL_ERR_OK, "单块池：初始化");

    p = Mem_BlockAlloc(&pool);
    Check(p != NULL, "单块池：申请成功");
    Check(Mem_BlockAlloc(&pool) == NULL, "单块池：第二次申请返回 NULL");

    ret = Mem_BlockFree(&pool, p);
    Check(ret == MEM_POOL_ERR_OK, "单块池：释放成功");
    Check(Mem_GetFreeCount(&pool) == 1u, "单块池：空闲块恢复 1");
}

int main(void)
{
    Test_PoolCreateValidate();
    Test_AllocAllThenFull();
    Test_FreeReuse();
    Test_InvalidFree();
    Test_BoundarySingleBlock();

    if (s_FailCount == 0) {
        printf("\n所有内存池测试通过\n");
        return 0;
    }
    printf("\n测试失败 %d 项\n", s_FailCount);
    return 1;
}
