#include "bsp_memory.h"
#include "bsp_system.h"

#define MP_READONLY_BUFFER_SIZE (32 * 1024)                        /* MCU只读缓存大小 */
static uint32_t s_ReadOnlyPoolBuffer[MP_READONLY_BUFFER_SIZE / 4]; /* MCU只读缓存池 */
static uint32_t s_ReadOnlyPoolPtr = 0;

/**
 * @brief  初始化内存池
 */
void Memory_Init(void)
{
    s_ReadOnlyPoolPtr = 0;
}

/**
 * @brief  内存池分配内存
 * @param  select: 内存池选择
 * @param  size: 申请内存大小
 * @return 申请的内存指针
 */
void *Memory_Malloc(MemorySource_T select, size_t size)
{
    uint8_t *current;
    switch (select)
    {
    case MP_ROM_IN:
    {
        if (size == 0 || s_ReadOnlyPoolPtr > MP_READONLY_BUFFER_SIZE)
        {
            return NULL;
        }
        current = (uint8_t *)&s_ReadOnlyPoolBuffer[s_ReadOnlyPoolPtr];
        s_ReadOnlyPoolPtr += size / 4 + (size % 4 == 0 ? 0 : 1);
        return current;
    }
    default:
    {
        return NULL;
    }
    }
}

/**
 * @brief  内存池释放内存
 * @param  select: 内存池选择
 * @param  ptr: 释放的内存指针
 */
void Memory_Free(MemorySource_T select, void *ptr)
{
    void *pool = NULL;
    switch (select)
    {
    default:
    {
        return;
    }
    }
}

/**
 * @brief  内存池重新分配内存
 * @param  select: 内存池选择
 * @param  ptr: 需要重新分配的内存指针
 * @param  newSize: 新的内存大小
 * @return 重新分配的内存指针 (若分配失败则返回原指针)
 */
void *Memory_Realloc(MemorySource_T select, void *ptr, size_t newSize)
{
    void *newPtr = Memory_Malloc(select, newSize);
    if (newPtr != NULL)
    {
        Memory_Copy(newPtr, ptr, newSize);
        Memory_Free(select, ptr);
    }
    else
    {
        return ptr;
    }
    return newPtr;
}

/**
 * @brief  获取内存池信息
 * @param  select: 内存池选择
 * @param  total: 总内存  (KB)
 * @param  used: 已用内存  (KB)
 */
void Memory_GetInfo(MemorySource_T select, float *total, float *used)
{
    if (select == MP_ROM_IN)
    {
        if (total) *total = (float)MP_READONLY_BUFFER_SIZE / 1024.0f;
        if (used) *used = (float)s_ReadOnlyPoolPtr * 4 / 1024.0f;
    }
    else
    {
        if (total) *total = 0;
        if (used) *used = 0;
    }
}

/**
 * @brief 内存拷贝函数
 * @param dst 目标内存区域
 * @param src 源内存区域
 * @param size 拷贝大小
 */
void Memory_Copy(void *dst, const void *src, size_t size)
{
    uint8_t *pDst = (uint8_t *)dst;
    const uint8_t *pSrc = (const uint8_t *)src;

    /* 处理内存重叠：如果目标在源之后且有重叠，从后往前拷贝 */
    if (pDst > pSrc && pDst < pSrc + size)
    {
        pDst += size;
        pSrc += size;
        while (size--) { *--pDst = *--pSrc; }
    }
    else
    {
        while (size--) { *pDst++ = *pSrc++; }
    }
}

/**
 * @brief 内存设置函数
 * @param dst 目标内存区域
 * @param value 8位设置数值
 * @param size 设置大小
 */
void Memory_Set(void *dst, uint8_t value, size_t size)
{
    uint8_t *pDst = (uint8_t *)dst;
    while (size--) { *pDst++ = value; }
}

/**
 * @brief  分配并复制字符串（安全版）
 * @param  select: 内存池选择
 * @param  src: 源字符串指针
 * @param  max_len: 最大复制长度（防止越界访问）
 * @retval 新分配的字符串指针（需要调用者释放），失败返回NULL
 */
char *String_MallocCopy(MemorySource_T select, const char *src, uint32_t max_len)
{
    void *newPtr = (char *)Memory_Malloc(select, (String_GetLength(src, max_len) + 1) * sizeof(char));
    if (newPtr != NULL)
    {
        String_Copy(newPtr, src, max_len);
        return newPtr;
    }
    return NULL;
}

/**
 * @brief  安全字符串拷贝函数
 * @param  dst: 目标缓冲区指针
 * @param  src: 源字符串指针
 * @param  max_len: 最大拷贝长度（目标缓冲区总大小，包含结尾的'\0'）
 * @retval 实际拷贝的字符数（不包含结尾的'\0'）
 * @note   1. 总是确保目标字符串以'\0'结尾，防止缓冲区溢出
 *         2. 如果 max_len 为0，则不执行任何操作并返回0
 *         3. 如果 src 长度 ≥ max_len，则拷贝 max_len-1 个字符并添加'\0'
 */
uint32_t String_Copy(char *dst, const char *src, uint32_t max_len)
{
    /* 参数合法性检查 */
    if (dst == NULL || src == NULL || max_len == 0)
    {
        return 0;
    }

    uint32_t i = 0;

    /* 拷贝最多 max_len-1 个字符，确保留出空间给 '\0' */
    while (i < (max_len - 1) && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }

    /* 确保字符串以 '\0' 结尾 */
    dst[i] = '\0';

    return i; /* 返回实际拷贝的字符数量 */
}

/**
 * @brief  获取字符串长度（带最大长度限制）
 * @param  str: 字符串指针
 * @param  max_len: 最大搜索长度（防止越界访问）
 * @retval 字符串长度（不包含结尾的'\0'）
 * @note   1. 在 [0, max_len) 范围内查找 '\0'，防止访问非法内存
 *         2. 如果在 max_len 范围内未找到 '\0'，则返回 max_len
 *         3. 适用于不确定字符串是否以 '\0' 结尾的场景，如接收缓冲区
 */
uint32_t String_GetLength(const char *str, uint32_t max_len)
{
    /* 参数合法性检查 */
    if (str == NULL || max_len == 0)
    {
        return 0;
    }

    uint32_t length = 0;

    /* 在最大长度范围内查找字符串结束符 */
    while (length < max_len && str[length] != '\0')
    {
        length++;
    }

    return length; /* 返回字符串长度或 max_len */
}

/**
 * @brief 判断两个字符串是否相等
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param max_len 最大比较长度
 * @return 1-相等，0-不相等
 */
uint32_t String_IsEqual(const char *str1, const char *str2, uint32_t max_len)
{
    if (str1 == NULL || str2 == NULL || max_len == 0)
    {
        return 0;
    }

    uint32_t i = 0;

    /* 逐字符比较，直到遇到不同字符或任一字符串结束或达到max_len */
    while (i < max_len && str1[i] != '\0' && str2[i] != '\0')
    {
        if (str1[i] != str2[i])
        {
            return 0; /* 发现不同字符 */
        }
        i++;
    }

    /* 检查是否同时到达字符串结尾 */
    return (str1[i] == '\0' && str2[i] == '\0') ? 1 : 0;
}

/**
 * @brief 比较两个字符串（字典序）
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param max_len 最大比较长度
 * @return <0- str1<str2, 0-相等, >0- str1>str2
 */
int32_t String_Compare(const char *str1, const char *str2, uint32_t max_len)
{
    if (str1 == NULL && str2 == NULL)
    {
        return 0;
    }
    if (str1 == NULL)
    {
        return -1; /* NULL < 非NULL */
    }
    if (str2 == NULL)
    {
        return 1; /* 非NULL > NULL */
    }

    uint32_t i = 0;

    /* 逐字符比较，直到遇到不同字符或任一字符串结束或达到max_len */
    while (i < max_len && str1[i] != '\0' && str2[i] != '\0')
    {
        if (str1[i] != str2[i])
        {
            return (int32_t)str1[i] - (int32_t)str2[i];
        }
        i++;
    }

    /* 如果前面字符都相同，比较长度 */
    if (i >= max_len)
    {
        return 0; /* 在max_len范围内完全相同 */
    }

    return (int32_t)str1[i] - (int32_t)str2[i];
}

/**
 * @brief  字符串转换为大写（原地修改）
 * @param  str: 待转换的字符串指针
 * @param  max_len: 最大转换长度（防止越界访问）
 */
void String_ToUpperCase(char *str, uint32_t max_len)
{
    uint32_t i = 0;

    while (i < max_len && str[i] != '\0')
    {
        if (str[i] >= 'a' && str[i] <= 'z')
        {
            str[i] = str[i] - 32;
        }
        i++;
    }
}

/**
 * @brief  字符串转换为小写（原地修改）
 * @param  str: 待转换的字符串指针
 * @param  max_len: 最大转换长度（防止越界访问）
 */
void String_ToLowerCase(char *str, uint32_t max_len)
{
    uint32_t i = 0;

    while (i < max_len && str[i] != '\0')
    {
        if (str[i] >= 'A' && str[i] <= 'Z')
        {
            str[i] = str[i] + 32;
        }
        i++;
    }
}

/**
 * @brief  字符转换为小写（辅助函数）
 * @param  ch: 待转换的字符
 * @return 转换后的字符
 */
static char Tolower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

/**
 * @brief 比较两个字符串（字典序，不区分大小写）
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param max_len 最大比较长度
 * @return <0- str1<str2, 0-相等, >0- str1>str2
 */
int32_t String_CompareWithoutCase(const char *str1, const char *str2, uint32_t max_len)
{
    if (str1 == NULL && str2 == NULL)
    {
        return 0;
    }
    if (str1 == NULL)
    {
        return -1; /* NULL < 非NULL */
    }
    if (str2 == NULL)
    {
        return 1; /* 非NULL > NULL */
    }

    uint32_t i = 0;

    /* 逐字符比较，直到遇到不同字符或任一字符串结束或达到max_len */
    while (i < max_len && str1[i] != '\0' && str2[i] != '\0')
    {
        if (str1[i] != str2[i])
        {
            return (int32_t)Tolower(str1[i]) - (int32_t)Tolower(str2[i]);
        }
        i++;
    }

    /* 如果前面字符都相同，比较长度 */
    if (i >= max_len)
    {
        return 0; /* 在max_len范围内完全相同 */
    }

    return (int32_t)Tolower(str1[i]) - (int32_t)Tolower(str2[i]);
}

/**
 * @brief 字符串连接（安全版）
 * @param dst 目标字符串缓冲区
 * @param src 源字符串
 * @param max_len 目标缓冲区总大小（包含结尾的'\0'）
 * @return 连接后的字符串长度（不包含结尾的'\0'）
 */
uint32_t String_Concat(char *dst, const char *src, uint32_t max_len)
{
    if (dst == NULL || src == NULL || max_len == 0)
    {
        return 0;
    }

    /* 查找dst当前长度（确保不超过max_len） */
    uint32_t dst_len = String_GetLength(dst, max_len);

    /* 检查是否还有空间连接src */
    if (dst_len >= max_len - 1)
    {
        return dst_len; /* 无剩余空间 */
    }

    uint32_t i = 0;
    uint32_t available = max_len - dst_len - 1; /* 剩余可用空间 */

    /* 从dst末尾开始拷贝src */
    while (i < available && src[i] != '\0')
    {
        dst[dst_len + i] = src[i];
        i++;
    }

    /* 确保字符串以'\0'结尾 */
    dst[dst_len + i] = '\0';

    return dst_len + i; /* 返回连接后的总长度 */
}

/**
 * @brief  KMP算法前缀表计算函数（Partial Match Table）
 * @param  pattern: 模式串（子串）
 * @param  pattern_len: 模式串长度
 * @param  lps: 输出的前缀表数组
 * @retval 无
 * @note   时间复杂度O(m)，m为模式串长度
 */
static void KMP_ComputeLPS(const char *pattern, uint32_t pattern_len, uint32_t *lps)
{
    if (pattern == NULL || lps == NULL || pattern_len == 0)
    {
        return;
    }

    uint32_t len = 0; /* 当前最长前后缀匹配长度 */
    lps[0] = 0;       /* 首字符的前缀表值为0 */

    uint32_t i = 1;
    while (i < pattern_len)
    {
        if (pattern[i] == pattern[len])
        {
            len++;
            lps[i] = len;
            i++;
        }
        else
        {
            if (len != 0)
            {
                len = lps[len - 1]; /* 回退到前一个可能匹配位置 */
            }
            else
            {
                lps[i] = 0;
                i++;
            }
        }
    }
}

/**
 * @brief  查找子串在父串中的位置（安全版，带长度限制）
 * @param  parent: 父串指针
 * @param  sub: 子串指针
 * @param  parent_max_len: 父串最大长度
 * @param  sub_max_len: 子串最大长度
 * @return 子串在父串中的起始位置索引，未找到返回0xFFFFFFFF
 */
uint32_t String_FindSubstring(const char *parent, const char *sub, uint32_t parent_max_len, uint32_t sub_max_len)
{
    /* 参数合法性检查 */
    if (parent == NULL || sub == NULL || parent_max_len == 0 || sub_max_len == 0)
    {
        return 0xFFFFFFFF;
    }

    /* 获取实际长度（受max_len限制） */
    uint32_t parent_len = String_GetLength(parent, parent_max_len);
    uint32_t sub_len = String_GetLength(sub, sub_max_len);

    /* 边界情况处理 */
    if (sub_len == 0)
    {
        return 0; /* 空字符串是任何字符串的子串，返回父串首地址 */
    }

    if (parent_len < sub_len)
    {
        return 0xFFFFFFFF; /* 父串长度不足，不可能是子串 */
    }

    /* 特殊情况优化：子串长度为1时，使用简单查找 */
    if (sub_len == 1)
    {
        char target = sub[0];
        for (uint32_t i = 0; i < parent_len; i++)
        {
            if (parent[i] == target)
            {
                return i; /* 返回找到的字符位置 */
            }
        }
        return 0xFFFFFFFF; /* 未找到 */
    }

    /* 计算KMP前缀表 */
    uint32_t *lps = (uint32_t *)MemoryManager_Malloc(MP_RAM_EX, sub_len * sizeof(uint32_t));
    if (lps == NULL)
    {
        /* 内存不足，降级为朴素搜索算法 */
        for (uint32_t i = 0; i <= parent_len - sub_len; i++)
        {
            uint32_t j = 0;
            for (; j < sub_len; j++)
            {
                if (parent[i + j] != sub[j])
                {
                    break; /* 不匹配，立即退出 */
                }
            }
            if (j == sub_len) /* 完全匹配 */
            {
                return i; /* 返回匹配起始位置 */
            }
        }
        return 0xFFFFFFFF; /* 未找到匹配 */
    }

    /* 计算LPS数组 */
    KMP_ComputeLPS(sub, sub_len, lps);

    /* KMP搜索主循环 */
    uint32_t i = 0; /* 父串中的当前位置 */
    uint32_t j = 0; /* 子串中的当前位置 */

    while (i < parent_len)
    {
        /* 当前字符匹配 */
        if (parent[i] == sub[j])
        {
            i++;
            j++;
        }

        /* 完全匹配成功 */
        if (j == sub_len)
        {
            MemoryManager_Free(MP_RAM_EX, lps);
            return i - sub_len; /* 返回匹配起始位置 */
        }
        /* 不匹配且不在子串开头 */
        else if (i < parent_len && parent[i] != sub[j])
        {
            if (j != 0)
            {
                j = lps[j - 1]; /* 利用前缀表跳过不必要的比较 */
            }
            else
            {
                i++; /* 子串在起始位置不匹配，父串右移 */
            }
        }
    }

    /* 释放内存并返回失败 */
    MemoryManager_Free(MP_RAM_EX, lps);
    return 0xFFFFFFFF; /* 未找到子串 */
}

/**
 * @brief  判断子串是否为父串的子串（安全版，带长度限制）
 * @param  parent: 父串指针
 * @param  sub: 子串指针
 * @param  parent_max_len: 父串最大长度
 * @param  sub_max_len: 子串最大长度
 * @return 1-是子串，0-不是子串
 */
uint32_t String_IsSubstring(const char *parent, const char *sub, uint32_t parent_max_len, uint32_t sub_max_len)
{
    return String_FindSubstring(parent, sub, parent_max_len, sub_max_len) != 0xFFFFFFFF; /* 未找到子串 */
}

/**
 * @brief  判断字符是否为空白字符
 * @param  ch: 待检查的字符
 * @retval 1-是空白字符，0-不是空白字符
 */
static uint32_t String_IsWhitespace(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') ? 1 : 0;
}

/**
 * @brief  删除字符串前后的空白字符（原地修改）
 * @param  str: 待处理的字符串指针
 * @param  max_len: 最大处理长度（防止越界访问）
 * @retval 处理后的字符串长度（不包含结尾的'\0'）
 */
uint32_t String_Trim(char *str, uint32_t max_len)
{
    /* 参数合法性检查 */
    if (str == NULL || max_len == 0)
    {
        return 0;
    }

    /* 获取字符串实际长度（受max_len限制） */
    uint32_t len = String_GetLength(str, max_len);

    /* 处理空字符串情况 */
    if (len == 0)
    {
        return 0;
    }

    /* 查找第一个非空白字符的位置 */
    uint32_t start = 0;
    while (start < len && String_IsWhitespace(str[start]))
    {
        start++;
    }

    /* 如果全是空白字符，返回空字符串 */
    if (start == len)
    {
        str[0] = '\0';
        return 0;
    }

    /* 查找最后一个非空白字符的位置 */
    uint32_t end = len - 1;
    while (end > start && String_IsWhitespace(str[end]))
    {
        end--;
    }

    /* 计算新字符串的长度 */
    uint32_t new_len = end - start + 1;

    /* 如果存在前导空白字符，需要移动内容到字符串开头 */
    if (start > 0)
    {
        /* 使用Memory_Copy处理内存重叠，将有效内容移动到缓冲区开头 */
        Memory_Copy(str, &str[start], new_len);
    }

    /* 在新字符串末尾添加结束符 */
    str[new_len] = '\0';

    return new_len; /* 返回处理后的字符串长度 */
}

/**
 * @brief  BKDR哈希函数（区分大小写）
 * @param  str 输入字符串
 * @return 哈希值
 */
uint32_t String_HashKey(const char *str, uint32_t max_len)
{
    uint32_t seed = 131; /* 31 131 1313 13131 131313 etc.. */
    uint32_t hash = 0, index = 0;
    char ch;

    while (str[index] != '\0' && index < max_len)
    {
        ch = *str;
        hash = hash * seed + ch;
        str++;
    }

    return (hash & 0x7FFFFFFF);
}

/**
 * @brief  BKDR哈希函数（不区分大小写）
 * @param  str 输入字符串
 * @return 哈希值
 */
uint32_t String_HashKeyWithoutCase(const char *str, uint32_t max_len)
{
    uint32_t seed = 131; /* 31 131 1313 13131 131313 etc.. */
    uint32_t hash = 0, index = 0;
    char ch;

    while (str[index] != '\0' && index < max_len)
    {
        ch = Tolower(*str);
        hash = hash * seed + ch;
        str++;
    }

    return (hash & 0x7FFFFFFF);
}
