/**
 * @file    variable_runtime.c
 * @brief   VarValue_T 动态变量运行时
 * @details 支持 int/float/double/string/array/dict/struct；
 *          string/array 支持切片与拼接，拼接前做类型检查；
 *          字符串长度不固定，由 malloc/free 管理。
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "variable_runtime.h"

/* 初始容器容量 */
#define VAR_RUNTIME_INIT_CAPACITY 4u

/**
 * @brief  创建空变量节点
 * @param  type 变量类型
 * @retval 新节点；内存不足返回 NULL
 */
static VarValue_T *Var_New(VarType_T type)
{
    VarValue_T *pValue = (VarValue_T *)calloc(1, sizeof(VarValue_T));
    if (pValue != NULL) {
        pValue->type = type;
    }
    return pValue;
}

/**
 * @brief  创建 int 变量
 */
VarValue_T *Var_CreateInt(int32_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_INT);
    if (pValue != NULL) {
        pValue->u.intValue = value;
    }
    return pValue;
}

VarValue_T *Var_CreateUint8(uint8_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_UINT8);
    if (pValue != NULL) { pValue->u.uint8Value = value; }
    return pValue;
}

VarValue_T *Var_CreateInt8(int8_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_INT8);
    if (pValue != NULL) { pValue->u.int8Value = value; }
    return pValue;
}

VarValue_T *Var_CreateUint16(uint16_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_UINT16);
    if (pValue != NULL) { pValue->u.uint16Value = value; }
    return pValue;
}

VarValue_T *Var_CreateInt16(int16_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_INT16);
    if (pValue != NULL) { pValue->u.int16Value = value; }
    return pValue;
}

VarValue_T *Var_CreateUint32(uint32_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_UINT32);
    if (pValue != NULL) { pValue->u.uint32Value = value; }
    return pValue;
}

VarValue_T *Var_CreateUint64(uint64_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_UINT64);
    if (pValue != NULL) { pValue->u.uint64Value = value; }
    return pValue;
}

VarValue_T *Var_CreateInt64(int64_t value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_INT64);
    if (pValue != NULL) { pValue->u.int64Value = value; }
    return pValue;
}

/**
 * @brief  创建 float 变量
 */
VarValue_T *Var_CreateFloat(float value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_FLOAT);
    if (pValue != NULL) {
        pValue->u.floatValue = value;
    }
    return pValue;
}

/**
 * @brief  创建 double 变量
 */
VarValue_T *Var_CreateDouble(double value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_DOUBLE);
    if (pValue != NULL) {
        pValue->u.doubleValue = value;
    }
    return pValue;
}

/**
 * @brief  创建 char 变量
 */
VarValue_T *Var_CreateChar(char value)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_CHAR);
    if (pValue != NULL) {
        pValue->u.charValue = value;
    }
    return pValue;
}

/**
 * @brief  创建字符串变量
 * @param  text 字符串内容
 * @retval 新字符串节点；失败返回 NULL
 */
VarValue_T *Var_CreateString(const char *text)
{
    size_t length = 0u;
    VarValue_T *pValue = NULL;

    if (text == NULL) {
        text = "";
    }
    length = strlen(text);

    pValue = Var_New(VAR_TYPE_STRING);
    if (pValue == NULL) {
        return NULL;
    }
    pValue->u.stringValue = (char *)malloc(length + 1u);
    if (pValue->u.stringValue == NULL) {
        Var_Free(pValue);
        return NULL;
    }
    memcpy(pValue->u.stringValue, text, length + 1u);
    return pValue;
}

/**
 * @brief  创建空数组
 */
VarValue_T *Var_CreateArray(void)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_ARRAY);
    if (pValue == NULL) {
        return NULL;
    }
    pValue->u.array.capacity = VAR_RUNTIME_INIT_CAPACITY;
    pValue->u.array.items = (VarValue_T **)calloc(
        pValue->u.array.capacity, sizeof(VarValue_T *));
    if (pValue->u.array.items == NULL) {
        Var_Free(pValue);
        return NULL;
    }
    return pValue;
}

/**
 * @brief  创建空字典
 */
VarValue_T *Var_CreateDict(void)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_DICT);
    if (pValue == NULL) {
        return NULL;
    }
    pValue->u.dict.capacity = VAR_RUNTIME_INIT_CAPACITY;
    pValue->u.dict.entries = (VarKeyValue_T *)calloc(
        pValue->u.dict.capacity, sizeof(VarKeyValue_T));
    if (pValue->u.dict.entries == NULL) {
        Var_Free(pValue);
        return NULL;
    }
    return pValue;
}

/**
 * @brief  创建空结构体
 */
VarValue_T *Var_CreateStruct(void)
{
    VarValue_T *pValue = Var_New(VAR_TYPE_STRUCT);
    if (pValue == NULL) {
        return NULL;
    }
    pValue->u.object.capacity = VAR_RUNTIME_INIT_CAPACITY;
    pValue->u.object.members = (VarMember_T *)calloc(
        pValue->u.object.capacity, sizeof(VarMember_T));
    if (pValue->u.object.members == NULL) {
        Var_Free(pValue);
        return NULL;
    }
    return pValue;
}

/**
 * @brief  只释放变量内部内容，不释放节点本身
 * @param  pValue 待清理变量；NULL 直接返回
 * @note   Var_SetValue 会借用栈上临时节点做内容交换，因此不能调用 Var_Free
 */
static void Var_FreeContent(VarValue_T *pValue)
{
    size_t index = 0u;

    if (pValue == NULL) {
        return;
    }
    switch (pValue->type) {
        case VAR_TYPE_STRING:
            free(pValue->u.stringValue);
            break;
        case VAR_TYPE_ARRAY:
            for (index = 0u; index < pValue->u.array.length; index++) {
                Var_Free(pValue->u.array.items[index]);
            }
            free(pValue->u.array.items);
            break;
        case VAR_TYPE_DICT:
            for (index = 0u; index < pValue->u.dict.length; index++) {
                free(pValue->u.dict.entries[index].key);
                Var_Free(pValue->u.dict.entries[index].value);
            }
            free(pValue->u.dict.entries);
            break;
        case VAR_TYPE_STRUCT:
            for (index = 0u; index < pValue->u.object.length; index++) {
                free(pValue->u.object.members[index].name);
                Var_Free(pValue->u.object.members[index].value);
            }
            free(pValue->u.object.members);
            break;
        default:
            break;
    }
}

/**
 * @brief  递归释放变量
 * @param  pValue 待释放变量；NULL 直接返回
 */
void Var_Free(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return;
    }
    Var_FreeContent(pValue);
    free(pValue);
}

/**
 * @brief  深拷贝变量
 * @param  pSrc 源变量
 * @retval 拷贝结果；失败返回 NULL
 */
static VarValue_T *Var_Clone(const VarValue_T *pSrc)
{
    VarValue_T *pDst = NULL;
    size_t index = 0u;

    if (pSrc == NULL) {
        return NULL;
    }
    switch (pSrc->type) {
        case VAR_TYPE_INT:
            return Var_CreateInt(pSrc->u.intValue);
        case VAR_TYPE_UINT8:
            return Var_CreateUint8(pSrc->u.uint8Value);
        case VAR_TYPE_INT8:
            return Var_CreateInt8(pSrc->u.int8Value);
        case VAR_TYPE_UINT16:
            return Var_CreateUint16(pSrc->u.uint16Value);
        case VAR_TYPE_INT16:
            return Var_CreateInt16(pSrc->u.int16Value);
        case VAR_TYPE_UINT32:
            return Var_CreateUint32(pSrc->u.uint32Value);
        case VAR_TYPE_UINT64:
            return Var_CreateUint64(pSrc->u.uint64Value);
        case VAR_TYPE_INT64:
            return Var_CreateInt64(pSrc->u.int64Value);
        case VAR_TYPE_FLOAT:
            return Var_CreateFloat(pSrc->u.floatValue);
        case VAR_TYPE_DOUBLE:
            return Var_CreateDouble(pSrc->u.doubleValue);
        case VAR_TYPE_CHAR:
            return Var_CreateChar(pSrc->u.charValue);
        case VAR_TYPE_STRING:
            return Var_CreateString(pSrc->u.stringValue);
        case VAR_TYPE_ARRAY:
            pDst = Var_CreateArray();
            if (pDst == NULL) {
                return NULL;
            }
            for (index = 0u; index < pSrc->u.array.length; index++) {
                if (Var_ArrayPush(pDst,
                                  Var_Clone(pSrc->u.array.items[index]))
                    != VAR_RUNTIME_OK) {
                    Var_Free(pDst);
                    return NULL;
                }
            }
            return pDst;
        case VAR_TYPE_DICT:
            pDst = Var_CreateDict();
            if (pDst == NULL) {
                return NULL;
            }
            for (index = 0u; index < pSrc->u.dict.length; index++) {
                Var_DictSet(pDst,
                            pSrc->u.dict.entries[index].key,
                            Var_Clone(pSrc->u.dict.entries[index].value));
            }
            return pDst;
        case VAR_TYPE_STRUCT:
            pDst = Var_CreateStruct();
            if (pDst == NULL) {
                return NULL;
            }
            for (index = 0u; index < pSrc->u.object.length; index++) {
                Var_StructSetField(pDst,
                                   pSrc->u.object.members[index].name,
                                   Var_Clone(pSrc->u.object.members[index].value));
            }
            return pDst;
        default:
            return NULL;
    }
}

/**
 * @brief  数组扩容
 */
static int32_t Var_ArrayGrow(VarArrayBody_T *pBody)
{
    size_t newCapacity = 0u;
    VarValue_T **pNewItems = NULL;

    newCapacity = pBody->capacity * 2u;
    pNewItems = (VarValue_T **)realloc(
        pBody->items, newCapacity * sizeof(VarValue_T *));
    if (pNewItems == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    pBody->items = pNewItems;
    pBody->capacity = newCapacity;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  向数组尾部添加元素
 * @note   数组接管 pItem 所有权，失败时调用者仍需自行释放
 */
int32_t Var_ArrayPush(VarValue_T *pArray, VarValue_T *pItem)
{
    int32_t ret = VAR_RUNTIME_OK;

    if ((pArray == NULL) || (pArray->type != VAR_TYPE_ARRAY)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    if (pItem == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    if (pArray->u.array.length >= pArray->u.array.capacity) {
        ret = Var_ArrayGrow(&pArray->u.array);
        if (ret != VAR_RUNTIME_OK) {
            return ret;
        }
    }
    pArray->u.array.items[pArray->u.array.length] = pItem;
    pArray->u.array.length++;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  读取数组元素
 */
VarValue_T *Var_ArrayGet(VarValue_T *pArray, size_t index)
{
    if ((pArray == NULL) || (pArray->type != VAR_TYPE_ARRAY)) {
        return NULL;
    }
    if (index >= pArray->u.array.length) {
        return NULL;
    }
    return pArray->u.array.items[index];
}

/**
 * @brief  写入数组元素（旧值由本函数释放）
 */
int32_t Var_ArraySet(VarValue_T *pArray, size_t index, VarValue_T *pItem)
{
    if ((pArray == NULL) || (pItem == NULL) ||
        (pArray->type != VAR_TYPE_ARRAY)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    if (index >= pArray->u.array.length) {
        return VAR_RUNTIME_ERR_BOUND;
    }
    Var_Free(pArray->u.array.items[index]);
    pArray->u.array.items[index] = pItem;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  字典扩容
 */
static int32_t Var_DictGrow(VarDictBody_T *pBody)
{
    size_t newCapacity = 0u;
    VarKeyValue_T *pNewEntries = NULL;

    newCapacity = pBody->capacity * 2u;
    pNewEntries = (VarKeyValue_T *)realloc(
        pBody->entries, newCapacity * sizeof(VarKeyValue_T));
    if (pNewEntries == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    pBody->entries = pNewEntries;
    pBody->capacity = newCapacity;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  设置字典键值
 * @note   若 key 已存在则替换旧值；字典接管 pValue 所有权
 */
int32_t Var_DictSet(VarValue_T *pDict, const char *key, VarValue_T *pValue)
{
    size_t index = 0u;

    if ((pDict == NULL) || (pDict->type != VAR_TYPE_DICT) ||
        (key == NULL) || (pValue == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }

    for (index = 0u; index < pDict->u.dict.length; index++) {
        if (strcmp(pDict->u.dict.entries[index].key, key) == 0) {
            Var_Free(pDict->u.dict.entries[index].value);
            pDict->u.dict.entries[index].value = pValue;
            return VAR_RUNTIME_OK;
        }
    }

    if (pDict->u.dict.length >= pDict->u.dict.capacity) {
        if (Var_DictGrow(&pDict->u.dict) != VAR_RUNTIME_OK) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
    }
    pDict->u.dict.entries[pDict->u.dict.length].key =
        (char *)malloc(strlen(key) + 1u);
    if (pDict->u.dict.entries[pDict->u.dict.length].key == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    strcpy(pDict->u.dict.entries[pDict->u.dict.length].key, key);
    pDict->u.dict.entries[pDict->u.dict.length].value = pValue;
    pDict->u.dict.length++;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  读取字典键值
 */
VarValue_T *Var_DictGet(VarValue_T *pDict, const char *key)
{
    size_t index = 0u;

    if ((pDict == NULL) || (pDict->type != VAR_TYPE_DICT) ||
        (key == NULL)) {
        return NULL;
    }
    for (index = 0u; index < pDict->u.dict.length; index++) {
        if (strcmp(pDict->u.dict.entries[index].key, key) == 0) {
            return pDict->u.dict.entries[index].value;
        }
    }
    return NULL;
}

/**
 * @brief  结构体成员扩容
 */
static int32_t Var_StructGrow(VarStructBody_T *pBody)
{
    size_t newCapacity = 0u;
    VarMember_T *pNewMembers = NULL;

    newCapacity = pBody->capacity * 2u;
    pNewMembers = (VarMember_T *)realloc(
        pBody->members, newCapacity * sizeof(VarMember_T));
    if (pNewMembers == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    pBody->members = pNewMembers;
    pBody->capacity = newCapacity;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  设置结构体成员
 */
int32_t Var_StructSetField(VarValue_T *pStruct, const char *name,
                           VarValue_T *pValue)
{
    size_t index = 0u;

    if ((pStruct == NULL) || (pStruct->type != VAR_TYPE_STRUCT) ||
        (name == NULL) || (pValue == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }

    for (index = 0u; index < pStruct->u.object.length; index++) {
        if (strcmp(pStruct->u.object.members[index].name, name) == 0) {
            Var_Free(pStruct->u.object.members[index].value);
            pStruct->u.object.members[index].value = pValue;
            return VAR_RUNTIME_OK;
        }
    }

    if (pStruct->u.object.length >= pStruct->u.object.capacity) {
        if (Var_StructGrow(&pStruct->u.object) != VAR_RUNTIME_OK) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
    }
    pStruct->u.object.members[pStruct->u.object.length].name =
        (char *)malloc(strlen(name) + 1u);
    if (pStruct->u.object.members[pStruct->u.object.length].name == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    strcpy(pStruct->u.object.members[pStruct->u.object.length].name, name);
    pStruct->u.object.members[pStruct->u.object.length].value = pValue;
    pStruct->u.object.length++;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  读取结构体成员
 */
VarValue_T *Var_StructGetField(VarValue_T *pStruct, const char *name)
{
    size_t index = 0u;

    if ((pStruct == NULL) || (pStruct->type != VAR_TYPE_STRUCT) ||
        (name == NULL)) {
        return NULL;
    }
    for (index = 0u; index < pStruct->u.object.length; index++) {
        if (strcmp(pStruct->u.object.members[index].name, name) == 0) {
            return pStruct->u.object.members[index].value;
        }
    }
    return NULL;
}

/**
 * @brief  归一化切片索引
 */
static int64_t Var_NormalizeIndex(int64_t index, int64_t length)
{
    if (index < 0) {
        index += length;
    }
    if (index < 0) {
        return 0;
    }
    if (index > length) {
        return length;
    }
    return index;
}

/**
 * @brief  字符串/数组切片
 */
int32_t Var_Slice(VarValue_T *pValue,
                  int64_t start,
                  int64_t end,
                  VarValue_T **ppOut)
{
    int64_t length = 0;
    int64_t begin = 0;
    int64_t finish = 0;
    VarValue_T *pResult = NULL;
    int64_t index = 0;

    if ((pValue == NULL) || (ppOut == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }

    if (pValue->type == VAR_TYPE_STRING) {
        length = (int64_t)strlen(pValue->u.stringValue);
        begin = Var_NormalizeIndex(start, length);
        finish = Var_NormalizeIndex(end, length);
        if (begin >= finish) {
            begin = finish;
        }
        pResult = Var_CreateString("");
        if (pResult == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        free(pResult->u.stringValue);
        pResult->u.stringValue = (char *)malloc(
            (size_t)(finish - begin) + 1u);
        if (pResult->u.stringValue == NULL) {
            Var_Free(pResult);
            return VAR_RUNTIME_ERR_MEMORY;
        }
        memcpy(pResult->u.stringValue,
               pValue->u.stringValue + begin,
               (size_t)(finish - begin));
        pResult->u.stringValue[finish - begin] = '\0';
        *ppOut = pResult;
        return VAR_RUNTIME_OK;
    }

    if (pValue->type == VAR_TYPE_ARRAY) {
        length = (int64_t)pValue->u.array.length;
        begin = Var_NormalizeIndex(start, length);
        finish = Var_NormalizeIndex(end, length);
        if (begin >= finish) {
            begin = finish;
        }
        pResult = Var_CreateArray();
        if (pResult == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        for (index = begin; index < finish; index++) {
            Var_ArrayPush(pResult, Var_Clone(
                pValue->u.array.items[index]));
        }
        *ppOut = pResult;
        return VAR_RUNTIME_OK;
    }

    return VAR_RUNTIME_ERR_TYPE;
}

/**
 * @brief  判断两个变量是否允许拼接
 */
int32_t Var_CanConcat(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    if ((pLeft->type == VAR_TYPE_STRING) &&
        (pRight->type == VAR_TYPE_STRING)) {
        return VAR_RUNTIME_OK;
    }
    if ((pLeft->type == VAR_TYPE_ARRAY) &&
        (pRight->type == VAR_TYPE_ARRAY)) {
        return VAR_RUNTIME_OK;
    }
    return VAR_RUNTIME_ERR_TYPE;
}

/**
 * @brief  拼接字符串或数组（先类型检查）
 */
int32_t Var_Concat(VarValue_T *pLeft,
                   VarValue_T *pRight,
                   VarValue_T **ppOut)
{
    VarValue_T *pResult = NULL;
    size_t leftLength = 0u;
    size_t rightLength = 0u;
    size_t index = 0u;

    if ((ppOut == NULL) ||
        (Var_CanConcat(pLeft, pRight) != VAR_RUNTIME_OK)) {
        return VAR_RUNTIME_ERR_TYPE;
    }

    if (pLeft->type == VAR_TYPE_STRING) {
        leftLength = strlen(pLeft->u.stringValue);
        rightLength = strlen(pRight->u.stringValue);
        pResult = Var_CreateString("");
        if (pResult == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        free(pResult->u.stringValue);
        pResult->u.stringValue = (char *)malloc(
            leftLength + rightLength + 1u);
        if (pResult->u.stringValue == NULL) {
            Var_Free(pResult);
            return VAR_RUNTIME_ERR_MEMORY;
        }
        memcpy(pResult->u.stringValue,
               pLeft->u.stringValue, leftLength);
        memcpy(pResult->u.stringValue + leftLength,
               pRight->u.stringValue, rightLength + 1u);
        *ppOut = pResult;
        return VAR_RUNTIME_OK;
    }

    /* 数组：结果内元素为深拷贝 */
    pResult = Var_CreateArray();
    if (pResult == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    for (index = 0u; index < pLeft->u.array.length; index++) {
        Var_ArrayPush(pResult, Var_Clone(pLeft->u.array.items[index]));
    }
    for (index = 0u; index < pRight->u.array.length; index++) {
        Var_ArrayPush(pResult, Var_Clone(pRight->u.array.items[index]));
    }
    *ppOut = pResult;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  深拷贝变量（公开桥接）
 */
VarValue_T *Var_CloneValue(const VarValue_T *pValue)
{
    return Var_Clone(pValue);
}

/**
 * @brief  把简单数值统一读取为 long double，供严格 Getter 做范围检查。
 * @param  pValue 源动态值。
 * @param  pResult 输出的高精度临时数值。
 * @retval VAR_RUNTIME_OK 表示成功，其他值表示类型或参数错误。
 */
static int32_t Var_GetLongDouble(const VarValue_T *pValue,
                                 long double *pResult)
{
    long double value = 0.0L;

    if ((pValue == NULL) || (pResult == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    switch (pValue->type) {
        case VAR_TYPE_INT:    value = (long double)pValue->u.intValue; break;
        case VAR_TYPE_UINT8:  value = (long double)pValue->u.uint8Value; break;
        case VAR_TYPE_INT8:   value = (long double)pValue->u.int8Value; break;
        case VAR_TYPE_UINT16: value = (long double)pValue->u.uint16Value; break;
        case VAR_TYPE_INT16:  value = (long double)pValue->u.int16Value; break;
        case VAR_TYPE_UINT32: value = (long double)pValue->u.uint32Value; break;
        case VAR_TYPE_UINT64: value = (long double)pValue->u.uint64Value; break;
        case VAR_TYPE_INT64:  value = (long double)pValue->u.int64Value; break;
        case VAR_TYPE_FLOAT:  value = (long double)pValue->u.floatValue; break;
        case VAR_TYPE_DOUBLE: value = (long double)pValue->u.doubleValue; break;
        case VAR_TYPE_CHAR:   value = (long double)pValue->u.charValue; break;
        default: return VAR_RUNTIME_ERR_TYPE;
    }
    if (isfinite(value) == 0) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = value;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  严格读取 int8_t。
 * @param  pValue 源动态值。
 * @param  pResult 输出值。
 * @retval 运行时状态码。
 */
int32_t Var_GetInt8(const VarValue_T *pValue, int8_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < INT8_MIN) || (value > INT8_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (int8_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 uint8_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetUint8(const VarValue_T *pValue, uint8_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < 0.0L) || (value > UINT8_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (uint8_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 int16_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetInt16(const VarValue_T *pValue, int16_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < INT16_MIN) || (value > INT16_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (int16_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 uint16_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetUint16(const VarValue_T *pValue, uint16_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < 0.0L) || (value > UINT16_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (uint16_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 int32_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetInt32(const VarValue_T *pValue, int32_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < INT32_MIN) || (value > INT32_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (int32_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 uint32_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetUint32(const VarValue_T *pValue, uint32_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < 0.0L) || (value > UINT32_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (uint32_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 int64_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetInt64(const VarValue_T *pValue, int64_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < (long double)INT64_MIN) ||
        (value > (long double)INT64_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (int64_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 uint64_t。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetUint64(const VarValue_T *pValue, uint64_t *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < 0.0L) || (value > (long double)UINT64_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (uint64_t)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 float。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetFloat(const VarValue_T *pValue, float *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < -(long double)FLT_MAX) ||
        (value > (long double)FLT_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (float)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 double。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetDouble(const VarValue_T *pValue, double *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < -(long double)DBL_MAX) ||
        (value > (long double)DBL_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (double)value;
    return VAR_RUNTIME_OK;
}

/** @brief 严格读取 char。 @param pValue 源值。 @param pResult 输出值。 @retval 运行时状态码。 */
int32_t Var_GetChar(const VarValue_T *pValue, char *pResult)
{
    long double value = 0.0L;
    int32_t ret = Var_GetLongDouble(pValue, &value);
    if ((ret != VAR_RUNTIME_OK) || (pResult == NULL)) {
        return (pResult == NULL) ? VAR_RUNTIME_ERR_TYPE : ret;
    }
    if ((value < CHAR_MIN) || (value > CHAR_MAX)) {
        return VAR_RUNTIME_ERR_RANGE;
    }
    *pResult = (char)value;
    return VAR_RUNTIME_OK;
}

/** @brief 读取字符串只读指针。 @param pValue 源值。 @param ppResult 输出指针。 @retval 运行时状态码。 */
int32_t Var_GetString(const VarValue_T *pValue, const char **ppResult)
{
    if ((pValue == NULL) || (ppResult == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    if (pValue->type != VAR_TYPE_STRING) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    *ppResult = pValue->u.stringValue;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  按类型模板转换并深拷贝动态值，支持简单类型、数组、字典与结构体。
 * @param  pSource 源动态值。
 * @param  pTypeTemplate 类型模板。
 * @param  ppResult 输出的新动态值。
 * @retval 运行时状态码。
 */
int32_t Var_ConvertValue(const VarValue_T *pSource,
                         const VarValue_T *pTypeTemplate,
                         VarValue_T **ppResult)
{
    VarValue_T *pResult = NULL;
    size_t index = 0u;
    int32_t ret = VAR_RUNTIME_OK;

    if ((pSource == NULL) || (pTypeTemplate == NULL) ||
        (ppResult == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    switch (pTypeTemplate->type) {
        case VAR_TYPE_INT: { int32_t value; ret = Var_GetInt32(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateInt(value); } break; }
        case VAR_TYPE_UINT8: { uint8_t value; ret = Var_GetUint8(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateUint8(value); } break; }
        case VAR_TYPE_INT8: { int8_t value; ret = Var_GetInt8(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateInt8(value); } break; }
        case VAR_TYPE_UINT16: { uint16_t value; ret = Var_GetUint16(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateUint16(value); } break; }
        case VAR_TYPE_INT16: { int16_t value; ret = Var_GetInt16(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateInt16(value); } break; }
        case VAR_TYPE_UINT32: { uint32_t value; ret = Var_GetUint32(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateUint32(value); } break; }
        case VAR_TYPE_UINT64: { uint64_t value; ret = Var_GetUint64(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateUint64(value); } break; }
        case VAR_TYPE_INT64: { int64_t value; ret = Var_GetInt64(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateInt64(value); } break; }
        case VAR_TYPE_FLOAT: { float value; ret = Var_GetFloat(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateFloat(value); } break; }
        case VAR_TYPE_DOUBLE: { double value; ret = Var_GetDouble(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateDouble(value); } break; }
        case VAR_TYPE_CHAR: { char value; ret = Var_GetChar(pSource, &value); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateChar(value); } break; }
        case VAR_TYPE_STRING: { const char *pText = NULL; ret = Var_GetString(pSource, &pText); if (ret == VAR_RUNTIME_OK) { pResult = Var_CreateString(pText); } break; }
        case VAR_TYPE_ARRAY:
            if ((pSource->type != VAR_TYPE_ARRAY) ||
                (pSource->u.array.length != pTypeTemplate->u.array.length)) {
                return VAR_RUNTIME_ERR_BOUND;
            }
            pResult = Var_CreateArray();
            if (pResult == NULL) {
                return VAR_RUNTIME_ERR_MEMORY;
            }
            for (index = 0u; index < pSource->u.array.length; index++) {
                VarValue_T *pItem = NULL;
                ret = Var_ConvertValue(pSource->u.array.items[index],
                                       pTypeTemplate->u.array.items[index],
                                       &pItem);
                if (ret != VAR_RUNTIME_OK) {
                    Var_Free(pResult);
                    return ret;
                }
                ret = Var_ArrayPush(pResult, pItem);
                if (ret != VAR_RUNTIME_OK) {
                    Var_Free(pItem);
                    Var_Free(pResult);
                    return ret;
                }
            }
            break;
        case VAR_TYPE_DICT:
        case VAR_TYPE_STRUCT:
            /* 中文注释：复杂类型以模板为形状依据，类型一致时深拷贝源值 */
            if (pSource->type != pTypeTemplate->type) {
                return VAR_RUNTIME_ERR_TYPE;
            }
            pResult = Var_Clone(pSource);
            if (pResult == NULL) {
                ret = VAR_RUNTIME_ERR_MEMORY;
            }
            break;
        default:
            return VAR_RUNTIME_ERR_TYPE;
    }
    if (ret != VAR_RUNTIME_OK) {
        return ret;
    }
    if (pResult == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }
    *ppResult = pResult;
    return VAR_RUNTIME_OK;
}

/**
 * @brief  把变量转换为 int（非数值类型返回 0）
 */
int64_t Var_ToInt(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return 0;
    }
    switch (pValue->type) {
        case VAR_TYPE_INT:
            return (int64_t)pValue->u.intValue;
        case VAR_TYPE_UINT8:
            return (int64_t)pValue->u.uint8Value;
        case VAR_TYPE_INT8:
            return (int64_t)pValue->u.int8Value;
        case VAR_TYPE_UINT16:
            return (int64_t)pValue->u.uint16Value;
        case VAR_TYPE_INT16:
            return (int64_t)pValue->u.int16Value;
        case VAR_TYPE_UINT32:
            return (int64_t)pValue->u.uint32Value;
        case VAR_TYPE_UINT64:
            return (int64_t)pValue->u.uint64Value;
        case VAR_TYPE_INT64:
            return pValue->u.int64Value;
        case VAR_TYPE_FLOAT:
            return (int64_t)pValue->u.floatValue;
        case VAR_TYPE_DOUBLE:
            return (int64_t)pValue->u.doubleValue;
        case VAR_TYPE_CHAR:
            return (int64_t)pValue->u.charValue;
        default:
            return 0;
    }
}

/**
 * @brief  把变量转换为 double
 */
double Var_ToDouble(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return 0.0;
    }
    switch (pValue->type) {
        case VAR_TYPE_INT:
            return (double)pValue->u.intValue;
        case VAR_TYPE_UINT8:
            return (double)pValue->u.uint8Value;
        case VAR_TYPE_INT8:
            return (double)pValue->u.int8Value;
        case VAR_TYPE_UINT16:
            return (double)pValue->u.uint16Value;
        case VAR_TYPE_INT16:
            return (double)pValue->u.int16Value;
        case VAR_TYPE_UINT32:
            return (double)pValue->u.uint32Value;
        case VAR_TYPE_UINT64:
            return (double)pValue->u.uint64Value;
        case VAR_TYPE_INT64:
            return (double)pValue->u.int64Value;
        case VAR_TYPE_FLOAT:
            return (double)pValue->u.floatValue;
        case VAR_TYPE_DOUBLE:
            return pValue->u.doubleValue;
        case VAR_TYPE_CHAR:
            return (double)pValue->u.charValue;
        default:
            return 0.0;
    }
}

/**
 * @brief  判断变量是否为真
 */
uint8_t Var_IsTrue(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return 0u;
    }
    if (pValue->type == VAR_TYPE_STRING) {
        return (pValue->u.stringValue != NULL) &&
               (pValue->u.stringValue[0] != '\0');
    }
    return Var_ToInt(pValue) != 0;
}

/**
 * @brief  两个值是否都是整数
 */
static uint8_t Var_BothInt(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return 0u;
    }
    return (pLeft->type == VAR_TYPE_INT) &&
           (pRight->type == VAR_TYPE_INT);
}

/**
 * @brief  数值加法
 */
VarValue_T *Var_Add(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        return Var_CreateInt(pLeft->u.intValue + pRight->u.intValue);
    }
    return Var_CreateDouble(Var_ToDouble(pLeft) + Var_ToDouble(pRight));
}

/**
 * @brief  数值减法
 */
VarValue_T *Var_Sub(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        return Var_CreateInt(pLeft->u.intValue - pRight->u.intValue);
    }
    return Var_CreateDouble(Var_ToDouble(pLeft) - Var_ToDouble(pRight));
}

/**
 * @brief  数值乘法
 */
VarValue_T *Var_Mul(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        return Var_CreateInt(pLeft->u.intValue * pRight->u.intValue);
    }
    return Var_CreateDouble(Var_ToDouble(pLeft) * Var_ToDouble(pRight));
}

/**
 * @brief  数值除法
 */
VarValue_T *Var_Div(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        if (pRight->u.intValue == 0) {
            return NULL;
        }
        return Var_CreateInt(pLeft->u.intValue / pRight->u.intValue);
    }
    return Var_CreateDouble(Var_ToDouble(pLeft) / Var_ToDouble(pRight));
}

/** @brief 求余；整数按 C 的 % 计算，浮点按 fmod 处理。 @retval 新变量。 */
VarValue_T *Var_Mod(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        if (pRight->u.intValue == 0) {
            return NULL;
        }
        return Var_CreateInt(pLeft->u.intValue % pRight->u.intValue);
    }
    {
        double rightValue = Var_ToDouble(pRight);
        if (rightValue == 0.0) {
            return NULL;
        }
        return Var_CreateDouble(fmod(Var_ToDouble(pLeft), rightValue));
    }
}

/**
 * @brief  一元负号
 */
VarValue_T *Var_Neg(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    if (pValue->type == VAR_TYPE_INT) {
        return Var_CreateInt(-pValue->u.intValue);
    }
    return Var_CreateDouble(-Var_ToDouble(pValue));
}

/** @brief 判断三个动态值是否都是 int 类型。 @retval 1 表示都是整数。 */
static uint8_t Var_AllInt(VarValue_T *pFirst, VarValue_T *pSecond,
                          VarValue_T *pThird)
{
    if ((pFirst == NULL) || (pSecond == NULL) || (pThird == NULL)) {
        return 0u;
    }
    return (pFirst->type == VAR_TYPE_INT) &&
           (pSecond->type == VAR_TYPE_INT) &&
           (pThird->type == VAR_TYPE_INT);
}

/** @brief 返回两个动态值的较大者。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Max(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        return Var_CreateInt(pLeft->u.intValue >= pRight->u.intValue
                             ? pLeft->u.intValue : pRight->u.intValue);
    }
    return Var_CreateDouble(fmax(Var_ToDouble(pLeft),
                                 Var_ToDouble(pRight)));
}

/** @brief 返回两个动态值的较小者。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Min(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pLeft, pRight)) {
        return Var_CreateInt(pLeft->u.intValue <= pRight->u.intValue
                             ? pLeft->u.intValue : pRight->u.intValue);
    }
    return Var_CreateDouble(fmin(Var_ToDouble(pLeft),
                                 Var_ToDouble(pRight)));
}

/** @brief 返回动态值的绝对值。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Abs(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    if (pValue->type == VAR_TYPE_INT) {
        if (pValue->u.intValue == INT32_MIN) {
            return Var_CreateDouble(fabs((double)pValue->u.intValue));
        }
        return Var_CreateInt(pValue->u.intValue < 0
                             ? -pValue->u.intValue : pValue->u.intValue);
    }
    return Var_CreateDouble(fabs(Var_ToDouble(pValue)));
}

/** @brief 计算动态值幂运算；整数指数且结果不溢出时保持 int。 @retval 新变量。 */
VarValue_T *Var_Pow(VarValue_T *pBase, VarValue_T *pExponent)
{
    int64_t result = 1;
    int64_t index = 0;
    int64_t base = 0;
    int64_t exponent = 0;
    if ((pBase == NULL) || (pExponent == NULL)) {
        return NULL;
    }
    if (Var_BothInt(pBase, pExponent)) {
        base = pBase->u.intValue;
        exponent = pExponent->u.intValue;
        if ((base == 0) && (exponent < 0)) {
            return NULL;
        }
        if (exponent < 0) {
            return Var_CreateDouble(pow((double)base, (double)exponent));
        }
        for (index = 0; index < exponent; index++) {
            result *= base;
            if ((result < INT32_MIN) || (result > INT32_MAX)) {
                return Var_CreateDouble(pow((double)base,
                                            (double)exponent));
            }
        }
        return Var_CreateInt((int32_t)result);
    }
    return Var_CreateDouble(pow(Var_ToDouble(pBase),
                                Var_ToDouble(pExponent)));
}

/** @brief 计算动态值的平方根。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Sqrt(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    return Var_CreateDouble(sqrt(Var_ToDouble(pValue)));
}

/** @brief 向下取整。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Floor(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    if (pValue->type == VAR_TYPE_INT) {
        return Var_CreateInt(pValue->u.intValue);
    }
    return Var_CreateDouble(floor(Var_ToDouble(pValue)));
}

/** @brief 向上取整。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Ceil(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    if (pValue->type == VAR_TYPE_INT) {
        return Var_CreateInt(pValue->u.intValue);
    }
    return Var_CreateDouble(ceil(Var_ToDouble(pValue)));
}

/** @brief 四舍五入。 @retval 新变量；参数为空返回 NULL。 */
VarValue_T *Var_Round(VarValue_T *pValue)
{
    if (pValue == NULL) {
        return NULL;
    }
    if (pValue->type == VAR_TYPE_INT) {
        return Var_CreateInt(pValue->u.intValue);
    }
    return Var_CreateDouble(round(Var_ToDouble(pValue)));
}

/** @brief 把动态值限制在 [low, high] 区间。 @retval 新变量。 */
VarValue_T *Var_Clamp(VarValue_T *pValue, VarValue_T *pLow,
                      VarValue_T *pHigh)
{
    if ((pValue == NULL) || (pLow == NULL) || (pHigh == NULL)) {
        return NULL;
    }
    if (Var_AllInt(pValue, pLow, pHigh)) {
        int32_t value = pValue->u.intValue;
        if (value < pLow->u.intValue) {
            value = pLow->u.intValue;
        }
        if (value > pHigh->u.intValue) {
            value = pHigh->u.intValue;
        }
        return Var_CreateInt(value);
    }
    return Var_CreateDouble(
        fmin(fmax(Var_ToDouble(pValue), Var_ToDouble(pLow)),
             Var_ToDouble(pHigh)));
}

/**
 * @brief  逻辑非
 */
VarValue_T *Var_Not(VarValue_T *pValue)
{
    return Var_CreateInt(Var_IsTrue(pValue) ? 0 : 1);
}

VarValue_T *Var_And(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    return Var_CreateInt((Var_IsTrue(pLeft) != 0) &&
                         (Var_IsTrue(pRight) != 0));
}

VarValue_T *Var_Or(VarValue_T *pLeft, VarValue_T *pRight)
{
    if ((pLeft == NULL) || (pRight == NULL)) {
        return NULL;
    }
    return Var_CreateInt((Var_IsTrue(pLeft) != 0) ||
                         (Var_IsTrue(pRight) != 0));
}

/**
 * @brief  用源值覆盖目标值
 */
int32_t Var_SetValue(VarValue_T *pDst, const VarValue_T *pSrc)
{
    VarValue_T *pCopy = NULL;

    if ((pDst == NULL) || (pSrc == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    pCopy = Var_Clone(pSrc);
    if (pCopy == NULL) {
        return VAR_RUNTIME_ERR_MEMORY;
    }

    /* 临时保存原对象主体，交换内部内容后释放旧内容 */
    {
        VarValue_T temp = *pDst;
        *pDst = *pCopy;
        free(pCopy);
        Var_FreeContent(&temp);
    }
    return VAR_RUNTIME_OK;
}

/**
 * @brief  用标量覆盖变量对象内容
 * @details 先用对应构造函数生成临时节点，再复用 Var_SetValue 完成整对象覆盖，
 *          最后释放临时节点，因此 pDst 的指针始终保持不变。
 */
#define VAR_DEFINE_SCALAR_SETTER(FnName, CreateFn, ArgType)            \
int32_t FnName(VarValue_T *pDst, ArgType value)                        \
{                                                                      \
    VarValue_T *pTemp = CreateFn(value);                               \
    int32_t ret = 0;                                                   \
                                                                       \
    if (pTemp == NULL) {                                               \
        return VAR_RUNTIME_ERR_MEMORY;                                 \
    }                                                                  \
    ret = Var_SetValue(pDst, pTemp);                                   \
    Var_Free(pTemp);                                                   \
    return ret;                                                        \
}

/* 逐类型展开，便于阅读者对照类型名查找对应函数 */
VAR_DEFINE_SCALAR_SETTER(Var_SetInt8, Var_CreateInt8, int8_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetUint8, Var_CreateUint8, uint8_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetChar, Var_CreateChar, char)
VAR_DEFINE_SCALAR_SETTER(Var_SetInt16, Var_CreateInt16, int16_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetUint16, Var_CreateUint16, uint16_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetInt, Var_CreateInt, int32_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetUint32, Var_CreateUint32, uint32_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetInt64, Var_CreateInt64, int64_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetUint64, Var_CreateUint64, uint64_t)
VAR_DEFINE_SCALAR_SETTER(Var_SetFloat, Var_CreateFloat, float)
VAR_DEFINE_SCALAR_SETTER(Var_SetDouble, Var_CreateDouble, double)

#undef VAR_DEFINE_SCALAR_SETTER

/**
 * @brief  类型名字符串
 */
const char *Var_TypeName(VarType_T type)
{
    switch (type) {
        case VAR_TYPE_INT:    return "int";
        case VAR_TYPE_UINT8:  return "uint8";
        case VAR_TYPE_INT8:   return "int8";
        case VAR_TYPE_UINT16: return "uint16";
        case VAR_TYPE_INT16:  return "int16";
        case VAR_TYPE_UINT32: return "uint32";
        case VAR_TYPE_UINT64: return "uint64";
        case VAR_TYPE_INT64:  return "int64";
        case VAR_TYPE_FLOAT:  return "float";
        case VAR_TYPE_DOUBLE: return "double";
        case VAR_TYPE_CHAR:   return "char";
        case VAR_TYPE_STRING: return "string";
        case VAR_TYPE_ARRAY:  return "array";
        case VAR_TYPE_DICT:   return "dict";
        case VAR_TYPE_STRUCT: return "struct";
        default:              return "none";
    }
}
