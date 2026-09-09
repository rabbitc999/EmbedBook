#ifndef __BSP_SYSTEM_H
#define __BSP_SYSTEM_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* PC 测试桩：把调试宏映射为 printf */
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)

/* store.c 使用到的字符串拷贝函数 */
void String_Copy(char *pDst, const char *pSrc, uint32_t maxLen);

#endif /* __BSP_SYSTEM_H */
