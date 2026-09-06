#ifndef __BSP_MEMORY_H
#define __BSP_MEMORY_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagMemorySource_T
{
    MP_ROM_IN,
} MemorySource_T;

void Memory_Init(void);
void *Memory_Malloc(MemorySource_T select, size_t size);
void Memory_Free(MemorySource_T select, void *ptr);
void *Memory_Realloc(MemorySource_T select, void *ptr, size_t newSize);
void Memory_GetInfo(MemorySource_T select, float *total, float *used);

char *String_MallocCopy(MemorySource_T select, const char *src, uint32_t max_len);
uint32_t String_Copy(char *dst, const char *src, uint32_t max_len);
uint32_t String_GetLength(const char *str, uint32_t max_len);
uint32_t String_IsEqual(const char *str1, const char *str2, uint32_t max_len);
int32_t String_Compare(const char *str1, const char *str2, uint32_t max_len);
int32_t String_CompareWithoutCase(const char *str1, const char *str2, uint32_t max_len);
uint32_t String_Concat(char *dst, const char *src, uint32_t max_len);
uint32_t String_IsSubstring(const char *parent, const char *sub, uint32_t parent_max_len, uint32_t sub_max_len);
uint32_t String_FindSubstring(const char *parent, const char *sub, uint32_t parent_max_len, uint32_t sub_max_len);
uint32_t String_Trim(char *str, uint32_t max_len);
void String_ToUpperCase(char *str, uint32_t max_len);
void String_ToLowerCase(char *str, uint32_t max_len);
uint32_t String_HashKey(const char *str, uint32_t max_len);
uint32_t String_HashKeyWithoutCase(const char *str, uint32_t max_len);

void Memory_Copy(void *dst, const void *src, size_t size);
void Memory_Set(void *dst, uint8_t value, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_MEMORY_H */