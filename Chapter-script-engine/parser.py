#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse
import os
import re
import sys
import warnings
from collections import deque

"""
.gram 文本文法加载模块

把 Yacc 风格但去掉 '|' 分隔符的文法描述转换成 parser.py 需要的字典结构：

    %token NUMBER PLUS MINUS STAR SLASH LPAREN RPAREN VAR
    %start expr

    %var X int32 = 0
    %var table int32[16]
    %var pid struct {
        kp float
        ki float
    }

    expr := expr PLUS term      { $$ = $1 + $3; }
    expr := term                { $$ = $1; }

规则特点：
  - 每条产生式独立写成  lhs := rhs { action }，不使用 '|'，也不需要结尾 ';'；
  - %empty 表示空产生式；
  - 允许直接左递归，后续由 parser.py 转换成“基产生式 + 循环续产生式”。
"""

import io
import re

# =====================================================================
# 变量运行时内嵌源码（原 variable_runtime.h / variable_runtime.c）
# 生成 C 代码时由 generate_parser_c 直接嵌入产物，运行时不再依赖外部文件。
# =====================================================================
VARIABLE_RUNTIME_HEADER = r'''#ifndef __VARIABLE_RUNTIME_H
#define __VARIABLE_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 变量类型 */
typedef enum __VarType_T
{
    VAR_TYPE_NONE = 0,
    VAR_TYPE_UINT8,
    VAR_TYPE_INT8,
    VAR_TYPE_UINT16,
    VAR_TYPE_INT16,
    VAR_TYPE_UINT32,
    VAR_TYPE_UINT64,
    VAR_TYPE_INT64,
    VAR_TYPE_INT,
    VAR_TYPE_FLOAT,
    VAR_TYPE_DOUBLE,
    VAR_TYPE_CHAR,
    VAR_TYPE_STRING,
    VAR_TYPE_ARRAY,
    VAR_TYPE_DICT,
    VAR_TYPE_STRUCT
} VarType_T;

/* 前向声明 */
typedef struct __VarValue_T VarValue_T;

/* 字典键值对 */
typedef struct __VarKeyValue_T
{
    char *key;        /* 键名（运行时 malloc/free） */
    VarValue_T *value; /* 键对应的值 */
} VarKeyValue_T;

/* 结构体成员 */
typedef struct __VarMember_T
{
    char *name;       /* 成员名 */
    VarValue_T *value; /* 成员值 */
} VarMember_T;

/* 动态容器 */
typedef struct __VarArrayBody_T
{
    VarValue_T **items;  /* 元素数组 */
    size_t length;       /* 当前长度 */
    size_t capacity;     /* 容量 */
} VarArrayBody_T;

typedef struct __VarDictBody_T
{
    VarKeyValue_T *entries; /* 键值对数组 */
    size_t length;          /* 当前项数 */
    size_t capacity;        /* 容量 */
} VarDictBody_T;

typedef struct __VarStructBody_T
{
    VarMember_T *members;   /* 成员数组 */
    size_t length;          /* 成员数 */
    size_t capacity;        /* 容量 */
} VarStructBody_T;

/* 变量值：所有类型共用 */
struct __VarValue_T
{
    VarType_T type;
    union
    {
        int32_t intValue;
        uint8_t uint8Value;
        int8_t int8Value;
        uint16_t uint16Value;
        int16_t int16Value;
        uint32_t uint32Value;
        uint64_t uint64Value;
        int64_t int64Value;
        float floatValue;
        double doubleValue;
        char charValue;
        char *stringValue;        /* malloc 管理，非固定长度 */
        VarArrayBody_T array;     /* 数组，元素可继续嵌套 */
        VarDictBody_T dict;       /* 字典，value 可继续嵌套 */
        VarStructBody_T object;   /* 结构体，成员可继续嵌套 */
    } u;
};

/* 创建基本类型 */
VarValue_T *Var_CreateInt(int32_t value);
VarValue_T *Var_CreateUint8(uint8_t value);
VarValue_T *Var_CreateInt8(int8_t value);
VarValue_T *Var_CreateUint16(uint16_t value);
VarValue_T *Var_CreateInt16(int16_t value);
VarValue_T *Var_CreateUint32(uint32_t value);
VarValue_T *Var_CreateUint64(uint64_t value);
VarValue_T *Var_CreateInt64(int64_t value);
VarValue_T *Var_CreateFloat(float value);
VarValue_T *Var_CreateDouble(double value);
VarValue_T *Var_CreateChar(char value);

/* 创建字符串：内部复制一份，长度不固定 */
VarValue_T *Var_CreateString(const char *text);

/* 创建空数组 / 字典 / 结构体 */
VarValue_T *Var_CreateArray(void);
VarValue_T *Var_CreateDict(void);
VarValue_T *Var_CreateStruct(void);

/* 释放变量（递归释放内部所有节点） */
void Var_Free(VarValue_T *pValue);

/* 数组操作 */
int32_t Var_ArrayPush(VarValue_T *pArray, VarValue_T *pItem);
VarValue_T *Var_ArrayGet(VarValue_T *pArray, size_t index);
int32_t Var_ArraySet(VarValue_T *pArray, size_t index, VarValue_T *pItem);

/* 字典操作 */
int32_t Var_DictSet(VarValue_T *pDict, const char *key, VarValue_T *pValue);
VarValue_T *Var_DictGet(VarValue_T *pDict, const char *key);

/* 结构体操作 */
int32_t Var_StructSetField(VarValue_T *pStruct, const char *name,
                           VarValue_T *pValue);
VarValue_T *Var_StructGetField(VarValue_T *pStruct, const char *name);

/* 运行时桥接 API */
VarValue_T *Var_CloneValue(const VarValue_T *pValue);
int64_t Var_ToInt(VarValue_T *pValue);
double Var_ToDouble(VarValue_T *pValue);
uint8_t Var_IsTrue(VarValue_T *pValue);
int32_t Var_SetValue(VarValue_T *pDst, const VarValue_T *pSrc);

/* 类型化赋值：把标量直接覆盖到已有变量对象（保留指针、释放旧内容） */
int32_t Var_SetInt8(VarValue_T *pDst, int8_t value);
int32_t Var_SetUint8(VarValue_T *pDst, uint8_t value);
int32_t Var_SetChar(VarValue_T *pDst, char value);
int32_t Var_SetInt16(VarValue_T *pDst, int16_t value);
int32_t Var_SetUint16(VarValue_T *pDst, uint16_t value);
int32_t Var_SetInt(VarValue_T *pDst, int32_t value);
int32_t Var_SetUint32(VarValue_T *pDst, uint32_t value);
int32_t Var_SetInt64(VarValue_T *pDst, int64_t value);
int32_t Var_SetUint64(VarValue_T *pDst, uint64_t value);
int32_t Var_SetFloat(VarValue_T *pDst, float value);
int32_t Var_SetDouble(VarValue_T *pDst, double value);
VarValue_T *Var_Add(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Sub(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Mul(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Div(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Mod(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Neg(VarValue_T *pValue);
VarValue_T *Var_Not(VarValue_T *pValue);
VarValue_T *Var_And(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Or(VarValue_T *pLeft, VarValue_T *pRight);

/* 常见数值函数：参数与返回值均为动态变量 */
VarValue_T *Var_Max(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Min(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Abs(VarValue_T *pValue);
VarValue_T *Var_Pow(VarValue_T *pBase, VarValue_T *pExponent);
VarValue_T *Var_Sqrt(VarValue_T *pValue);
VarValue_T *Var_Floor(VarValue_T *pValue);
VarValue_T *Var_Ceil(VarValue_T *pValue);
VarValue_T *Var_Round(VarValue_T *pValue);
VarValue_T *Var_Clamp(VarValue_T *pValue, VarValue_T *pLow,
                      VarValue_T *pHigh);

/* 返回码 */
#define VAR_RUNTIME_OK               0
#define VAR_RUNTIME_ERR_TYPE        -1
#define VAR_RUNTIME_ERR_BOUND       -2
#define VAR_RUNTIME_ERR_NOT_FOUND   -3
#define VAR_RUNTIME_ERR_MEMORY      -4
#define VAR_RUNTIME_ERR_RANGE       -5

/* 强类型读取：成功后才写输出参数，源变量保持不变 */
int32_t Var_GetInt8(const VarValue_T *pValue, int8_t *pResult);
int32_t Var_GetUint8(const VarValue_T *pValue, uint8_t *pResult);
int32_t Var_GetInt16(const VarValue_T *pValue, int16_t *pResult);
int32_t Var_GetUint16(const VarValue_T *pValue, uint16_t *pResult);
int32_t Var_GetInt32(const VarValue_T *pValue, int32_t *pResult);
int32_t Var_GetUint32(const VarValue_T *pValue, uint32_t *pResult);
int32_t Var_GetInt64(const VarValue_T *pValue, int64_t *pResult);
int32_t Var_GetUint64(const VarValue_T *pValue, uint64_t *pResult);
int32_t Var_GetFloat(const VarValue_T *pValue, float *pResult);
int32_t Var_GetDouble(const VarValue_T *pValue, double *pResult);
int32_t Var_GetChar(const VarValue_T *pValue, char *pResult);
int32_t Var_GetString(const VarValue_T *pValue, const char **ppResult);

/* 按类型模板构造转换后的独立值，支持简单类型、数组、字典与结构体 */
int32_t Var_ConvertValue(const VarValue_T *pSource,
                         const VarValue_T *pTypeTemplate,
                         VarValue_T **ppResult);

/* 切片：字符串/数组共用，start 可为负数，end 可为负数或大于长度 */
int32_t Var_Slice(VarValue_T *pValue,
                  int64_t start,
                  int64_t end,
                  VarValue_T **ppOut);

/* 拼接：只有同为字符串或同为数组才允许 */
int32_t Var_Concat(VarValue_T *pLeft,
                   VarValue_T *pRight,
                   VarValue_T **ppOut);

/* 类型检查工具 */
int32_t Var_CanConcat(VarValue_T *pLeft, VarValue_T *pRight);
const char *Var_TypeName(VarType_T type);

#ifdef __cplusplus
}
#endif

#endif /* __VARIABLE_RUNTIME_H */
'''

# 中文注释：variable_runtime.c 的完整实现，生成时同样嵌入
VARIABLE_RUNTIME_SOURCE = r'''/**
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
'''

# 中文注释：嵌入版本移除对 variable_runtime.h 的外部 include，改为行内注释
VARIABLE_RUNTIME_SOURCE_EMBEDDED = VARIABLE_RUNTIME_SOURCE.replace(
    '#include "variable_runtime.h"',
    '/* 中文注释：variable_runtime.h 已内嵌于上方，无需再引用外部头文件 */'
)


# 与 parser.py 保持一致的标量类型
GRAMMAR_SCALAR_TYPES = {
    "int8", "uint8", "char", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "float", "double", "string", "int",
}

# 文本类型名 -> 内部规范名
GRAMMAR_TYPE_ALIAS = {
    "int": "int32",
    "uint": "uint32",
    "int8_t": "int8",
    "uint8_t": "uint8",
    "int16_t": "int16",
    "uint16_t": "uint16",
    "int32_t": "int32",
    "uint32_t": "uint32",
    "int64_t": "int64",
    "uint64_t": "uint64",
}

# 默认测试文法
DEFALUT_LOAD_TEXT = r'''
%mode lalr
%define PLUS "+"
%define MINUS "-"
%define STAR "*"
%define SLASH "/"
%define PERCENT "%"
%define LPAREN "("
%define RPAREN ")"
%start expr

%var X int32 = 0
%var Z float = 1.5
%var table int32[16] = { 1, 2, 3 }        // 不足的长度自动补零
%var pid struct {
    kp float
    ki float
    kd float
    out int32
} = { kp: 1.0, ki: 0.1, out: 100 }
%var grid int16[4][3]
%var config dict[int32] = { mode: 1, calib: 5 }

expr := expr PLUS term       { $$ = $1 + $3; }
expr := expr MINUS term      { $$ = $1 - $3; }
expr := term                 { $$ = $1; }

term := term STAR factor     { $$ = $1 * $3; }
term := term SLASH factor    { $$ = $1 / $3; }
term := term PERCENT factor  { $$ = $1 % $3; }
term := factor               { $$ = $1; }

factor := NUMBER             { $$ = $1; }
factor := VARIABLE           { $$ = [$1]; }
factor := LPAREN expr RPAREN { $$ = $2; }
'''

# 文本词法器始终提供的基础终结符
DEFAULT_TOKEN_NAMES = ["STRING", "NUMBER", "VARIABLE"]


class GrammarLoadError(ValueError):
    """.gram 语法错误。"""


def _decode_string_literal(text):
    """把 C 风格字符串/字符字面量转成 Python 字符串。"""
    body = text[1:-1]
    result = []
    index = 0
    escape_map = {
        "n": "\n", "r": "\r", "t": "\t", "0": "\0",
        "\\": "\\", '"': '"', "'": "'",
    }
    while index < len(body):
        ch = body[index]
        if ch == "\\" and index + 1 < len(body):
            nxt = body[index + 1]
            result.append(escape_map.get(nxt, nxt))
            index += 2
        else:
            result.append(ch)
            index += 1
    return "".join(result)


def _decode_regex_literal(text):
    """解析 %regex / %identifier 的正则字面量。

    只把 `\\\\` 还原成单个反斜杠、`\\"` 还原成引号，其余转义（`\\d`、`\\*`、
    `\\n`……）原样交给正则解析器，所以下面两种写法都表示“星号字面量”：
      %regex MUL "\\*"      %regex MUL "\\\\*"
    """
    body = text[1:-1]
    result = []
    index = 0
    while index < len(body):
        ch = body[index]
        if (ch == "\\") and (index + 1 < len(body)):
            nxt = body[index + 1]
            if nxt in ('"', "'", "\\"):
                result.append(nxt)
                index += 2
                continue
        result.append(ch)
        index += 1
    return "".join(result)


class _GramTokenizer:
    """把 .gram 文本切成 token；注释被跳过，但保留换行以便指令按行结束。"""

    _RE = re.compile(
        r"(?P<num>\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)|"
        r"(?P<str>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')|"
        r"(?P<id>[A-Za-z_%][A-Za-z0-9_]*)|"
        r"(?P<dollar>\$\$|\$\d+|\$)|"
        r"(?P<op>\:=|&&|\|\||<<|>>|<=|>=|==|!=|"
        r"[+\-*/%^&|!~<>=?:(.)\[\]{},;:])|"
        r"(?P<nl>\n)",
        re.VERBOSE
    )

    def __init__(self, text):
        self.text = text
        self.tokens = []
        self.pos = 0
        self._skip_comments_and_collect()

    def _skip_comments_and_collect(self):
        """逐字符扫描：// 与 /* */ 注释跳过，字符串整体成 token。"""
        text = self.text
        i = 0
        while i < len(text):
            ch = text[i]
            # 行注释
            if ch == "/" and i + 1 < len(text) and text[i + 1] == "/":
                while i < len(text) and text[i] != "\n":
                    i += 1
                continue
            # 块注释
            if ch == "/" and i + 1 < len(text) and text[i + 1] == "*":
                i += 2
                while i + 1 < len(text) and not (
                        text[i] == "*" and text[i + 1] == "/"):
                    i += 1
                i = min(i + 2, len(text))
                continue
            # 空白与换行
            if ch in " \t\r":
                i += 1
                continue
            if ch == "\n":
                self.tokens.append(("nl", "\n", i, i + 1))
                i += 1
                continue
            match = self._RE.match(text, i)
            if match is None:
                raise GrammarLoadError(
                    "无法识别的字符: %r (位置 %d)" % (ch, i)
                )
            kind = match.lastgroup
            self.tokens.append(
                (kind, match.group(), match.start(), match.end())
            )
            i = match.end()


class _GramParser:
    """把 token 流解析成 tokens/rules/variables 字典。"""

    def __init__(self, text):
        self.text = text
        self.tokens = _GramTokenizer(text).tokens
        self.pos = 0
        self.token_names = list(DEFAULT_TOKEN_NAMES)
        self.token_defs = {}
        self.literal_owners = {}
        self.start = None
        self.rules = {}
        self.variables = {}
        self.first_rule = None
        self.tree_enabled = False
        self.var_max_count = None
        self.mode = "ll1"
        self.mode_declared = False
        self.ast_skip = set()
        self.ast_fold = set()
        self.ast_label = set()
        # 优先级声明：token -> 级别（后声明的行更高，行内自左向右递增）
        self.precedence = {}
        # 结合性：token -> "left" / "right" / "nonassoc"
        self.associativity = {}
        # %prec 覆盖：(lhs, prod_index) -> token
        self.production_prec = {}
        self.expect_conflicts = None
        self._precedence_level = 0
        # %regex：Token 名 -> 简单正则；%identifier：标识符正则
        self.regex_defs = {}
        self.identifier_pattern = None

    # ------------------------------------------------------------------
    def _peek(self, offset=0):
        index = self.pos + offset
        if index < len(self.tokens):
            return self.tokens[index][1]
        return None

    def _peek_kind(self, offset=0):
        index = self.pos + offset
        if index < len(self.tokens):
            return self.tokens[index][0]
        return None

    def _skip_nl(self):
        while self._peek() == "\n":
            self.pos += 1

    def _accept(self, text):
        if self._peek() == text:
            self.pos += 1
            return True
        return False

    def _expect(self, text):
        if not self._accept(text):
            raise GrammarLoadError(
                "期望 '%s'，实际得到 '%s'" % (text, self._peek())
            )

    # ------------------------------------------------------------------
    def parse(self):
        """主入口：解析完整文件。"""
        while self.pos < len(self.tokens):
            self._skip_nl()
            if self.pos >= len(self.tokens):
                break
            token = self._peek()
            if token == "%token":
                self._parse_token_directive()
            elif token == "%define":
                self._parse_define_directive()
            elif token == "%start":
                self._parse_start_directive()
            elif token == "%mode":
                self._parse_mode_directive()
            elif token == "%tree":
                self._parse_tree_directive()
            elif token == "%var_max_count":
                self._parse_var_max_count_directive()
            elif token == "%var":
                self._parse_var_directive()
            elif token == "%ast_skip":
                self._parse_name_list_directive(self.ast_skip)
            elif token == "%ast_fold":
                self._parse_name_list_directive(self.ast_fold)
            elif token == "%ast_label":
                self._parse_name_list_directive(self.ast_label)
            elif token in ("%left", "%right", "%nonassoc"):
                self._parse_precedence_directive()
            elif token == "%expect":
                self._parse_expect_directive()
            elif token == "%regex":
                self._parse_regex_directive()
            elif token == "%identifier":
                self._parse_identifier_directive()
            elif (self._peek_kind() == "id" and
                  self._peek(1) == ":=" and
                  not token.startswith("%")):
                self._parse_production()
            else:
                raise GrammarLoadError(
                    "无法解析的语句起点: '%s'（第 %d 个 token）"
                    % (token, self.pos)
                )

        if self.start is None:
            if self.first_rule is None:
                raise GrammarLoadError("没有找到任何文法规则")
            self.start = self.first_rule

        # 校验：%var 预声明数量不能超过 %var_max_count 指定的容量
        if (self.var_max_count is not None) and (
                len(self.variables) > self.var_max_count):
            raise GrammarLoadError(
                "%%var_max_count 为 %d，但 %%var 预声明了 %d 个变量"
                % (self.var_max_count, len(self.variables))
            )

        # 校验：RHS 符号必须已声明为终结符或非终结符
        known = set(self.token_names) | set(self.rules.keys())
        for lhs, prods in self.rules.items():
            if lhs in self.token_names:
                raise GrammarLoadError(
                    "非终结符 %s 与终结符同名" % lhs
                )
            for prod in prods:
                for symbol in prod["rhs"]:
                    if symbol not in known:
                        raise GrammarLoadError(
                            "规则 %s 使用了未定义的符号 %s"
                            % (lhs, symbol)
                        )

        # 校验：AST 标记必须指向已定义的非终结符
        for marker_set in (self.ast_skip, self.ast_fold, self.ast_label):
            for name in sorted(marker_set):
                if name not in self.rules:
                    raise GrammarLoadError(
                        "AST 标记 %s 不是已定义的非终结符" % name
                    )

        # 校验：%prec 指定的 Token 必须已声明（%token / %define 或优先级指令）
        for lhs, prod_index in sorted(self.production_prec):
            token = self.production_prec[(lhs, prod_index)]
            if token not in self.token_names:
                raise GrammarLoadError(
                    "规则 %s 的 %%prec 引用了未声明的 Token %s"
                    % (lhs, token)
                )

        # 校验：同一个非终结符不能同时出现在多条 %ast_* 指令里
        categories = (
            ("skip", self.ast_skip),
            ("fold", self.ast_fold),
            ("label", self.ast_label),
        )
        for name in sorted(set(self.ast_skip) | set(self.ast_fold) |
                           set(self.ast_label)):
            owners = [text for text, marker_set in categories
                      if name in marker_set]
            if len(owners) > 1:
                raise GrammarLoadError(
                    "AST 标记 %s 同时出现在 %s 中"
                    % (name, " / ".join(owners))
                )

        # 校验：每条产生式都必须有语义动作，除非文法是 %tree（缺少的判空）
        if not self.tree_enabled:
            for lhs, productions in self.rules.items():
                for prod in productions:
                    if not prod.get("action", "").strip():
                        raise GrammarLoadError(
                            "规则 %s 缺少语义动作；请写动作或启用 %%tree" % lhs
                        )

        return {
            "tokens": self.token_names,
            "token_defs": self.token_defs,
            "grammar": {"start": self.start, "rules": self.rules},
            "variables": self.variables,
            "tree": self.tree_enabled,
            "var_max_count": self.var_max_count,
            "mode": self.mode,
            "mode_declared": self.mode_declared,
            "ast_skip": self.ast_skip,
            "ast_fold": self.ast_fold,
            "ast_label": self.ast_label,
            "precedence": dict(self.precedence),
            "associativity": dict(self.associativity),
            "production_prec": dict(self.production_prec),
            "expect_conflicts": self.expect_conflicts,
            "regex_defs": dict(self.regex_defs),
            "identifier_pattern": self.identifier_pattern,
        }

    # ------------------------------------------------------------------
    def _parse_token_directive(self):
        """%token A B C ...（一行一条指令，换行结束）。"""
        self.pos += 1
        while True:
            if self.pos >= len(self.tokens):
                break
            token = self._peek()
            if token == "\n":
                self.pos += 1
                break
            if token == ",":
                self.pos += 1
                continue
            if self._peek_kind() != "id" or token.startswith("%"):
                raise GrammarLoadError(
                    "%%token 只能包含标识符，实际得到 '%s'" % token
                )
            if token not in self.token_names:
                self.token_names.append(token)
            self.pos += 1

    def _parse_name_list_directive(self, target):
        """%ast_skip / %ast_fold / %ast_label 后跟一组非终结符名。

        名字之间用空格分隔；允许换行续写，列表在遇到下一条指令、产生式
        （名字后跟 :=）或文件结尾时结束。同一名字重复出现时静默去重。
        """
        keyword = self._peek()
        self.pos += 1
        names = []
        while self.pos < len(self.tokens):
            token = self._peek()
            if token == "\n":
                self.pos += 1
                continue
            if self._peek_kind() == "id" and not token.startswith("%"):
                # 下一行若是产生式（名字后跟 :=），说明名字列表已经结束
                if self._peek(1) == ":=":
                    break
                if token not in names:
                    names.append(token)
                self.pos += 1
                continue
            if token.startswith("%"):
                break
            raise GrammarLoadError(
                "%s 只能包含非终结符名，实际得到 '%s'" % (keyword, token)
            )
        if not names:
            raise GrammarLoadError(
                "%s 至少需要一个非终结符名" % keyword
            )
        target.update(names)

    def _parse_precedence_directive(self):
        """%left / %right / %nonassoc：声明 Token 的结合性与优先级。

        行内从左到右优先级递增，后声明的行比先声明的行优先级更高。
        列出的名字同时被当作 Token 声明（与 yacc 一致），所以可以只用来
        给 %prec 当伪 Token（例如 UMINUS）。
        """
        keyword = self._peek()
        self.pos += 1
        assoc = {"%left": "left", "%right": "right",
                 "%nonassoc": "nonassoc"}[keyword]
        names = []
        while self.pos < len(self.tokens):
            token = self._peek()
            if token == "\n":
                self.pos += 1
                break
            if token == ",":
                self.pos += 1
                continue
            if self._peek_kind() != "id" or token.startswith("%"):
                raise GrammarLoadError(
                    "%s 只能包含 Token 名，实际得到 '%s'" % (keyword, token)
                )
            names.append(token)
            self.pos += 1
        if not names:
            raise GrammarLoadError("%s 至少需要一个 Token 名" % keyword)
        for name in names:
            if name in self.precedence:
                raise GrammarLoadError(
                    "Token %s 的优先级重复声明" % name
                )
            self._precedence_level += 1
            self.precedence[name] = self._precedence_level
            self.associativity[name] = assoc
            if name not in self.token_names:
                self.token_names.append(name)

    def _parse_expect_directive(self):
        """%expect N：声明预期保留的未消解冲突个数。"""
        keyword = self._peek()
        self.pos += 1
        if self._peek_kind() != "num":
            raise GrammarLoadError(
                "%s 后应跟非负整数，实际得到 '%s'" % (keyword, self._peek())
            )
        value = int(float(self._peek()))
        if value < 0:
            raise GrammarLoadError("%expect 不能为负数")
        if self.expect_conflicts is not None:
            raise GrammarLoadError("%expect 只能声明一次")
        self.expect_conflicts = value
        self.pos += 1
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%expect 后有多余内容 '%s'" % self._peek()
            )

    def _read_regex_literal(self, what):
        """读取 %regex / %identifier 后面的正则字面量并做语法检查。"""
        quoted = self._peek()
        if (self._peek_kind() != "str") or (not quoted) or \
                (quoted[0] not in "\"'"):
            raise GrammarLoadError(
                '%s 后应跟引号包起来的正则，例如 "[0-9]+"' % what
            )
        self.pos += 1
        pattern = _decode_regex_literal(quoted)
        try:
            ast = _RegexParser(pattern).parse()
        except _RegexSyntaxError as error:
            raise GrammarLoadError(
                "%s 的正则 %s 有误：%s" % (what, quoted, error)
            )
        class_sets = []
        _regex_collect_class_sets(ast, class_sets)
        cells, _ = _regex_char_classes(class_sets)
        dfa = _regex_build_dfa(pattern, cells)
        if dfa["start"] in dfa["accept"]:
            raise GrammarLoadError(
                "%s 的正则 %s 可以匹配空串，Lexer 无法前进" % (what, quoted)
            )
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%s 后有多余内容 '%s'" % (what, self._peek())
            )
        return pattern

    def _parse_regex_directive(self):
        """%regex NAME "pattern"：用简单正则匹配的 Token。

        正则支持字面量、转义（\\* \\d 等）、. 、[a-z]、[^...]、()、|、*、+、?
        与 {m,n}；生成期编译成 DFA 写进 C，运行期不做正则解释。
        """
        self.pos += 1
        if self._peek_kind() != "id" or self._peek().startswith("%"):
            raise GrammarLoadError("%regex 后应跟 Token 名")
        name = self._peek()
        self.pos += 1
        pattern = self._read_regex_literal("%regex " + name)
        if (name in self.token_defs) or (name in self.regex_defs):
            raise GrammarLoadError("Token %s 重复定义" % name)
        if name not in self.token_names:
            self.token_names.append(name)
        self.regex_defs[name] = pattern

    def _parse_identifier_directive(self):
        """%identifier "pattern"：声明标识符正则。

        生成代码会用这条正则扫描标识符，并在 token 模块里生成
        Parser_TokenIsIdentifier / Parser_TokenIdentifierLength /
        Parser_TokenKeyword 三个接口。
        """
        self.pos += 1
        if self.identifier_pattern is not None:
            raise GrammarLoadError("%identifier 只能声明一次")
        self.identifier_pattern = self._read_regex_literal("%identifier")

    def _parse_define_directive(self):
        """解析固定字符串 Token；仅允许使用竖线表达多个候选。"""
        self.pos += 1
        if self._peek_kind() != "id" or self._peek().startswith("%"):
            raise GrammarLoadError("%define 后应跟 Token 名")
        token_name = self._peek()
        self.pos += 1
        if self._peek_kind() != "str" or not self._peek().startswith('\"'):
            raise GrammarLoadError("%define 规则必须使用双引号")
        literal_text = self._peek()
        self.pos += 1
        branches = self._split_define_literal(literal_text)
        if (token_name in self.token_defs) or (token_name in self.regex_defs):
            raise GrammarLoadError("Token %s 重复定义" % token_name)
        for literal in branches:
            owner = self.literal_owners.get(literal)
            if owner is not None:
                raise GrammarLoadError(
                    "字面量 %r 已由 Token %s 定义" % (literal, owner)
                )
            self.literal_owners[literal] = token_name
        self.token_defs[token_name] = branches
        if token_name not in self.token_names:
            self.token_names.append(token_name)
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%define %s 后有多余内容 '%s'"
                % (token_name, self._peek())
            )

    @staticmethod
    def _split_define_literal(text):
        """拆分 `%define` 字面量，并把 `\\|` 保留为普通竖线。"""
        escape_map = {
            "n": "\n", "r": "\r", "t": "\t", "0": "\0",
            "\\": "\\", '\"': '\"', "|": "|",
        }
        branches = []
        current = []
        body = text[1:-1]
        index = 0
        while index < len(body):
            ch = body[index]
            if ch == "\\":
                if index + 1 >= len(body):
                    raise GrammarLoadError("%define 字面量末尾存在孤立转义符")
                nxt = body[index + 1]
                current.append(escape_map.get(nxt, nxt))
                index += 2
                continue
            if ch == "|":
                literal = "".join(current)
                if literal == "":
                    raise GrammarLoadError("%define 不允许空分支")
                branches.append(literal)
                current = []
            else:
                current.append(ch)
            index += 1
        literal = "".join(current)
        if literal == "":
            raise GrammarLoadError("%define 不允许空分支")
        branches.append(literal)
        if len(set(branches)) != len(branches):
            raise GrammarLoadError("%define 内存在重复字面量")
        return branches

    def _parse_start_directive(self):
        self.pos += 1
        if self._peek_kind() != "id":
            raise GrammarLoadError("%start 后应跟起始非终结符名")
        self.start = self._peek()
        self.pos += 1
        self._skip_nl()
        if self._peek() == "\n":
            self.pos += 1

    def _parse_tree_directive(self):
        """%tree：要求生成代码附带通用语法树 API。"""
        self.pos += 1
        self.tree_enabled = True
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%tree 指令不能带参数，实际得到 '%s'" % self._peek()
            )

    def _parse_mode_directive(self):
        """%mode ll1 / %mode slr / %mode lalr：选择解析器生成模式。

        命令行 --mode 与 %mode 同时存在时必须一致（见 main()），不一致直接报错，
        不做“命令行优先”的隐式覆盖。
        """
        self.pos += 1
        token = self._peek()
        if token not in ("ll1", "slr", "lalr"):
            raise GrammarLoadError(
                "%%mode 只支持 ll1、slr 或 lalr，实际得到 '%s'" % token
            )
        if self.mode_declared:
            raise GrammarLoadError("%%mode 只能声明一次")
        self.mode = token
        self.mode_declared = True
        self.pos += 1
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%mode %s 后有多余内容 '%s'" % (token, self._peek())
            )

    def _parse_var_max_count_directive(self):
        """%var_max_count N：设置生成代码中变量表的固定容量。"""
        self.pos += 1
        if self._peek_kind() != "num":
            raise GrammarLoadError(
                "%%var_max_count 后应跟正整数，实际得到 '%s'" % self._peek()
            )
        value = int(float(self._peek()))
        self.pos += 1
        if value <= 0:
            raise GrammarLoadError("%%var_max_count 必须是正整数")
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%var_max_count %d 后有多余内容 '%s'"
                % (value, self._peek())
            )
        if self.var_max_count is not None:
            raise GrammarLoadError("%%var_max_count 只能声明一次")
        self.var_max_count = value

    # ------------------------------------------------------------------
    def _parse_var_directive(self):
        """%var 名字 类型 [= 初始值]（一条声明，可跨行写 struct/dict）。"""
        self.pos += 1
        if self._peek_kind() != "id":
            raise GrammarLoadError("%var 后应跟变量名")
        name = self._peek()
        self.pos += 1
        spec = self._parse_type()

        if self._accept("="):
            initialized = self._parse_typed_initializer(spec)
            spec = initialized

        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%var %s 声明后有多余内容 '%s'"
                % (name, self._peek())
            )
        self.variables[name] = spec

    def _parse_type(self):
        """类型表达式：标量 / struct {...} / dict[...]，后可跟数组后缀。"""
        token = self._peek()
        if self._peek_kind() != "id":
            raise GrammarLoadError("期望类型名，实际得到 '%s'" % token)

        if token == "struct":
            self.pos += 1
            spec = self._parse_struct()
        elif token == "dict":
            self.pos += 1
            self._expect("[")
            value_spec = self._parse_type()
            self._expect("]")
            spec = {"type": "dict", "value": value_spec}
        else:
            if token not in GRAMMAR_SCALAR_TYPES:
                raise GrammarLoadError(
                    "未知类型 '%s'，支持的标量：%s"
                    % (token, ", ".join(sorted(GRAMMAR_SCALAR_TYPES)))
                )
            canonical = GRAMMAR_TYPE_ALIAS.get(token, token)
            self.pos += 1
            spec = {"type": canonical}

        # 数组后缀：int32[4]、float[3][2]；先写的维度是外层
        dimensions = []
        while self._peek() == "[":
            self.pos += 1
            if self._peek_kind() != "num":
                raise GrammarLoadError("数组长度必须是正整数")
            length = int(float(self._peek()))
            if length <= 0:
                raise GrammarLoadError("数组长度必须是正整数")
            self.pos += 1
            self._expect("]")
            dimensions.append(length)
        for length in reversed(dimensions):
            spec = {"type": "array", "element": spec, "length": length}
        return spec

    def _parse_struct(self):
        """struct { 字段名 类型; ... }，字段以换行或 ; 分隔。"""
        fields = {}
        self._expect("{")
        while True:
            self._skip_nl()
            if self._accept("}"):
                break
            if self._peek_kind() != "id":
                raise GrammarLoadError(
                    "结构体字段名必须是标识符，实际得到 '%s'" % self._peek()
                )
            field_name = self._peek()
            self.pos += 1
            if field_name in fields:
                raise GrammarLoadError(
                    "结构体字段重复: %s" % field_name
                )
            fields[field_name] = self._parse_type()
            # 可选的 ; / , 分隔符
            self._accept(";")
            self._accept(",")
        return {"type": "struct", "fields": fields}

    def _parse_scalar_value(self, type_name):
        """解析 = 后面的初始值：数值、字符串或字符。"""
        self._skip_nl()
        negative = False
        if self._peek() == "-" and self._peek_kind(1) == "num":
            negative = True
            self.pos += 1
        kind = self._peek_kind()
        token = self._peek()
        if token is None:
            raise GrammarLoadError("= 后面缺少初始值")

        if kind == "num":
            text = token
            self.pos += 1
            if "." in text or "e" in text or "E" in text:
                value = float(text)
            else:
                value = int(text)
            if negative:
                value = -value
            return value

        if kind == "str":
            self.pos += 1
            if type_name not in ("char", "string"):
                raise GrammarLoadError(
                    "类型 %s 不能用字符串/字符字面量初始化" % type_name
                )
            if type_name == "char":
                text = _decode_string_literal(token)
                if len(text) != 1:
                    raise GrammarLoadError(
                        "char 初始化必须是单个字符: %s" % token
                    )
                return text
            return _decode_string_literal(token)

        raise GrammarLoadError("无法解析的初始值: '%s'" % token)

    def _parse_typed_initializer(self, target_spec):
        """解析一个与 target_spec 类型一致的初始化值。

        返回新规格（不改动模板），标量填 'value'，聚合填 'init'/'init_items'。
        """
        type_name = target_spec.get("type", "int32")
        if type_name in GRAMMAR_SCALAR_TYPES or type_name in (
                "int32", "uint8", "int8", "uint16", "int16", "uint32",
                "uint64", "int64"):
            value = self._parse_scalar_value(type_name)
            return {"type": type_name, "value": value}

        if type_name == "array":
            element_spec = target_spec["element"]
            length = int(target_spec.get("length", 0))
            self._expect("{")
            init_items = []
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                init_items.append(
                    self._parse_typed_initializer(element_spec)
                )
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            if len(init_items) > length:
                raise GrammarLoadError(
                    "数组初始化项 %d 超过声明长度 %d"
                    % (len(init_items), length)
                )
            return {
                "type": "array",
                "element": element_spec,
                "length": length,
                "init": init_items,
            }

        if type_name == "struct":
            fields = target_spec["fields"]
            init_map = {}
            self._expect("{")
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                if self._peek_kind() != "id":
                    raise GrammarLoadError(
                        "结构体初始化应写 字段名: 值，实际得到 '%s'"
                        % self._peek()
                    )
                field_name = self._peek()
                if field_name not in fields:
                    raise GrammarLoadError(
                        "结构体没有字段 %s" % field_name
                    )
                self.pos += 1
                self._expect(":")
                init_map[field_name] = self._parse_typed_initializer(
                    fields[field_name]
                )
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            return {
                "type": "struct",
                "fields": fields,
                "init": init_map,
            }

        if type_name == "dict":
            value_spec = target_spec.get("value")
            if value_spec is None:
                raise GrammarLoadError("dict 类型必须声明值类型")
            self._expect("{")
            init_items = []
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                key_kind = self._peek_kind()
                key_text = self._peek()
                if key_kind == "id":
                    self.pos += 1
                elif key_kind == "str":
                    key_text = _decode_string_literal(key_text)
                    self.pos += 1
                else:
                    raise GrammarLoadError(
                        "字典键必须是字符串字面量或标识符，实际得到 '%s'"
                        % key_text
                    )
                self._expect(":")
                value_init = self._parse_typed_initializer(value_spec)
                init_items.append((key_text, value_init))
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            return {
                "type": "dict",
                "value": value_spec,
                "init_items": init_items,
            }

        raise GrammarLoadError("不支持的初始化类型: %s" % type_name)

    # ------------------------------------------------------------------
    def _parse_production(self):
        """lhs := rhs [动作]，禁止 '|'。

        动作既可以是 { ... }，也可以是省略大括号的单行动作（读到行尾）。
        RHS 的换行只在下一行开启新规则、新指令或文件结尾时才结束，
        因此 RHS 与动作都可以跨行续写。
        """
        lhs = self._peek()
        if lhs.startswith("%"):
            raise GrammarLoadError("规则名不能以 %% 开头: %s" % lhs)
        self.pos += 1
        self._expect(":=")
        if self.first_rule is None:
            self.first_rule = lhs

        rhs = []
        prec_token = None
        while True:
            if self._peek() is None:
                break
            tok = self._peek()
            kind = self._peek_kind()

            if tok == "{":
                break
            if tok == ":=":
                raise GrammarLoadError("规则 %s 缺少语义动作" % lhs)
            if kind == "id":
                if tok == "%empty":
                    self.pos += 1
                    rhs = []
                    continue
                if tok == "%prec":
                    # %prec TOKEN 出现在右部末尾，覆盖产生式的优先级
                    self.pos += 1
                    if (self._peek_kind() != "id" or
                            self._peek().startswith("%")):
                        raise GrammarLoadError(
                            "规则 %s 的 %%prec 后应跟 Token 名" % lhs
                        )
                    prec_token = self._peek()
                    self.pos += 1
                    break
                if tok in ("def", "delete"):
                    break
                if self._peek(1) == "(":
                    break
                rhs.append(tok)
                self.pos += 1
                continue
            if tok == "\n":
                # 跳到下一行的第一个有效 token，判断是否开启新语句
                index = self.pos
                while (index < len(self.tokens) and
                       self.tokens[index][1] == "\n"):
                    index += 1
                if index >= len(self.tokens):
                    break
                next_token = self.tokens[index]
                if next_token[1].startswith("%"):
                    break
                if next_token[0] == "id" and (
                        (index + 1 >= len(self.tokens)) or
                        (self.tokens[index + 1][1] == ":=")):
                    break
                self.pos += 1
                continue
            break

        # 动作解析（缺少动作的判定推迟到 parse() 末尾，便于 %tree 文法）
        if self._peek() == "{":
            action = self._read_action_block()
        elif (self.pos >= len(self.tokens) or
              self.tokens[self.pos][1] == "\n"):
            action = ""
            if (self.pos < len(self.tokens) and
                    self.tokens[self.pos][1] == "\n"):
                self.pos += 1
        else:
            action = self._read_inline_action(lhs)

        production_index = len(self.rules.get(lhs, []))
        self.rules.setdefault(lhs, []).append({
            "rhs": rhs,
            "action": action,
        })
        if prec_token is not None:
            self.production_prec[(lhs, production_index)] = prec_token

    def _read_action_block(self):
        """读取 { ... } 动作，返回去掉外层花括号的原始文本。"""
        start_token = self.tokens[self.pos]
        if start_token[1] != "{":
            raise GrammarLoadError("期望动作 { ... }")
        depth = 0
        index = self.pos
        while index < len(self.tokens):
            kind, text = self.tokens[index][0], self.tokens[index][1]
            if kind == "op":
                if text == "{":
                    depth += 1
                elif text == "}":
                    depth -= 1
                    if depth == 0:
                        break
            index += 1
        if index >= len(self.tokens):
            raise GrammarLoadError("动作缺少右花括号 }")

        body = self.text[start_token[2] + 1:self.tokens[index][2]]
        self.pos = index + 1
        return body.strip()

    def _read_inline_action(self, lhs):
        """读取无大括号动作：从当前位置读到行尾，换行不包含在动作内。

        调用方必须保证 self.pos 指向动作的起始 token；
        本方法不做“动作是否存在”的判断。
        """
        start = self.tokens[self.pos][2]
        end_pos = self.pos
        while (end_pos < len(self.tokens) and
               self.tokens[end_pos][1] != "\n"):
            end_pos += 1
        end = self.tokens[end_pos - 1][3]
        body = self.text[start:end].strip()

        self.pos = end_pos
        if (self.pos < len(self.tokens) and
                self.tokens[self.pos][1] == "\n"):
            self.pos += 1
        return body


def load_grammar_text(text):
    """解析 .gram 文本，返回 parser.py 的输入字典。"""
    # 容忍记事本等工具写出的 UTF-8 BOM
    if text.startswith("\ufeff"):
        text = text[1:]
    return _GramParser(text).parse()


def load_grammar_file(path):
    """读取并解析 .gram 文件。"""
    with io.open(path, "r", encoding="utf-8") as file_handle:
        return load_grammar_text(file_handle.read())

EPS = ""


def compute_first(rules, terminals, nonterms):
    """计算每个非终结符的 First 集合（含 EPS 表示可空）。"""
    first = {n: set() for n in nonterms}
    changed = True
    while changed:
        changed = False
        for lhs, prods in rules.items():
            for prod in prods:
                rhs = prod["rhs"]
                index = 0
                while index < len(rhs):
                    sym = rhs[index]
                    if sym in terminals:
                        if sym not in first[lhs]:
                            first[lhs].add(sym)
                            changed = True
                        break
                    # 非终结符
                    before = len(first[lhs])
                    first[lhs] |= (first[sym] - {EPS})
                    if len(first[lhs]) != before:
                        changed = True
                    if EPS not in first[sym]:
                        break
                    index += 1
                else:
                    if EPS not in first[lhs]:
                        first[lhs].add(EPS)
                        changed = True
    return first


def compute_follow(start, rules, terminals, nonterms, first):
    """计算每个非终结符的 Follow 集合。"""
    follow = {n: set() for n in nonterms}
    follow[start].add("END")
    changed = True
    while changed:
        changed = False
        for lhs, prods in rules.items():
            for prod in prods:
                rhs = prod["rhs"]
                for i, sym in enumerate(rhs):
                    if sym not in nonterms:
                        continue
                    suffix = rhs[i + 1:]
                    suffix_first = set()
                    nullable = True
                    for s in suffix:
                        if s in terminals:
                            suffix_first.add(s)
                            nullable = False
                            break
                        suffix_first |= (first[s] - {EPS})
                        if EPS not in first[s]:
                            nullable = False
                            break
                    before = len(follow[sym])
                    follow[sym] |= suffix_first
                    if nullable:
                        follow[sym] |= follow[lhs]
                    if len(follow[sym]) != before:
                        changed = True
    return follow


def select_set(prod, lhs, rules, terminals, nonterms, first, follow):
    """LL(1) 选择集。"""
    rhs = prod["rhs"]
    result = set()
    nullable = True
    for sym in rhs:
        if sym in terminals:
            result.add(sym)
            nullable = False
            break
        result |= (first[sym] - {EPS})
        if EPS not in first[sym]:
            nullable = False
            break
    if nullable:
        result |= follow[lhs]
    return result


def _eliminate_direct_left_recursion(grammar):
    """消除直接左递归，并保留后续生成语义折叠所需的元数据。"""
    source_rules = grammar["rules"]
    final_rules = {}
    for lhs, productions in source_rules.items():
        recursive = [
            prod for prod in productions
            if prod.get("rhs") and prod["rhs"][0] == lhs
        ]
        base = [prod for prod in productions if prod not in recursive]
        if not recursive:
            final_rules[lhs] = [dict(prod) for prod in productions]
            continue
        if not base:
            raise ValueError(
                "文法错误：%s 只有直接左递归产生式，缺少基础产生式" % lhs
            )
        tail = lhs + "__tail"
        while tail in source_rules or tail in final_rules:
            tail += "_"
        final_rules[lhs] = []
        for prod in base:
            transformed = dict(prod)
            transformed["rhs"] = list(prod.get("rhs", [])) + [tail]
            transformed["transform"] = "left_base"
            transformed["base_rhs_length"] = len(prod.get("rhs", []))
            final_rules[lhs].append(transformed)
        final_rules[tail] = []
        for prod in recursive:
            transformed = dict(prod)
            transformed["rhs"] = list(prod["rhs"][1:]) + [tail]
            transformed["transform"] = "left_continue"
            transformed["owner"] = lhs
            final_rules[tail].append(transformed)
        final_rules[tail].append({
            "rhs": [],
            "action": "",
            "transform": "left_empty",
            "owner": lhs,
        })
    return {"start": grammar["start"], "rules": final_rules}


def _check_indirect_left_recursion(rules):
    """检查含可空前缀的左角图，拒绝间接左递归。"""
    nonterms = set(rules)
    nullable = set()
    changed = True

    # 先求可空非终结符闭包，避免漏掉 b 可空时 a := b c 的左角 c。
    while changed:
        changed = False
        for lhs, productions in rules.items():
            for prod in productions:
                rhs = prod.get("rhs", [])
                if all(symbol in nullable for symbol in rhs):
                    if lhs not in nullable:
                        nullable.add(lhs)
                        changed = True
                    break

    # 每条产生式沿可空前缀收集所有可能成为最左符号的非终结符。
    graph = {name: [] for name in rules}
    for lhs, productions in rules.items():
        for prod in productions:
            rhs = prod.get("rhs", [])
            for symbol in rhs:
                if symbol not in nonterms:
                    break
                graph[lhs].append(symbol)
                if symbol not in nullable:
                    break

    def visit(name, path):
        """深度优先检查当前非终结符是否回到访问路径。"""
        if name in path:
            cycle = path[path.index(name):] + [name]
            raise ValueError(
                "文法包含间接左递归：%s" % " -> ".join(cycle)
            )
        for target in graph[name]:
            visit(target, path + [name])

    for nonterm in rules:
        visit(nonterm, [])


def prepare_grammar(grammar, tokens):
    """生成最终文法、FIRST/FOLLOW 集合与非递归预测分析表。"""
    final_grammar = _eliminate_direct_left_recursion(grammar)
    rules = final_grammar["rules"]
    _check_indirect_left_recursion(rules)
    terminals = list(dict.fromkeys(
        list(DEFAULT_TOKEN_NAMES) + list(tokens) + ["END"]
    ))
    nonterms = list(rules.keys())
    first = compute_first(rules, terminals, nonterms)
    follow = compute_follow(
        final_grammar["start"], rules, terminals, nonterms, first
    )
    productions = []
    table = {}
    for lhs, alternatives in rules.items():
        for prod in alternatives:
            prod_id = len(productions)
            entry = dict(prod)
            entry["id"] = prod_id
            entry["lhs"] = lhs
            productions.append(entry)
            selected = select_set(
                entry, lhs, rules, terminals, nonterms, first, follow
            )
            for token in selected:
                key = (lhs, token)
                if key in table:
                    previous = productions[table[key]]
                    raise ValueError(
                        "文法不是 LL(1)：M[%s, %s] 同时对应 %s -> %s 和 %s -> %s"
                        % (lhs, token, previous["lhs"],
                           " ".join(previous["rhs"]) or "%empty",
                           lhs, " ".join(entry["rhs"]) or "%empty")
                    )
                table[key] = prod_id
    return {
        "grammar": final_grammar,
        "terminals": terminals,
        "nonterms": nonterms,
        "first": first,
        "follow": follow,
        "productions": productions,
        "table": table,
    }


SLR_START_SYMBOL = "__slr_start"
LALR_START_SYMBOL = "__lalr_start"


def _slr_augment_grammar(grammar):
    """新增虚拟起始规则 __slr_start := <原始 start>。

    这样起始状态只包含“待展开的起始项”，不会一上来就出现可归约项，
    接受动作也能和普通归约区分开。
    """
    rules = {
        SLR_START_SYMBOL: [{"rhs": [grammar["start"]], "action": ""}]
    }
    for lhs, productions in grammar["rules"].items():
        rules[lhs] = [dict(prod) for prod in productions]
    return {"start": SLR_START_SYMBOL, "rules": rules}


def _slr_production_ids(rules):
    """给摊平后的产生式编号：{(lhs, 规则内下标): 全局编号}。

    全局编号顺序与 generate_parser_c 里 productions 的顺序一致，
    也是 C 侧 ACTION 归约项引用的编号。
    """
    ids = {}
    for lhs, productions in rules.items():
        for index in range(len(productions)):
            ids[(lhs, index)] = len(ids)
    return ids


def _slr_closure(items, rules, nonterms):
    """LR(0) 项集闭包。item = (lhs, prod_index, dot)。

    prod_index 是 rules[lhs] 内的下标；dot 是圆点位置。
    """
    nonterm_set = set(nonterms)
    result = set(items)
    pending = list(items)
    while pending:
        lhs, prod_index, dot = pending.pop()
        rhs = rules[lhs][prod_index]["rhs"]
        if dot >= len(rhs):
            continue
        symbol = rhs[dot]
        if symbol not in nonterm_set:
            continue
        for index in range(len(rules[symbol])):
            item = (symbol, index, 0)
            if item not in result:
                result.add(item)
                pending.append(item)
    return frozenset(result)


def _slr_goto(items, symbol, rules, nonterms):
    """GOTO 转移：圆点后是 symbol 的项前移一位后取闭包。"""
    moved = set()
    for lhs, prod_index, dot in items:
        rhs = rules[lhs][prod_index]["rhs"]
        if (dot < len(rhs)) and (rhs[dot] == symbol):
            moved.add((lhs, prod_index, dot + 1))
    if not moved:
        return frozenset()
    return _slr_closure(moved, rules, nonterms)


def _slr_build_automaton(rules, nonterms, start):
    """构造 LR(0) 项集族（广度优先，状态编号稳定）。

    返回 {"states": [frozenset(items), ...],
          "transitions": {(state_id, symbol): state_id, ...}}
    """
    start_state = _slr_closure({(start, 0, 0)}, rules, nonterms)
    states = [start_state]
    state_ids = {start_state: 0}
    transitions = {}
    pending = [0]
    while pending:
        state_id = pending.pop(0)
        symbols = []
        for lhs, prod_index, dot in sorted(states[state_id]):
            rhs = rules[lhs][prod_index]["rhs"]
            if dot >= len(rhs):
                continue
            symbol = rhs[dot]
            if symbol not in symbols:
                symbols.append(symbol)
        for symbol in symbols:
            target = _slr_goto(states[state_id], symbol, rules, nonterms)
            if not target:
                continue
            if target not in state_ids:
                state_ids[target] = len(states)
                states.append(target)
                pending.append(state_ids[target])
            transitions[(state_id, symbol)] = state_ids[target]
    return {"states": states, "transitions": transitions}


def _slr_item_text(item, rules):
    """把项 (lhs, prod_index, dot) 渲染成 lhs -> a . b 的可读文本。"""
    lhs, prod_index, dot = item
    rhs = list(rules[lhs][prod_index]["rhs"])
    rhs.insert(min(dot, len(rhs)), ".")
    return "(%s -> %s)" % (lhs, " ".join(rhs))


def _slr_state_text(state_items, rules):
    """把整个项集渲染成一行文本，用于冲突诊断。"""
    return "[%s]" % ", ".join(
        _slr_item_text(item, rules) for item in sorted(state_items)
    )


def _slr_action_text(action, rules, production_ids):
    """把 ACTION 表项渲染成可读文本。"""
    kind, value = action
    if kind == "shift":
        return "移进到状态 %d" % value
    if kind == "accept":
        return "接受"
    for (lhs, index), prod_id in production_ids.items():
        if prod_id == value:
            rhs = " ".join(rules[lhs][index]["rhs"]) or "%empty"
            return "按产生式 %d（%s -> %s）归约" % (prod_id, lhs, rhs)
    return "按产生式 %d 归约" % value


def _slr_conflict_text(previous, value):
    """按新旧动作类型给出冲突种类文本。"""
    kinds = {previous[0], value[0]}
    if ("shift" in kinds) and ("reduce" in kinds):
        return "移进/归约冲突"
    if kinds == {"reduce"}:
        return "归约/归约冲突"
    if kinds == {"shift"}:
        return "移进/移进冲突"
    return "接受动作冲突"


# =====================================================================
# 优先级与冲突消解（SLR(1) / LALR(1) 共用）
#
# 默认行为不变：文法不写 %left / %right / %nonassoc / %prec 时，冲突仍然
# 在生成期报错。只有当冲突双方都能算出优先级（移进 Token 有优先级，且归约
# 产生式能取到优先级）时才按 yacc 规则消解，并把消解理由记录下来。
# =====================================================================


def _priority_config(precedence=None, associativity=None,
                     production_prec=None, expect_conflicts=None):
    """把优先级元数据打包成一个字典，便于一路传到表构造。"""
    return {
        "precedence": dict(precedence or {}),
        "associativity": dict(associativity or {}),
        "production_prec": dict(production_prec or {}),
        "expect_conflicts": expect_conflicts,
    }


def _priority_from_cfg(cfg):
    """从 load_grammar_text 返回的 cfg 里取出优先级元数据。"""
    return _priority_config(
        cfg.get("precedence"), cfg.get("associativity"),
        cfg.get("production_prec"), cfg.get("expect_conflicts")
    )


def _lex_spec_from_cfg(cfg):
    """从 load_grammar_text 返回的 cfg 里取出 %regex / %identifier 声明。"""
    return {
        "regex": dict(cfg.get("regex_defs") or {}),
        "identifier": cfg.get("identifier_pattern"),
    }


def _normalize_lex_spec(lex_spec):
    """把 None / dict 统一成 {"regex": {...}, "identifier": 正则或 None}。"""
    if not lex_spec:
        return {"regex": {}, "identifier": None}
    return {
        "regex": dict(lex_spec.get("regex") or {}),
        "identifier": lex_spec.get("identifier"),
    }


def _normalize_priority(priority):
    """把 None / dict 统一成四键优先级配置。"""
    if priority is None:
        return _priority_config()
    return _priority_config(
        priority.get("precedence"), priority.get("associativity"),
        priority.get("production_prec"), priority.get("expect_conflicts")
    )


def _compute_production_priority(rules, precedence, associativity,
                                 production_prec):
    """算出每条产生式的 (结合性, 优先级)。

    优先级来源：%prec 指定的 Token；否则取右部最右终结符。
    取不到优先级（或该 Token 没声明过优先级）的产生式不会出现在结果里，
    它们在冲突里无法按优先级消解。返回 {(lhs, prod_index): (assoc, level)}。
    """
    nonterms = set(rules)
    result = {}
    for lhs, productions in rules.items():
        for prod_index, prod in enumerate(productions):
            token = production_prec.get((lhs, prod_index))
            if token is None:
                for symbol in reversed(prod["rhs"]):
                    if symbol not in nonterms:
                        token = symbol
                        break
            if token is None:
                continue
            level = precedence.get(token)
            if level is None:
                continue
            result[(lhs, prod_index)] = (
                associativity.get(token, "left"), level
            )
    return result


def _try_resolve_shift_reduce(previous, value, token,
                              precedence, associativity,
                              production_priority):
    """按优先级 / 结合性消解一个移进/归约冲突。

    previous / value：ACTION 表里已有的动作与新增动作；
    production_priority：{产生式编号: (结合性, 优先级)}；
    返回 (resolved, reason)：resolved 是 "shift" / "reduce" / None，
    None 表示无法消解（缺优先级，或 %nonassoc 不允许链式使用）。
    """
    kinds = {previous[0], value[0]}
    if kinds != {"shift", "reduce"}:
        return None, None
    reduce_action = previous if previous[0] == "reduce" else value
    reduce_id = reduce_action[1]
    shift_level = precedence.get(token)
    reduce_conf = production_priority.get(reduce_id)
    if (shift_level is None) or (reduce_conf is None):
        return None, None
    reduce_assoc, reduce_level = reduce_conf
    assoc = associativity.get(token) or reduce_assoc
    if shift_level > reduce_level:
        return "shift", ("优先级（移进 %s 优先级 %d 高于产生式 P%d 的 %d）"
                         % (token, shift_level, reduce_id, reduce_level))
    if shift_level < reduce_level:
        return "reduce", ("优先级（产生式 P%d 优先级 %d 高于移进 %s 的 %d）"
                          % (reduce_id, reduce_level, token, shift_level))
    if assoc == "left":
        return "reduce", "%%left 结合性（%s）" % token
    if assoc == "right":
        return "shift", "%%right 结合性（%s）" % token
    if assoc == "nonassoc":
        return None, "%%nonassoc 不允许链式使用（%s）" % token
    return None, None


def _make_lr_action_recorder(label, states, state_text, rules,
                             production_ids, action, priority):
    """构造写 ACTION 表的闭包：能消解就消解，不能就记成未消解冲突。

    返回 (record, resolutions, unresolved)：
      record(key, value, state_id, token)：写表项并处理冲突；
      resolutions：已消解的冲突（状态、Token、种类、理由、选择）；
      unresolved：未消解的冲突（含种类与完整说明文本）。
    未消解的移进/归约冲突按移进优先写表，交给调用方用 %expect 决定是否放行。
    """
    precedence = priority["precedence"]
    associativity = priority["associativity"]
    production_priority = {
        production_ids[key]: value
        for key, value in _compute_production_priority(
            rules, precedence, associativity, priority["production_prec"]
        ).items() if key in production_ids
    }
    resolutions = []
    unresolved = []

    def record(key, value, state_id, token):
        previous = action.get(key)
        if previous is None:
            action[key] = value
            return
        if previous == value:
            # 同一个符号由多个项产生相同动作（GOTO 是确定性的），不是冲突
            return
        kind = _slr_conflict_text(previous, value)
        resolved, reason = _try_resolve_shift_reduce(
            previous, value, token, precedence, associativity,
            production_priority
        )
        if resolved == "shift":
            action[key] = previous if previous[0] == "shift" else value
            resolutions.append((state_id, token, kind, reason, "移进"))
            return
        if resolved == "reduce":
            action[key] = previous if previous[0] == "reduce" else value
            resolutions.append((state_id, token, kind, reason, "归约"))
            return
        text = ("%s 冲突（状态 %d，Token %s）：%s；已有 %s，新增 %s；涉及项集 %s"
                % (label, state_id, token, kind,
                   _slr_action_text(previous, rules, production_ids),
                   _slr_action_text(value, rules, production_ids),
                   state_text(states[state_id], rules)))
        if reason is not None:
            text += "；%s" % reason
        elif kind == "移进/归约冲突":
            action[key] = previous if previous[0] == "shift" else value
            text += "；没有可用的优先级，默认移进优先（可用 %expect 放行）"
        unresolved.append({
            "kind": kind, "state": state_id, "token": token, "text": text,
        })

    return record, resolutions, unresolved


def _check_unresolved_conflicts(label, unresolved, expect_conflicts):
    """%expect 语义：允许保留预期数量的移进/归约冲突，其余一律报错。

    未消解的移进/归约冲突可以靠 %expect 放行（表里已按移进优先写），
    归约/归约冲突无论 %expect 写多少都直接报错。
    """
    if not unresolved:
        return
    for item in unresolved:
        if item["kind"] == "归约/归约冲突":
            raise GrammarLoadError(item["text"])
    if expect_conflicts is None:
        raise GrammarLoadError(unresolved[0]["text"])
    if len(unresolved) != expect_conflicts:
        raise GrammarLoadError(
            "%s：%%expect %d 与实际未消解冲突数 %d 不一致；%s"
            % (label, expect_conflicts, len(unresolved),
               unresolved[0]["text"])
        )
    warnings.warn(
        "%s：%d 个未消解冲突与 %%expect %d 一致，"
        "移进/归约冲突按移进优先继续生成；%s"
        % (label, len(unresolved), expect_conflicts,
           "；".join(item["text"] for item in unresolved))
    )


def _slr_build_tables(automaton, rules, nonterms, terminals,
                      first, follow, start, priority=None):
    """构造 ACTION / GOTO 表。

    返回 {"action": {(state, terminal): ("shift", s) | ("reduce", p)
                                        | ("accept", None)},
          "goto": {(state, nonterm): s},
          "conflicts": [未消解冲突说明文本, ...],
          "unresolved": [{kind, state, token, text}, ...],
          "resolutions": [(state, token, 种类, 理由, 选择), ...]}
    移进/归约冲突先按 %left / %right / %nonassoc / %prec 消解；
    剩余的交给调用方（%expect）决定放行还是报错。
    """
    terminal_set = set(terminals)
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    production_ids = _slr_production_ids(rules)
    states = automaton["states"]
    transitions = automaton["transitions"]
    action = {}
    goto = {}
    record, resolutions, unresolved = _make_lr_action_recorder(
        "SLR", states, _slr_state_text, rules, production_ids, action,
        _normalize_priority(priority)
    )

    for state_id, state_items in enumerate(states):
        for item in sorted(state_items):
            lhs, prod_index, dot = item
            rhs = rules[lhs][prod_index]["rhs"]
            if dot < len(rhs):
                symbol = rhs[dot]
                if symbol in terminal_set:
                    target = transitions.get((state_id, symbol))
                    if target is not None:
                        record((state_id, symbol), ("shift", target),
                               state_id, symbol)
                continue
            if lhs == start:
                record((state_id, "END"), ("accept", None), state_id, "END")
                continue
            for token in sorted(follow[lhs]):
                record((state_id, token),
                       ("reduce", production_ids[(lhs, prod_index)]),
                       state_id, token)
    for (state_id, symbol), target in transitions.items():
        if symbol not in terminal_set:
            goto[(state_id, nonterm_ids[symbol])] = target
    return {
        "action": action,
        "goto": goto,
        "conflicts": [item["text"] for item in unresolved],
        "unresolved": unresolved,
        "resolutions": resolutions,
    }


def _slr_analyze(grammar, tokens, priority=None):
    """SLR(1) 模式的分析入口：构造 LR(0) 自动机与 ACTION / GOTO 表。

    与 prepare_grammar 不同，这里不做左递归消除，也不做 LL(1) 检查；
    移进/归约冲突先按 %left / %right / %nonassoc / %prec 消解；
    未消解的冲突按 %expect 处理，没有 %expect 或数量不符时抛
    GrammarLoadError（不产出 C 代码）。
    """
    priority = _normalize_priority(priority)
    augmented = _slr_augment_grammar(grammar)
    rules = augmented["rules"]
    start = augmented["start"]
    terminals = list(dict.fromkeys(
        list(DEFAULT_TOKEN_NAMES) + list(tokens) + ["END"]
    ))
    nonterms = list(rules.keys())
    first = compute_first(rules, terminals, nonterms)
    follow = compute_follow(start, rules, terminals, nonterms, first)
    automaton = _slr_build_automaton(rules, nonterms, start)
    tables = _slr_build_tables(
        automaton, rules, nonterms, terminals, first, follow, start, priority
    )
    _check_unresolved_conflicts("SLR(1)", tables["unresolved"],
                                priority["expect_conflicts"])
    productions = []
    for lhs, alternatives in rules.items():
        for prod in alternatives:
            entry = dict(prod)
            entry["id"] = len(productions)
            entry["lhs"] = lhs
            productions.append(entry)
    return {
        "mode": "slr",
        "grammar": augmented,
        "terminals": terminals,
        "nonterms": nonterms,
        "first": first,
        "follow": follow,
        "productions": productions,
        "states": automaton["states"],
        "transitions": automaton["transitions"],
        "action": tables["action"],
        "goto": tables["goto"],
        "conflicts": tables["conflicts"],
        "unresolved": tables["unresolved"],
        "resolutions": tables["resolutions"],
        "priority": priority,
    }


# =====================================================================
# LALR(1) 表构造（阶段 2）
#
# LALR(1) 复用 SLR(1) 的 LR 运行时、栈结构、ACTION / GOTO 编码与动作翻译，
# 唯一区别在“表构造”这一层：归约动作的 lookahead 不再取 FOLLOW(lhs)，
# 而是取“同心 LR(1) 项集合并后”的精确 lookahead 集合。因此：
#   1. 先构造 LR(1) 项集族（项 = (lhs, prod_index, dot, lookahead)）；
#   2. 再按 core（前三个分量）合并同心项集，并集各自的 lookahead；
#   3. 基于合并结果构造 ACTION / GOTO，归约只对项集内出现的 lookahead 生效。
# 与 SLR 一样：不消解冲突，冲突一律抛 GrammarLoadError。
# =====================================================================


def _lalr_augment_grammar(grammar):
    """新增虚拟起始规则 __lalr_start := <原始 start>。

    作用与 SLR 的 __slr_start 相同：让起始状态只含“待展开的起始项”，
    避免一上来就出现可归约项，从而把接受动作与普通归约区分开。
    """
    rules = {
        LALR_START_SYMBOL: [{"rhs": [grammar["start"]], "action": ""}]
    }
    for lhs, productions in grammar["rules"].items():
        rules[lhs] = [dict(prod) for prod in productions]
    return {"start": LALR_START_SYMBOL, "rules": rules}


def _lr1_first_of_sequence(sequence, nonterms, first):
    """计算 FIRST(sequence)，返回 (终结符集合, 是否可空)。

    用于 LR(1) 闭包里的 FIRST(beta a)：beta 是圆点后的剩余右部，
    a 是当前项的 lookahead，因此整体一定非空。
    不在 nonterms 里的符号即终结符（文法符号不是非终结符就是终结符）。
    """
    nonterm_set = set(nonterms)
    result = set()
    for symbol in sequence:
        if symbol not in nonterm_set:
            result.add(symbol)
            return result, False
        result |= (first[symbol] - {EPS})
        if EPS not in first[symbol]:
            return result, False
    return result, True


def _lr1_closure(items, rules, nonterms, first):
    """LR(1) 项集闭包。item = (lhs, prod_index, dot, lookahead)。

    对形如 [A -> alpha . B beta, a] 的项，为每条 B -> gamma 与每个
    b ∈ FIRST(beta a) 加入 [B -> . gamma, b]。
    """
    nonterm_set = set(nonterms)
    result = set(items)
    pending = list(items)
    while pending:
        lhs, prod_index, dot, lookahead = pending.pop()
        rhs = rules[lhs][prod_index]["rhs"]
        if dot >= len(rhs):
            continue
        symbol = rhs[dot]
        if symbol not in nonterm_set:
            continue
        suffix = list(rhs[dot + 1:]) + [lookahead]
        lookaheads, _ = _lr1_first_of_sequence(suffix, nonterm_set, first)
        for index in range(len(rules[symbol])):
            for token in lookaheads:
                item = (symbol, index, 0, token)
                if item not in result:
                    result.add(item)
                    pending.append(item)
    return frozenset(result)


def _lr1_goto(items, symbol, rules, nonterms, first):
    """LR(1) GOTO 转移：圆点后是 symbol 的项前移一位后取 LR(1) 闭包。"""
    moved = set()
    for lhs, prod_index, dot, lookahead in items:
        rhs = rules[lhs][prod_index]["rhs"]
        if (dot < len(rhs)) and (rhs[dot] == symbol):
            moved.add((lhs, prod_index, dot + 1, lookahead))
    if not moved:
        return frozenset()
    return _lr1_closure(moved, rules, nonterms, first)


def _lr1_build_automaton(rules, nonterms, start, first):
    """构造 LR(1) 项集族（广度优先，状态编号稳定）。

    返回 {"states": [frozenset(items), ...],
          "transitions": {(state_id, symbol): state_id, ...}}
    """
    start_state = _lr1_closure({(start, 0, 0, "END")}, rules, nonterms, first)
    states = [start_state]
    state_ids = {start_state: 0}
    transitions = {}
    pending = [0]
    while pending:
        state_id = pending.pop(0)
        symbols = []
        for lhs, prod_index, dot, lookahead in sorted(states[state_id]):
            rhs = rules[lhs][prod_index]["rhs"]
            if dot >= len(rhs):
                continue
            symbol = rhs[dot]
            if symbol not in symbols:
                symbols.append(symbol)
        for symbol in symbols:
            target = _lr1_goto(
                states[state_id], symbol, rules, nonterms, first
            )
            if not target:
                continue
            if target not in state_ids:
                state_ids[target] = len(states)
                states.append(target)
                pending.append(state_ids[target])
            transitions[(state_id, symbol)] = state_ids[target]
    return {"states": states, "transitions": transitions}


def _lalr_merge_states(lr1_states):
    """合并同心项集，返回 (LALR 状态列表, LR(1) -> LALR 状态映射)。

    LALR 状态用 {core: frozenset(lookaheads)} 表示，core = (lhs, prod, dot)；
    相同 core 的 LR(1) 状态合并为同一状态，lookahead 集合取并集。
    """
    core_ids = {}
    states = []
    state_map = []
    for lr1_state in lr1_states:
        core = frozenset(
            (lhs, prod_index, dot)
            for lhs, prod_index, dot, _lookahead in lr1_state
        )
        merged = {}
        for lhs, prod_index, dot, lookahead in lr1_state:
            key = (lhs, prod_index, dot)
            merged.setdefault(key, set()).add(lookahead)
        if core not in core_ids:
            core_ids[core] = len(states)
            states.append(
                {key: frozenset(tokens) for key, tokens in merged.items()}
            )
        else:
            target = states[core_ids[core]]
            for key, tokens in merged.items():
                target[key] = frozenset(set(target[key]) | tokens)
        state_map.append(core_ids[core])
    return states, state_map


def _lalr_state_text(state_items, rules):
    """把 LALR 项集渲染成一行文本（每个 core 带 lookahead 集合）。"""
    parts = []
    for (lhs, prod_index, dot), lookaheads in sorted(state_items.items()):
        rhs = list(rules[lhs][prod_index]["rhs"])
        rhs.insert(min(dot, len(rhs)), ".")
        parts.append(
            "(%s -> %s, {%s})"
            % (lhs, " ".join(rhs), ", ".join(sorted(lookaheads)))
        )
    return "[%s]" % ", ".join(parts)


def _lalr_build_tables(automaton, rules, nonterms, terminals, first, start,
                       priority=None):
    """基于合并后的 LALR 项集构造 ACTION / GOTO 表。

    返回结构与 _slr_build_tables 完全一致（含 resolutions / unresolved）：
    与 SLR 的唯一区别是归约 lookahead 取项集里携带的集合，而不是 FOLLOW(lhs)；
    冲突消解复用同一个 _make_lr_action_recorder。
    """
    terminal_set = set(terminals)
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    production_ids = _slr_production_ids(rules)
    states = automaton["states"]
    transitions = automaton["transitions"]
    action = {}
    goto = {}
    record, resolutions, unresolved = _make_lr_action_recorder(
        "LALR", states, _lalr_state_text, rules, production_ids, action,
        _normalize_priority(priority)
    )

    for state_id, state_items in enumerate(states):
        for (lhs, prod_index, dot), lookaheads in sorted(state_items.items()):
            rhs = rules[lhs][prod_index]["rhs"]
            if dot < len(rhs):
                symbol = rhs[dot]
                if symbol in terminal_set:
                    target = transitions.get((state_id, symbol))
                    if target is not None:
                        record((state_id, symbol), ("shift", target),
                               state_id, symbol)
                continue
            if lhs == start:
                record((state_id, "END"), ("accept", None), state_id, "END")
                continue
            for token in sorted(lookaheads):
                record((state_id, token),
                       ("reduce", production_ids[(lhs, prod_index)]),
                       state_id, token)
    for (state_id, symbol), target in transitions.items():
        if symbol not in terminal_set:
            goto[(state_id, nonterm_ids[symbol])] = target
    return {
        "action": action,
        "goto": goto,
        "conflicts": [item["text"] for item in unresolved],
        "unresolved": unresolved,
        "resolutions": resolutions,
    }


def _lalr_analyze(grammar, tokens, priority=None):
    """LALR(1) 模式的分析入口：LR(1) 项集 + 同心合并 + ACTION / GOTO 表。

    与 SLR 一样不做左递归消除，也不做 LL(1) 检查；冲突消解与 %expect 处理
    也完全一致（复用同一内部函数）。返回结构与 _slr_analyze 对齐，便于
    generate_parser_c 复用同一套 LR 运行时。
    """
    priority = _normalize_priority(priority)
    augmented = _lalr_augment_grammar(grammar)
    rules = augmented["rules"]
    start = augmented["start"]
    terminals = list(dict.fromkeys(
        list(DEFAULT_TOKEN_NAMES) + list(tokens) + ["END"]
    ))
    nonterms = list(rules.keys())
    first = compute_first(rules, terminals, nonterms)
    lr1 = _lr1_build_automaton(rules, nonterms, start, first)
    states, state_map = _lalr_merge_states(lr1["states"])
    transitions = {}
    for (source, symbol), target in lr1["transitions"].items():
        transitions[(state_map[source], symbol)] = state_map[target]
    automaton = {"states": states, "transitions": transitions}
    tables = _lalr_build_tables(
        automaton, rules, nonterms, terminals, first, start, priority
    )
    _check_unresolved_conflicts("LALR", tables["unresolved"],
                                priority["expect_conflicts"])
    productions = []
    for lhs, alternatives in rules.items():
        for prod in alternatives:
            entry = dict(prod)
            entry["id"] = len(productions)
            entry["lhs"] = lhs
            productions.append(entry)
    return {
        "mode": "lalr",
        "grammar": augmented,
        "terminals": terminals,
        "nonterms": nonterms,
        "first": first,
        "follow": None,
        "lr1_state_count": len(lr1["states"]),
        "productions": productions,
        "states": states,
        "transitions": transitions,
        "action": tables["action"],
        "goto": tables["goto"],
        "conflicts": tables["conflicts"],
        "unresolved": tables["unresolved"],
        "resolutions": tables["resolutions"],
        "priority": priority,
    }


def format_grammar_report(analysis):
    """把最终产生式、集合和预测表格式化为稳定的教学报告。"""
    terminals = analysis["terminals"]
    order = {token: index for index, token in enumerate(terminals)}
    order[EPS] = len(order)

    def ordered(values):
        """按 Token 声明顺序排列集合元素。"""
        return sorted(values, key=lambda item: (order.get(item, 999999), item))

    lines = ["[最终产生式]"]
    for prod in analysis["productions"]:
        rhs = " ".join(prod["rhs"]) if prod["rhs"] else "%empty"
        lines.append("P%d: %s -> %s" % (prod["id"], prod["lhs"], rhs))
    lines.extend(["", "[FIRST]"])
    for name in analysis["nonterms"]:
        values = ["%empty" if item == EPS else item
                  for item in ordered(analysis["first"][name])]
        lines.append("FIRST(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[FOLLOW]"])
    for name in analysis["nonterms"]:
        values = ordered(analysis["follow"][name])
        lines.append("FOLLOW(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[预测分析表]"])
    for name in analysis["nonterms"]:
        for token in terminals:
            prod_id = analysis["table"].get((name, token))
            if prod_id is not None:
                lines.append("M[%s, %s] = P%d" % (name, token, prod_id))
    return "\n".join(lines) + "\n"


def _slr_entry_text(entry, analysis):
    """把一条 ACTION 表项渲染成可读文本。"""
    kind, value = entry
    if kind == "shift":
        return "移进 %d" % value
    if kind == "accept":
        return "接受"


def _resolution_line(entry):
    """把一条冲突消解记录渲染成一行文本。"""
    state_id, token, kind, reason, chosen = entry
    return ("状态 %d，Token %s：%s，按%s选择%s"
            % (state_id, token, kind, reason, chosen))
    prod = analysis["productions"][value]
    rhs = " ".join(prod["rhs"]) if prod["rhs"] else "%empty"
    return "归约 P%d（%s -> %s）" % (value, prod["lhs"], rhs)


def format_slr_report(analysis):
    """把 SLR(1) 的产生式、FIRST/FOLLOW 与 ACTION / GOTO 表格式化为报告。"""
    terminals = analysis["terminals"]
    order = {token: index for index, token in enumerate(terminals)}
    order[EPS] = len(order)

    def ordered(values):
        """按 Token 声明顺序排列集合元素。"""
        return sorted(values, key=lambda item: (order.get(item, 999999), item))

    lines = ["[生成模式] SLR(1)：LR(0) 项集 + FOLLOW 集合"]
    lines.extend(["", "[最终产生式]"])
    for prod in analysis["productions"]:
        rhs = " ".join(prod["rhs"]) if prod["rhs"] else "%empty"
        lines.append("P%d: %s -> %s" % (prod["id"], prod["lhs"], rhs))
    lines.extend(["", "[FIRST]"])
    for name in analysis["nonterms"]:
        values = ["%empty" if item == EPS else item
                  for item in ordered(analysis["first"][name])]
        lines.append("FIRST(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[FOLLOW]"])
    for name in analysis["nonterms"]:
        values = ordered(analysis["follow"][name])
        lines.append("FOLLOW(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[LR(0) 自动机]"])
    lines.append("状态数 = %d，GOTO 转移数 = %d"
                 % (len(analysis["states"]), len(analysis["transitions"])))
    lines.extend(["", "[ACTION 表]"])
    for state_id in range(len(analysis["states"])):
        for token in terminals:
            entry = analysis["action"].get((state_id, token))
            if entry is not None:
                lines.append("ACTION[%d, %s] = %s"
                             % (state_id, token,
                                _slr_entry_text(entry, analysis)))
    lines.extend(["", "[GOTO 表]"])
    for (state_id, nonterm_id), target in sorted(analysis["goto"].items()):
        lines.append("GOTO[%d, %s] = %d"
                     % (state_id, analysis["nonterms"][nonterm_id], target))
    lines.extend(["", "[冲突消解]"])
    resolutions = analysis.get("resolutions") or []
    if resolutions:
        for entry in resolutions:
            lines.append(_resolution_line(entry))
    else:
        lines.append("（无）")
    lines.extend(["", "[未消解冲突]"])
    unresolved = analysis.get("unresolved") or []
    if unresolved:
        for item in unresolved:
            lines.append(item["text"])
    else:
        lines.append("（无）")
    return "\n".join(lines) + "\n"


def format_slr_automaton(analysis):
    """渲染 LR(0) 状态、项集与 GOTO 转移（--dump-lr-automaton）。"""
    rules = analysis["grammar"]["rules"]
    lines = ["[LR(0) 项集族]"]
    for state_id, items in enumerate(analysis["states"]):
        lines.append("I%d:" % state_id)
        for item in sorted(items):
            lines.append("    %s" % _slr_item_text(item, rules))
    lines.extend(["", "[GOTO 转移]"])
    for (state_id, symbol), target in sorted(
            analysis["transitions"].items(),
            key=lambda pair: (pair[0][0], str(pair[0][1]))):
        lines.append("GOTO(I%d, %s) = I%d" % (state_id, symbol, target))
    return "\n".join(lines) + "\n"


def format_slr_tables(analysis):
    """渲染 ACTION / GOTO 矩阵（--dump-slr-tables）。"""
    terminals = analysis["terminals"]
    nonterms = analysis["nonterms"]
    lines = ["[ACTION 表]", "状态  " + "  ".join(terminals)]
    for state_id in range(len(analysis["states"])):
        cells = []
        for token in terminals:
            entry = analysis["action"].get((state_id, token))
            if entry is None:
                cells.append(".")
            elif entry[0] == "shift":
                cells.append("s%d" % entry[1])
            elif entry[0] == "reduce":
                cells.append("r%d" % entry[1])
            else:
                cells.append("acc")
        lines.append("%-4d  %s" % (state_id, "  ".join(cells)))
    lines.extend(["", "[GOTO 表]", "状态  " + "  ".join(nonterms)])
    for state_id in range(len(analysis["states"])):
        cells = []
        for index in range(len(nonterms)):
            target = analysis["goto"].get((state_id, index))
            cells.append("." if target is None else str(target))
        lines.append("%-4d  %s" % (state_id, "  ".join(cells)))
    return "\n".join(lines) + "\n"


def format_lalr_report(analysis):
    """把 LALR(1) 的产生式、FIRST 与 ACTION / GOTO 表格式化为报告。

    LALR(1) 的归约 lookahead 来自同心项集合并，不是 FOLLOW，因此报告里
    不打印 FOLLOW，改为打印 LR(1) 原始状态数与合并后的 LALR 状态数。
    """
    terminals = analysis["terminals"]
    order = {token: index for index, token in enumerate(terminals)}
    order[EPS] = len(order)

    def ordered(values):
        """按 Token 声明顺序排列集合元素。"""
        return sorted(values, key=lambda item: (order.get(item, 999999), item))

    lines = ["[生成模式] LALR(1)：LR(1) 项集 + 同心项集合并 lookahead"]
    lines.extend(["", "[最终产生式]"])
    for prod in analysis["productions"]:
        rhs = " ".join(prod["rhs"]) if prod["rhs"] else "%empty"
        lines.append("P%d: %s -> %s" % (prod["id"], prod["lhs"], rhs))
    lines.extend(["", "[FIRST]"])
    for name in analysis["nonterms"]:
        values = ["%empty" if item == EPS else item
                  for item in ordered(analysis["first"][name])]
        lines.append("FIRST(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[LALR 自动机]"])
    lines.append(
        "LR(1) 状态数 = %d，合并后 LALR 状态数 = %d，GOTO 转移数 = %d"
        % (analysis.get("lr1_state_count", 0), len(analysis["states"]),
           len(analysis["transitions"]))
    )
    lines.extend(["", "[ACTION 表]"])
    for state_id in range(len(analysis["states"])):
        for token in terminals:
            entry = analysis["action"].get((state_id, token))
            if entry is not None:
                lines.append("ACTION[%d, %s] = %s"
                             % (state_id, token,
                                _slr_entry_text(entry, analysis)))
    lines.extend(["", "[GOTO 表]"])
    for (state_id, nonterm_id), target in sorted(analysis["goto"].items()):
        lines.append("GOTO[%d, %s] = %d"
                     % (state_id, analysis["nonterms"][nonterm_id], target))
    lines.extend(["", "[冲突消解]"])
    resolutions = analysis.get("resolutions") or []
    if resolutions:
        for entry in resolutions:
            lines.append(_resolution_line(entry))
    else:
        lines.append("（无）")
    lines.extend(["", "[未消解冲突]"])
    unresolved = analysis.get("unresolved") or []
    if unresolved:
        for item in unresolved:
            lines.append(item["text"])
    else:
        lines.append("（无）")
    return "\n".join(lines) + "\n"


def format_lalr_automaton(analysis):
    """渲染 LALR 状态、项集（带 lookahead）与 GOTO 转移。"""
    rules = analysis["grammar"]["rules"]
    lines = ["[LALR 项集族]"]
    for state_id, items in enumerate(analysis["states"]):
        lines.append("I%d:" % state_id)
        for (lhs, prod_index, dot), lookaheads in sorted(items.items()):
            rhs = list(rules[lhs][prod_index]["rhs"])
            rhs.insert(min(dot, len(rhs)), ".")
            lines.append("    (%s -> %s, {%s})"
                         % (lhs, " ".join(rhs), ", ".join(sorted(lookaheads))))
    lines.extend(["", "[GOTO 转移]"])
    for (state_id, symbol), target in sorted(
            analysis["transitions"].items(),
            key=lambda pair: (pair[0][0], str(pair[0][1]))):
        lines.append("GOTO(I%d, %s) = I%d" % (state_id, symbol, target))
    return "\n".join(lines) + "\n"


def format_lalr_tables(analysis):
    """渲染 LALR 的 ACTION / GOTO 矩阵（--dump-slr-tables）。"""
    return format_slr_tables(analysis)


# 变量标量类型 -> VarValue 构造函数
VAR_SCALAR_CREATE = {
    "int8":   "Var_CreateInt8",
    "uint8":  "Var_CreateUint8",
    "char":   "Var_CreateChar",
    "int16":  "Var_CreateInt16",
    "uint16": "Var_CreateUint16",
    "int32":  "Var_CreateInt",
    "int":    "Var_CreateInt",
    "uint32": "Var_CreateUint32",
    "int64":  "Var_CreateInt64",
    "uint64": "Var_CreateUint64",
    "float":  "Var_CreateFloat",
    "double": "Var_CreateDouble",
    "string": "Var_CreateString",
}

# 变量标量类型的默认值（C 字面量）
VAR_SCALAR_DEFAULT = {
    "int8":   "0",
    "uint8":  "0",
    "char":   "'\\0'",
    "int16":  "0",
    "uint16": "0",
    "int32":  "0",
    "int":    "0",
    "uint32": "0u",
    "int64":  "0",
    "uint64": "0u",
    "float":  "0.0f",
    "double": "0.0",
    "string": '""',
}


def _var_c_string(value):
    """把 Python 字符串转成安全的 C 字符串字面量。"""
    return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'


def _var_scalar_code(spec):
    """生成标量变量的一条创建表达式。"""
    type_name = spec.get("type", "int")
    create = VAR_SCALAR_CREATE.get(type_name, "Var_CreateInt")
    value = spec.get("value")
    if value is None:
        return f"{create}({VAR_SCALAR_DEFAULT.get(type_name, '0')})"
    if type_name == "string":
        return f"{create}({_var_c_string(value)})"
    if type_name == "char":
        ch = str(value)
        if len(ch) == 1:
            esc = {"\\": "\\\\", "'": "\\'"}.get(ch, ch)
            return f"{create}('{esc}')"
        return f"{create}('\\0')"
    if type_name in ("float", "double"):
        number = float(value)
        text = repr(number)
        if type_name == "float":
            text += "f"
        return f"{create}({text})"
    return f"{create}({str(value)})"


def _var_array_code(array_fn, element_fn, init_fns, length):
    """数组构建函数：显式初始化项在前，剩余位置用默认元素补齐。"""
    lines = [
        "static VarValue_T *%s(void)" % array_fn,
        "{",
        "    VarValue_T *pArray = NULL;",
        "    size_t index = 0u;",
        "",
        "    pArray = Var_CreateArray();",
        "    if (pArray == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for init_fn in init_fns:
        lines.append("    Var_ArrayPush(pArray, %s());" % init_fn)
    if len(init_fns) < length:
        lines.append(
            "    for (index = 0u; index < %du; index++) {"
            % (length - len(init_fns))
        )
        lines.append("        Var_ArrayPush(pArray, %s());" % element_fn)
        lines.append("    }")
    lines.append("    return pArray;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def _var_dict_code(dict_fn, entries):
    """字典构建函数：按初始化列表逐项 Var_DictSet。"""
    lines = [
        "static VarValue_T *%s(void)" % dict_fn,
        "{",
        "    VarValue_T *pDict = NULL;",
        "",
        "    pDict = Var_CreateDict();",
        "    if (pDict == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for key_text, value_fn in entries:
        lines.append(
            "    Var_DictSet(pDict, %s, %s());"
            % (_var_c_string(key_text), value_fn)
        )
    lines.append("    return pDict;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def _var_struct_code(struct_fn, fields):
    """结构体构建函数：先建结构体，再逐字段注册子函数创建的成员。"""
    lines = [
        "static VarValue_T *%s(void)" % struct_fn,
        "{",
        "    VarValue_T *pStruct = NULL;",
        "",
        "    pStruct = Var_CreateStruct();",
        "    if (pStruct == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for index, (field_name, child_fn) in enumerate(fields):
        lines.append(
            "    Var_StructSetField(pStruct, %s, %s());"
            % (_var_c_string(field_name), child_fn)
        )
    lines.append("    return pStruct;")
    lines.append("}")
    return "\n".join(lines)


def _var_emit_node(spec, fn_name):
    """递归生成一个类型节点的构建函数 C 代码（子函数在前）。"""
    type_name = spec.get("type", "int")
    if type_name == "array":
        element_fn = fn_name + "_elem"
        length = int(spec.get("length", 0))
        init_items = spec.get("init", [])
        element_code = ""
        if len(init_items) < length:
            element_code = _var_emit_node(spec["element"], element_fn)
        init_fns = []
        for index, item_spec in enumerate(init_items):
            init_fn = fn_name + "_init%d" % index
            element_code += "\n\n" + _var_emit_node(item_spec, init_fn)
            init_fns.append(init_fn)
        return element_code + "\n\n" + _var_array_code(
            fn_name, element_fn, init_fns, length)
    if type_name == "struct":
        children = []
        child_codes = []
        init_map = spec.get("init", {})
        for index, (field_name, field_spec) in enumerate(
                spec.get("fields", {}).items()):
            child_fn = fn_name + "_f%d_%s" % (index, field_name)
            children.append((field_name, child_fn))
            emit_spec = init_map.get(field_name, field_spec)
            child_codes.append(_var_emit_node(emit_spec, child_fn))
        return "\n\n".join(child_codes + [_var_struct_code(fn_name, children)])
    if type_name == "dict":
        child_codes = []
        entries = []
        for index, (key_text, value_spec) in enumerate(
                spec.get("init_items", [])):
            value_fn = fn_name + "_item%d" % index
            entries.append((key_text, value_fn))
            child_codes.append(_var_emit_node(value_spec, value_fn))
        return "\n\n".join(
            child_codes + [_var_dict_code(fn_name, entries)])
    return "static VarValue_T *%s(void)\n{\n    return %s;\n}\n" % (
        fn_name, _var_scalar_code(spec))


# ---------------------------------------------------------------------------
# 动作表达式翻译器
# ---------------------------------------------------------------------------
# DSL 类型表：名字 -> (C 类型, VarValue 构造函数)
ACTION_TYPE_TABLE = {
    "char":   ("char", "Var_CreateChar"),
    "int8":   ("int8_t", "Var_CreateInt8"),
    "uint8":  ("uint8_t", "Var_CreateUint8"),
    "int16":  ("int16_t", "Var_CreateInt16"),
    "uint16": ("uint16_t", "Var_CreateUint16"),
    "int32":  ("int32_t", "Var_CreateInt"),
    "uint32": ("uint32_t", "Var_CreateUint32"),
    "int64":  ("int64_t", "Var_CreateInt64"),
    "uint64": ("uint64_t", "Var_CreateUint64"),
    "float":  ("float", "Var_CreateFloat"),
    "double": ("double", "Var_CreateDouble"),
}

ACTION_FLOAT_TYPES = {"float", "double"}

# 类型化赋值使用的运行时函数
ACTION_SETTER_TABLE = {
    "char":   "Var_SetChar",
    "int8":   "Var_SetInt8",
    "uint8":  "Var_SetUint8",
    "int16":  "Var_SetInt16",
    "uint16": "Var_SetUint16",
    "int32":  "Var_SetInt",
    "uint32": "Var_SetUint32",
    "int64":  "Var_SetInt64",
    "uint64": "Var_SetUint64",
    "float":  "Var_SetFloat",
    "double": "Var_SetDouble",
}


class _ExprNode:
    """动作表达式节点。

    kind  : 'ptr'（VarValue_T * 表达式）或 'raw'（C 标量表达式）
    code  : 生成的 C 表达式文本
    dtype : kind == 'raw' 时的 DSL 类型名
    lvalue: None，或 ('result',)，或 ('field', 指针表达式, 类型名)
    """

    __slots__ = ("kind", "code", "dtype", "lvalue")

    def __init__(self, kind, code, dtype=None, lvalue=None):
        self.kind = kind
        self.code = code
        self.dtype = dtype
        self.lvalue = lvalue


class _ActionTranslator:
    """把 Yacc 风格动作表达式翻译为运行时 API 调用的 C 语句。"""

    # 二元运算符优先级（数字越大越先结合）
    _PRECEDENCE = {
        "||": 1,
        "&&": 2,
        "|": 3,
        "^": 4,
        "&": 5,
        "==": 6, "!=": 6,
        "<": 7, ">": 7, "<=": 7, ">=": 7,
        "<<": 8, ">>": 8,
        "+": 9, "-": 9,
        "*": 10, "/": 10, "%": 10,
    }

    # 运行时值直接支持的二元运算
    _RUNTIME_ARITH = {
        "+": "Var_Add",
        "-": "Var_Sub",
        "*": "Var_Mul",
        "/": "Var_Div",
        "%": "Var_Mod",
    }
    _RUNTIME_LOGIC = {
        "&&": "Var_And",
        "||": "Var_Or",
    }
    # 常见运行时数值函数：函数名 -> (运行时 API, 参数个数)
    _RUNTIME_FUNCTIONS = {
        "max": ("Var_Max", 2),
        "min": ("Var_Min", 2),
        "abs": ("Var_Abs", 1),
        "pow": ("Var_Pow", 2),
        "sqrt": ("Var_Sqrt", 1),
        "floor": ("Var_Floor", 1),
        "ceil": ("Var_Ceil", 1),
        "round": ("Var_Round", 1),
        "clamp": ("Var_Clamp", 3),
    }

    _TOKEN_RE = re.compile(
        r"(?P<space>\s+)|"
        r"(?P<num>\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|"
        r"\.\d+(?:[eE][+-]?\d+)?)|"
        r'(?P<str>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|'
        r"(?P<id>[A-Za-z_]\w*)|"
        r"(?P<dollar>\$\$|\$\d+)|"
        r"(?P<op>&&|\|\||<<|>>|<=|>=|==|!=|"
        r"[+\-*/%^&|!~<>=?:(.)\[\];,])",
        re.VERBOSE
    )

    def __init__(self, action, mode="ll1"):
        """对单条动作文本做词法分析；mode 决定 $n 的取值方式。

        ll1：$n 读当前语义帧的 values[n] / names[n] 槽位；
        slr / lalr：$n 读 LR 栈从栈顶往下数第 k - n + 1 个槽位（k 为右部长度）。
        """
        self.action = action
        self.mode = mode
        self._slr_mode = (mode in ("slr", "lalr"))
        self._rhs_length = 0
        self.tokens = []
        for match in self._TOKEN_RE.finditer(action):
            if match.group("space"):
                continue
            if match.group("num") is not None:
                kind = "num"
            elif match.group("str") is not None:
                kind = "str"
            elif match.group("id") is not None:
                kind = "id"
            elif match.group("dollar") is not None:
                kind = "dollar"
            else:
                kind = "op"
            self.tokens.append(
                (kind, match.group(), match.start(), match.end())
            )
        self.pos = 0
        self._warned = set()

    # ------------------------------------------------------------------
    # 词法辅助
    # ------------------------------------------------------------------
    def _peek(self):
        """返回当前 token 文本；越界返回 None。"""
        if self.pos < len(self.tokens):
            return self.tokens[self.pos][1]
        return None

    def _accept(self, text):
        """若当前位置是指定文本则前进并返回 True。"""
        if self._peek() == text:
            self.pos += 1
            return True
        return False

    def _expect(self, text):
        """要求当前位置是指定文本。"""
        if not self._accept(text):
            raise ValueError(
                f"动作 '{self.action}' 期望 '{text}'，"
                f"实际遇到 '{self._peek()}'"
            )

    def _warn(self, message):
        """去重输出生成期警告。"""
        if message not in self._warned:
            self._warned.add(message)
            warnings.warn(message, RuntimeWarning, stacklevel=2)

    # ------------------------------------------------------------------
    # 类型转换辅助
    # ------------------------------------------------------------------
    def _ptr_to_raw(self, ptr_code, dtype):
        """把 VarValue 表达式读取为指定类型的 C 标量。"""
        ctype = ACTION_TYPE_TABLE[dtype][0]
        if dtype in ACTION_FLOAT_TYPES:
            return f"(({ctype})Var_ToDouble({ptr_code}))"
        return f"(({ctype})Var_ToInt({ptr_code}))"

    def _wrap_raw(self, node):
        """把 C 标量包装成 VarValue_T * 表达式。"""
        ctype, create = ACTION_TYPE_TABLE[node.dtype]
        return _ExprNode(
            "ptr",
            f"{create}(({ctype})({node.code}))"
        )

    def _common_dtype(self, left_dtype, right_dtype):
        """求两个 DSL 标量类型运算后的公共类型。"""
        if left_dtype == right_dtype:
            return left_dtype
        left_float = left_dtype in ACTION_FLOAT_TYPES
        right_float = right_dtype in ACTION_FLOAT_TYPES
        if left_float or right_float:
            if (left_dtype == "double") or (right_dtype == "double"):
                return "double"
            return "float"
        # 不同整数类型统一提升到 int64，避免中间溢出告警
        return "int64"

    # ------------------------------------------------------------------
    # 语法分析（词法 -> 表达式树）
    # ------------------------------------------------------------------
    def _value_code(self, index):
        """$index 对应的 VarValue_T * 表达式。"""
        if self._slr_mode:
            # 归约动作执行时 k 个右部符号仍在栈上：$n 即倒数第 k - n + 1 个槽位。
            # LR 运行时保证 stack->top >= k，因此这里不必再兜底判空；
            # 用三目会让表达式不能作为 $n.type 赋值的实参，反而破坏可赋值性。
            return ("stack->values[stack->top - %du + %du]"
                    % (self._rhs_length, index - 1))
        return "values[%d]" % index

    def _name_code(self, index):
        """$index 对应的变量名字符串表达式。"""
        if self._slr_mode:
            return ("stack->names[stack->top - %du + %du]"
                    % (self._rhs_length, index - 1))
        return "names[%d]" % index

    def translate(self, mode=None, rhs_length=0):
        """翻译整条动作，返回可直接写入生成的 C 文件的语句。

        mode 缺省沿用构造时传入的模式；rhs_length 只对 SLR 有意义。
        """
        if mode is None:
            mode = self.mode
        if mode in ("slr", "lalr"):
            return self._translate_lr(rhs_length)
        return self._translate_ll1()

    def _translate_ll1(self):
        """LL(1)：$n 取当前语义帧的槽位。"""
        self._slr_mode = False
        self._rhs_length = 0
        return self._translate_statements()

    def _translate_lr(self, rhs_length):
        """SLR(1) / LALR(1)：$n 取 LR 栈上该产生式右部对应的槽位。"""
        self._slr_mode = True
        self._rhs_length = max(0, int(rhs_length))
        return self._translate_statements()

    def _translate_statements(self):
        """翻译整条动作的公共逻辑（两种模式只在 $n 取值上不同）。"""
        statements = []
        while self.pos < len(self.tokens):
            if ((self._peek() == "delete") and
                    (self.pos + 1 < len(self.tokens)) and
                    (self.tokens[self.pos + 1][1] == "[")):
                statements.append(self._parse_delete_statement())
                continue
            lhs = self._parse_ternary()
            if self._accept("="):
                rhs = self._parse_ternary()
                statement = self._build_assignment(lhs, rhs)
            else:
                statement = lhs.code + ";"
            self._expect(";")
            statements.append(statement)
        if self.pos != len(self.tokens):
            raise ValueError(
                f"动作 '{self.action}' 存在无法识别的尾部内容："
                f"'{self.tokens[self.pos][1]}'"
            )
        return "\n        ".join(statements)

    def _parse_delete_statement(self):
        """delete[$n];：删除普通变量；系统/默认变量返回 PARSER_ERR_SYSTEM。"""
        self._expect("delete")
        self._expect("[")
        dollar = self._peek()
        if not (dollar is not None and dollar.startswith("$") and
                dollar[1:].isdigit()):
            raise ValueError(
                f"动作 '{self.action}'：delete 的参数必须是 $n"
            )
        name_index = int(dollar[1:])
        self.pos += 1
        self._expect("]")
        self._expect(";")
        return (
            "if (actionStatus == PARSER_OK) { "
            "actionStatus = Parser_DeleteVariable(%s); }"
            % self._name_code(name_index)
        )

    def _parse_ternary(self):
        """ternary := binary ('?' ternary ':' ternary)?  —— 自顶向下入口"""
        condition = self._parse_binary(1)
        if self._accept("?"):
            then_node = self._parse_ternary()
            self._expect(":")
            else_node = self._parse_ternary()
            return self._make_ternary(condition, then_node, else_node)
        return condition

    def _parse_binary(self, min_prec):
        """Pratt 表达式解析，天然支持括号与任意运算符组合。"""
        left = self._parse_unary()
        while True:
            op = self._peek()
            prec = self._PRECEDENCE.get(op)
            if (prec is None) or (prec < min_prec):
                break
            self.pos += 1
            right = self._parse_binary(prec + 1)
            left = self._make_binary(op, left, right)
        return left

    def _parse_unary(self):
        """一元前缀：! - + ~"""
        op = self._peek()
        if op in ("!", "-", "+", "~"):
            self.pos += 1
            return self._make_unary(op, self._parse_unary())
        return self._parse_postfix()

    def _parse_postfix(self):
        """后缀字段访问：$1.int32 / [$1].float。"""
        node = self._parse_primary()
        while True:
            token = self._peek()
            if token == ".":
                # 必须紧跟类型名，例如 int32/float/double
                nxt = self.tokens[self.pos + 1][1] \
                    if self.pos + 1 < len(self.tokens) else None
                if (nxt is None) or (nxt not in ACTION_TYPE_TABLE):
                    break
                self.pos += 2
                if node.kind != "ptr":
                    raise ValueError(
                        f"动作 '{self.action}'：只有运行时值能带类型字段"
                    )
                raw = self._ptr_to_raw(node.code, nxt)
                node = _ExprNode("raw", raw, nxt,
                                 lvalue=("field", node.code, nxt))
                continue
            break
        return node

    def _parse_primary(self):
        """原子：数字、$$、$n、[$n]、类型构造、函数调用、括号。"""
        token = self._peek()
        if token is None:
            raise ValueError(f"动作 '{self.action}' 意外结束")

        if self.tokens[self.pos][0] == "num":
            text = self.tokens[self.pos][1]
            self.pos += 1
            dtype = "double" if (("." in text) or
                                 ("e" in text) or ("E" in text)) \
                else "int32"
            return _ExprNode("raw", text, dtype)

        if token == "(":
            self.pos += 1
            node = self._parse_ternary()
            self._expect(")")
            return node

        if token == "$$":
            self.pos += 1
            return _ExprNode("ptr", "result.value", lvalue=("result",))

        if (token == "$") or (token.startswith("$") and
                              token[1:].isdigit()):
            # $1 / $2 ...
            self.pos += 1
            index = int(token[1:])
            return _ExprNode("ptr", self._value_code(index),
                             lvalue=("value", self._value_code(index)))

        if token == "[":
            self.pos += 1
            dollar = self._peek()
            if not (dollar is not None and dollar.startswith("$") and
                    dollar[1:].isdigit()):
                raise ValueError(
                    f"动作 '{self.action}'：'[' 后应为 [$n] 变量引用"
                )
            index = int(dollar[1:])
            self.pos += 1
            self._expect("]")
            code = (
                "Var_CloneValue(Parser_FindVariable(%s))"
                % self._name_code(index)
            )
            return _ExprNode("ptr", code,
                             lvalue=("variable", index, code))

        if self.tokens[self.pos][0] == "id":
            ident = token
            self.pos += 1
            if ident == "def" and self._peek() == "[":
                return self._parse_definition_target()
            if self._peek() == "(":
                # 类型构造 int32(...)/float(...)
                if ident in ACTION_TYPE_TABLE:
                    self.pos += 1
                    arg = self._parse_ternary()
                    self._expect(")")
                    return self._make_constructor(ident, arg)
                if ident in self._RUNTIME_FUNCTIONS:
                    return self._make_runtime_function(ident)
                # 其余函数调用保持原样（例如 Var_CloneValue(...)）
                code = self._consume_call(token)
                return _ExprNode("ptr", code)
            raise ValueError(
                f"动作 '{self.action}'：未定义标识符 '{ident}'，"
                f"它后面没有 '('"
            )

        raise ValueError(
            f"动作 '{self.action}'：无法解析 '{token}'"
        )

    def _parse_definition_target(self):
        """解析 def[$n]、def[$n, type] 或 def[$n, $m] 左值。"""
        self._expect("[")
        name_token = self._peek()
        if not (name_token is not None and name_token.startswith("$") and
                name_token[1:].isdigit()):
            raise ValueError(
                f"动作 '{self.action}'：def 的第一个参数必须是 $n"
            )
        name_index = int(name_token[1:])
        self.pos += 1
        type_spec = None
        if self._accept(","):
            token = self._peek()
            if token is not None and token.startswith("$") and token[1:].isdigit():
                type_spec = ("value", int(token[1:]))
                self.pos += 1
            elif token in ACTION_TYPE_TABLE or token == "string":
                base_type = token
                self.pos += 1
                dimensions = []
                while self._accept("["):
                    length_token = self._peek()
                    if (length_token is None or
                            self.tokens[self.pos][0] != "num" or
                            not length_token.isdigit() or
                            int(length_token) <= 0):
                        raise ValueError(
                            f"动作 '{self.action}'：数组长度必须是正整数"
                        )
                    dimensions.append(int(length_token))
                    self.pos += 1
                    self._expect("]")
                type_spec = ("explicit", base_type, dimensions)
            else:
                raise ValueError(
                    f"动作 '{self.action}'：def 类型只支持简单类型、数组或 $n"
                )
        self._expect("]")
        return _ExprNode(
            "ptr", "Parser_FindVariable(%s)" % self._name_code(name_index),
            lvalue=("define", name_index, type_spec)
        )

    def _make_runtime_function(self, ident):
        """把 max/min/abs/pow 等动作函数翻译成运行时 Var_* 调用。"""
        runtime_name, expected_arity = self._RUNTIME_FUNCTIONS[ident]
        arguments = []
        self._expect("(")
        if self._peek() != ")":
            while True:
                arguments.append(self._parse_ternary())
                if not self._accept(","):
                    break
        self._expect(")")
        if len(arguments) != expected_arity:
            raise ValueError(
                f"动作 '{self.action}'：函数 {ident} 需要 {expected_arity} "
                f"个参数，实际得到 {len(arguments)} 个"
            )
        # 中文注释：运行时函数接收 VarValue_T*，标量参数先包装成动态值
        arg_codes = []
        for argument in arguments:
            if argument.kind == "raw":
                argument = self._wrap_raw(argument)
            arg_codes.append(argument.code)
        return _ExprNode(
            "ptr",
            "%s(%s)" % (runtime_name, ", ".join(arg_codes))
        )

    def _consume_call(self, ident):
        """从 '(' 开始按括号配对称取整段调用文本。"""
        paren_start = self.tokens[self.pos][2]
        depth = 0
        index = paren_start
        quote = None
        while index < len(self.action):
            ch = self.action[index]
            if quote is not None:
                if ch == "\\":
                    index += 1
                elif ch == quote:
                    quote = None
            else:
                if ch in "\"'":
                    quote = ch
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        index += 1
                        break
            index += 1
        end = index
        ident_start = self.tokens[self.pos - 1][2]
        while (self.pos < len(self.tokens) and
               self.tokens[self.pos][3] <= end):
            self.pos += 1
        return self.action[ident_start:end]

    # ------------------------------------------------------------------
    # 表达式树 -> C
    # ------------------------------------------------------------------
    def _make_constructor(self, dtype, arg):
        """int32(0) / float($1) 等类型构造。"""
        ctype, create = ACTION_TYPE_TABLE[dtype]
        if arg.kind == "ptr":
            code = f"{create}({self._ptr_to_raw(arg.code, dtype)})"
        else:
            code = f"{create}(({ctype})({arg.code}))"
        return _ExprNode("ptr", code)

    def _make_ternary(self, condition, then_node, else_node):
        """三元：运行时值按 Var_IsTrue 判断，选中的分支做一次深拷贝。"""
        if condition.kind == "ptr":
            if then_node.kind == "raw":
                then_node = self._wrap_raw(then_node)
            if else_node.kind == "raw":
                else_node = self._wrap_raw(else_node)
            return _ExprNode(
                "ptr",
                f"Var_IsTrue({condition.code}) ? "
                f"Var_CloneValue({then_node.code}) : "
                f"Var_CloneValue({else_node.code})"
            )
        if (then_node.kind == "ptr") or (else_node.kind == "ptr"):
            if then_node.kind == "raw":
                then_node = self._wrap_raw(then_node)
            if else_node.kind == "raw":
                else_node = self._wrap_raw(else_node)
            return _ExprNode(
                "ptr",
                f"(({condition.code}) != 0) ? "
                f"Var_CloneValue({then_node.code}) : "
                f"Var_CloneValue({else_node.code})"
            )
        dtype = self._common_dtype(then_node.dtype, else_node.dtype)
        return _ExprNode(
            "raw",
            f"(({condition.code}) ? ({then_node.code}) : "
            f"({else_node.code}))",
            dtype
        )

    def _make_unary(self, op, node):
        """一元运算符。"""
        if op == "!":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_Not({node.code})")
            return _ExprNode("raw", f"(!({node.code}))", "int32")
        if op == "-":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_Neg({node.code})")
            return _ExprNode("raw", f"(-({node.code}))", node.dtype)
        if op == "+":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_CloneValue({node.code})")
            return _ExprNode("raw", f"(+({node.code}))", node.dtype)
        if op == "~":
            if node.kind == "ptr":
                self._warn(
                    f"动作 '{self.action}'：运算符 ~ 未对运行时值定义，"
                    "已按 int32 隐式换算；建议写 $n.int32 后参与位运算"
                )
                return _ExprNode(
                    "raw",
                    f"(~(int32_t)Var_ToInt({node.code}))",
                    "int32"
                )
            return _ExprNode("raw", f"(~({node.code}))", node.dtype)
        raise ValueError(f"动作 '{self.action}'：未知一元运算符 {op}")

    def _make_binary(self, op, left, right):
        """二元运算符，自动区分运行时值与 C 标量。"""
        # 两侧都是运行时值：优先映射到 Var_* 动态运算
        if (left.kind == "ptr") and (right.kind == "ptr"):
            fn = self._RUNTIME_ARITH.get(op) or \
                self._RUNTIME_LOGIC.get(op)
            if fn is not None:
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            # 未定义的运行时运算符：警告并按 int32 隐式换算
            self._warn(
                f"动作 '{self.action}'：运算符 {op} 未对运行时值定义，"
                f"已按 int32 隐式换算；建议写 $n.int32 {op} $n.int32"
            )
            left_raw = self._ptr_to_raw(left.code, "int32")
            right_raw = self._ptr_to_raw(right.code, "int32")
            return self._make_binary(op,
                                     _ExprNode("raw", left_raw, "int32"),
                                     _ExprNode("raw", right_raw, "int32"))

        # 一侧是运行时值、一侧是标量：标量先包成 VarValue 再走动态运算
        if (left.kind == "ptr") or (right.kind == "ptr"):
            if op in self._RUNTIME_ARITH:
                if left.kind == "raw":
                    left = self._wrap_raw(left)
                if right.kind == "raw":
                    right = self._wrap_raw(right)
                fn = self._RUNTIME_ARITH[op]
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            if op in self._RUNTIME_LOGIC:
                if left.kind == "raw":
                    left = self._wrap_raw(left)
                if right.kind == "raw":
                    right = self._wrap_raw(right)
                fn = self._RUNTIME_LOGIC[op]
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            # 位运算 / 移位 / 比较：把运行时值按 int32 读取
            if left.kind == "ptr":
                left = _ExprNode(
                    "raw", self._ptr_to_raw(left.code, "int32"), "int32")
            if right.kind == "ptr":
                right = _ExprNode(
                    "raw", self._ptr_to_raw(right.code, "int32"), "int32")
            return self._make_binary(op, left, right)

        # 两侧都是 C 标量：按 C 类型提升直接生成运算
        return self._make_raw_binary(op, left, right)

    def _make_raw_binary(self, op, left, right):
        """C 标量运算：返回 'raw' 节点，顶层赋值时再包成 VarValue。"""
        comparison = op in ("==", "!=", "<", ">", "<=", ">=")
        int_only = op in ("%", "&", "|", "^", "<<", ">>")
        dtype = self._common_dtype(left.dtype, right.dtype)

        # 比较结果直接是 0/1
        if comparison:
            return _ExprNode(
                "raw",
                f"(({left.code}) {op} ({right.code}))",
                "int32"
            )
        # 位运算只允许整数参与：若出现浮点则统一按 int64 读取
        if int_only and (dtype in ACTION_FLOAT_TYPES):
            self._warn(
                f"动作 '{self.action}'：运算符 {op} 在浮点上无定义，"
                "已把两侧按 int64 截断"
            )
            dtype = "int64"
        ctype = ACTION_TYPE_TABLE[dtype][0]
        return _ExprNode(
            "raw",
            f"(({ctype})({left.code}) {op} ({ctype})({right.code}))",
            dtype
        )

    def _build_assignment(self, lhs, rhs):
        """根据左值的种类生成赋值语句。"""
        if lhs.lvalue is None:
            raise ValueError(
                f"动作 '{self.action}'：'=' 左侧不是可赋值目标"
            )
        lvalue_kind = lhs.lvalue[0]

        # $$ = 表达式：把 C 标量自动包成 VarValue
        if lvalue_kind == "result":
            if rhs.kind == "ptr":
                return f"{lhs.code} = {rhs.code};"
            return f"{lhs.code} = {self._wrap_raw(rhs).code};"

        # $n.type / [$n].type = 表达式：直接覆盖数值内容
        if lvalue_kind == "field":
            _, ptr_code, dtype = lhs.lvalue
            setter = ACTION_SETTER_TABLE[dtype]
            if rhs.kind == "ptr":
                raw = self._ptr_to_raw(rhs.code, dtype)
            else:
                ctype = ACTION_TYPE_TABLE[dtype][0]
                raw = f"(({ctype})({rhs.code}))"
            return f"{setter}({ptr_code}, {raw});"

        # def[$n, ...] = 表达式：定义新变量并固定其类型
        if lvalue_kind == "define":
            _, name_index, type_spec = lhs.lvalue
            rhs_code = rhs.code if rhs.kind == "ptr" else self._wrap_raw(rhs).code
            if type_spec is None:
                return (
                    "if (actionStatus == PARSER_OK) { "
                    "actionStatus = Parser_DefineVariable(%s, "
                    "%s, NULL); }"
                    % (self._name_code(name_index), rhs_code)
                )
            if type_spec[0] == "value":
                return (
                    "if (actionStatus == PARSER_OK) { "
                    "actionStatus = Parser_DefineVariable(%s, "
                    "%s, %s); }"
                    % (self._name_code(name_index), rhs_code,
                       self._value_code(type_spec[1]))
                )
            _, base_type, dimensions = type_spec
            enum_name = {
                "int32": "VAR_TYPE_INT", "int": "VAR_TYPE_INT",
                "uint8": "VAR_TYPE_UINT8", "int8": "VAR_TYPE_INT8",
                "uint16": "VAR_TYPE_UINT16", "int16": "VAR_TYPE_INT16",
                "uint32": "VAR_TYPE_UINT32", "uint64": "VAR_TYPE_UINT64",
                "int64": "VAR_TYPE_INT64", "float": "VAR_TYPE_FLOAT",
                "double": "VAR_TYPE_DOUBLE", "char": "VAR_TYPE_CHAR",
                "string": "VAR_TYPE_STRING",
            }[base_type]
            if dimensions:
                dims = ", ".join(str(item) + "u" for item in dimensions)
                return (
                    f"{{ const uint32_t dimensions[] = {{ {dims} }}; "
                    "if (actionStatus == PARSER_OK) { "
                    "actionStatus = Parser_DefineTyped(%s, "
                    "%s, %s, dimensions, "
                    "%du); } }"
                    % (self._name_code(name_index), rhs_code,
                       enum_name, len(dimensions))
                )
            return (
                "if (actionStatus == PARSER_OK) { "
                "actionStatus = Parser_DefineTyped(%s, "
                "%s, %s, NULL, 0u); }"
                % (self._name_code(name_index), rhs_code, enum_name)
            )

        # [$n] = 表达式：只允许给已经存在的固定类型变量赋值
        if lvalue_kind == "variable":
            _, name_index, _ = lhs.lvalue
            rhs_code = rhs.code if rhs.kind == "ptr" else self._wrap_raw(rhs).code
            return (
                "if (actionStatus == PARSER_OK) { "
                "actionStatus = Parser_AssignVariable(%s, "
                "%s); }"
                % (self._name_code(name_index), rhs_code)
            )

        raise ValueError(
            f"动作 '{self.action}'：直接给 $n 赋值会把子结果指针指向新对象，"
            "会破坏归属关系；请用 $n.int32 = 表达式 或 $$ = 表达式"
        )


def _translate_action(action, mode="ll1", rhs_length=0):
    """对外入口：把一条动作文本翻译成可生成的 C 语句。"""
    return _ActionTranslator(action, mode).translate(
        mode=mode, rhs_length=rhs_length
    )


def c_token_name(token):
    return "TOKEN_" + token


def _generator_registry_block(variables, max_variables):
    """生成固定容量变量表，以及定义和固定类型赋值接口。"""
    builders = []
    initializers = []
    for index, (name, spec) in enumerate(variables.items()):
        if isinstance(spec, str):
            spec = {"type": spec}
        elif not isinstance(spec, dict):
            spec = {"type": "int32", "value": spec}
        builder = "Parser_CreateBuiltin_%d" % index
        builders.append(_var_emit_node(spec, builder))
        initializers.append(
            "    Parser_StoreBuiltin(%s, %s());"
            % (_var_c_string(name), builder)
        )
    builder_text = "\n\n".join(builders)
    init_text = ("\n".join(initializers) if initializers
                 else "    /* 当前文法没有预声明变量。 */")
    code = r'''
/* 固定容量变量表避免在 MCU 上动态扩展注册表。 */
#ifndef PARSER_MAX_VARIABLES
#define PARSER_MAX_VARIABLES __VAR_MAX_COUNT__u
#endif

typedef struct __ParserVariable_T
{
    uint8_t used;
    uint8_t system;
    char name[PARSER_MAX_NAME_LENGTH];
    VarValue_T *pValue;
} ParserVariable_T;

static ParserVariable_T s_ParserVariables[PARSER_MAX_VARIABLES];
static uint8_t s_ParserVariablesReady = 0u;

/** @brief 查找变量表项。 @param pName 名称。 @retval 表项或 NULL。 */
static ParserVariable_T *Parser_FindVariableEntry(const char *pName)
{
    uint32_t index = 0u;
    if (pName == NULL) { return NULL; }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if ((s_ParserVariables[index].used != 0u) &&
            (strcmp(s_ParserVariables[index].name, pName) == 0)) {
            return &s_ParserVariables[index];
        }
    }
    return NULL;
}

/** @brief 保存预声明变量。 @param pName 名称。 @param pValue 值。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_StoreBuiltin(const char *pName,
                                                 VarValue_T *pValue)
{
    uint32_t index = 0u;
    if ((pName == NULL) || (pValue == NULL)) { return VAR_RUNTIME_ERR_TYPE; }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if (s_ParserVariables[index].used == 0u) {
            s_ParserVariables[index].used = 1u;
            s_ParserVariables[index].system = 1u;
            snprintf(s_ParserVariables[index].name,
                     sizeof(s_ParserVariables[index].name), "%s", pName);
            s_ParserVariables[index].pValue = pValue;
            return VAR_RUNTIME_OK;
        }
    }
    Var_Free(pValue);
    return VAR_RUNTIME_ERR_MEMORY;
}

/** @brief 初始化预声明变量。 @retval 状态码。 */
static int32_t Parser_InitVariables(void)
{
    if (s_ParserVariablesReady != 0u) { return VAR_RUNTIME_OK; }
    s_ParserVariablesReady = 1u;
__INITIALIZERS__
    return VAR_RUNTIME_OK;
}

/** @brief 查找变量值。 @param pName 名称。 @retval 值或 NULL。 */
static PARSER_UNUSED VarValue_T *Parser_FindVariable(const char *pName)
{
    ParserVariable_T *pEntry = NULL;
    Parser_InitVariables();
    pEntry = Parser_FindVariableEntry(pName);
    return (pEntry != NULL) ? pEntry->pValue : NULL;
}

/** @brief 定义变量。 @param pName 名称。 @param pValue 初值。 @param pTemplate 类型模板。 @retval 状态码。 */
static int32_t Parser_DefineVariable(const char *pName,
                                     const VarValue_T *pValue,
                                     const VarValue_T *pTemplate)
{
    VarValue_T *pNewValue = NULL;
    ParserVariable_T *pEntry = NULL;
    uint32_t index = 0u;
    int32_t ret = VAR_RUNTIME_OK;
    Parser_InitVariables();
    if ((pName == NULL) || (pValue == NULL)) { return VAR_RUNTIME_ERR_TYPE; }
    if (pTemplate == NULL) {
        pNewValue = Var_CloneValue(pValue);
    } else {
        ret = Var_ConvertValue(pValue, pTemplate, &pNewValue);
        if (ret != VAR_RUNTIME_OK) { return ret; }
    }
    if (pNewValue == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    pEntry = Parser_FindVariableEntry(pName);
    if (pEntry != NULL) {
        /* 中文注释：系统变量禁止覆盖；普通变量允许用新类型覆盖旧定义 */
        if (pEntry->system != 0u) {
            Var_Free(pNewValue);
            return PARSER_ERR_SYSTEM;
        }
        Var_Free(pEntry->pValue);
        pEntry->pValue = pNewValue;
        return VAR_RUNTIME_OK;
    }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if (s_ParserVariables[index].used == 0u) {
            s_ParserVariables[index].used = 1u;
            s_ParserVariables[index].system = 0u;
            snprintf(s_ParserVariables[index].name,
                     sizeof(s_ParserVariables[index].name), "%s", pName);
            s_ParserVariables[index].pValue = pNewValue;
            return VAR_RUNTIME_OK;
        }
    }
    Var_Free(pNewValue);
    return VAR_RUNTIME_ERR_MEMORY;
}

/** @brief 固定类型赋值。 @param pName 名称。 @param pValue 新值。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_AssignVariable(
    const char *pName, const VarValue_T *pValue)
{
    ParserVariable_T *pEntry = NULL;
    VarValue_T *pConverted = NULL;
    int32_t ret = VAR_RUNTIME_OK;
    Parser_InitVariables();
    pEntry = Parser_FindVariableEntry(pName);
    if (pEntry == NULL) { return PARSER_ERR_UNDEFINED; }
    ret = Var_ConvertValue(pValue, pEntry->pValue, &pConverted);
    if (ret != VAR_RUNTIME_OK) { return ret; }
    ret = Var_SetValue(pEntry->pValue, pConverted);
    Var_Free(pConverted);
    return ret;
}

/** @brief 删除普通变量；系统/默认变量禁止删除。 @param pName 名称。 @retval 状态码。 */
int32_t Parser_DeleteVariable(const char *pName)
{
    ParserVariable_T *pEntry = NULL;
    Parser_InitVariables();
    pEntry = Parser_FindVariableEntry(pName);
    if (pEntry == NULL) { return PARSER_ERR_UNDEFINED; }
    if (pEntry->system != 0u) { return PARSER_ERR_SYSTEM; }
    Var_Free(pEntry->pValue);
    memset(pEntry, 0, sizeof(*pEntry));
    return PARSER_OK;
}
'''
    return builder_text + "\n\n" + code \
        .replace("__VAR_MAX_COUNT__", str(max_variables)) \
        .replace("__INITIALIZERS__", init_text)


# =====================================================================
# 简单正则 → DFA
#
# 参考 lex.py 的做法：正则解析成 AST → Thompson 构造 NFA → 子集构造 DFA →
# 划分法最小化。差别是符号用“字符类”表示（这样 [a-z]、\d 这类字符集也能
# 参与），并且生成期就把 DFA 表算好，直接写进 C 代码，运行期不做正则解释。
#
# 支持：字面量、\ 转义（\n \r \t \0 \\ \. 等）、\d \w \s 及其大写取反、
#       . 任意字符（不含换行）、[abc] [a-z] [^...]、( )、|、*、+、?，
#       以及 {m} {m,} {m,n} 重复（展开实现，m/n 不超过 _REGEX_MAX_REPEAT）。
# =====================================================================

_REGEX_MAX_REPEAT = 32
# 参与匹配的字符集合：1..255（不含 NUL，C 字符串里不会出现）
_REGEX_ALL_CHARS = "".join(chr(code) for code in range(1, 256))
_REGEX_DIGITS = frozenset(chr(code) for code in range(ord("0"), ord("9") + 1))
_REGEX_WORDS = frozenset(
    [ch for ch in _REGEX_ALL_CHARS if ch.isalnum()] + ["_"]
)
_REGEX_SPACES = frozenset(" \t\r\n\f\v")


class _RegexSyntaxError(ValueError):
    """简单正则表达式的语法错误。"""


def _regex_escape_set(ch):
    """\\d / \\w / \\s 一类转义返回字符集合，其余返回 None。"""
    shorthand = {
        "d": _REGEX_DIGITS,
        "w": _REGEX_WORDS,
        "s": _REGEX_SPACES,
    }
    lowered = ch.lower()
    if lowered not in shorthand:
        return None
    chars = shorthand[lowered]
    if ch.isupper():
        return frozenset(_REGEX_ALL_CHARS) - chars
    return chars


def _regex_escape_char(ch):
    """把 \\ 转义还原成一个字符。"""
    return {"n": "\n", "r": "\r", "t": "\t", "f": "\f", "v": "\v",
            "0": "\0", "\\": "\\", ".": ".", "*": "*", "+": "+",
            "?": "?", "(": "(", ")": ")", "[": "[", "]": "]",
            "{": "{", "}": "}", "|": "|", "^": "^", "-": "-",
            "/": "/"}.get(ch, ch)


def _regex_dot_set():
    """`.` 的字符集合：除换行以外的任意字符。"""
    return frozenset(ch for ch in _REGEX_ALL_CHARS if ch != "\n")


class _RegexParser:
    """把简单正则解析成 AST（元组形式，与 lex.py 一致）。"""

    def __init__(self, pattern):
        self.pattern = pattern
        self.pos = 0
        self.length = len(pattern)

    def peek(self, offset=0):
        index = self.pos + offset
        if 0 <= index < self.length:
            return self.pattern[index]
        return None

    def next(self):
        ch = self.peek()
        if ch is not None:
            self.pos += 1
        return ch

    def parse(self):
        """入口：解析整条正则。"""
        if self.length == 0:
            raise _RegexSyntaxError("正则表达式不能为空")
        node = self.parse_alt()
        if self.pos != self.length:
            raise _RegexSyntaxError(
                "位置 %d 处有多余字符 '%s'" % (self.pos, self.peek())
            )
        return node

    def parse_alt(self):
        """alt := concat ('|' concat)*"""
        node = self.parse_concat()
        while self.peek() == "|":
            self.next()
            node = ("alt", node, self.parse_concat())
        return node

    def parse_concat(self):
        """concat := factor*"""
        nodes = []
        while (self.peek() is not None) and (self.peek() not in ("|", ")")):
            nodes.append(self.parse_factor())
        if not nodes:
            return ("empty",)
        node = nodes[0]
        for item in nodes[1:]:
            node = ("concat", node, item)
        return node

    def parse_factor(self):
        """factor := base ('*' | '+' | '?' | '{m,n}')*"""
        node = self.parse_base()
        while True:
            ch = self.peek()
            if ch == "*":
                self.next()
                node = ("star", node)
            elif ch == "+":
                self.next()
                node = ("plus", node)
            elif ch == "?":
                self.next()
                node = ("opt", node)
            elif ch == "{":
                node = self.parse_repeat(node)
            else:
                break
        return node

    def parse_repeat(self, node):
        """把 {m} / {m,} / {m,n} 展开成等价的连接与可选。"""
        self.next()  # '{'
        digits = ""
        while (self.peek() is not None) and self.peek().isdigit():
            digits += self.next()
        minimum = int(digits) if digits else 0
        maximum = minimum
        if self.peek() == ",":
            self.next()
            digits = ""
            while (self.peek() is not None) and self.peek().isdigit():
                digits += self.next()
            maximum = int(digits) if digits else None
        if self.peek() != "}":
            raise _RegexSyntaxError("重复次数缺少右花括号 }")
        self.next()
        if (maximum is not None) and (maximum < minimum):
            raise _RegexSyntaxError("重复次数上限小于下限")
        limit = minimum if maximum is None else maximum
        if limit > _REGEX_MAX_REPEAT:
            raise _RegexSyntaxError(
                "重复次数不能超过 %d" % _REGEX_MAX_REPEAT
            )
        pieces = [node] * minimum
        if maximum is None:
            pieces.append(("star", node))
        else:
            pieces.extend([("opt", node)] * (maximum - minimum))
        if not pieces:
            raise _RegexSyntaxError("{0} 不匹配任何输入")
        result = pieces[0]
        for item in pieces[1:]:
            result = ("concat", result, item)
        return result

    def parse_base(self):
        """base := '(' alt ')' | '[' class ']' | 转义 | '.' | 普通字符"""
        ch = self.peek()
        if ch == "(":
            self.next()
            node = self.parse_alt()
            if self.peek() != ")":
                raise _RegexSyntaxError("缺少右括号 )")
            self.next()
            return node
        if ch == "[":
            return self.parse_class()
        if ch == "\\":
            self.next()
            escaped = self.next()
            if escaped is None:
                raise _RegexSyntaxError("反斜杠后没有字符")
            chars = _regex_escape_set(escaped)
            if chars is not None:
                return ("class", chars)
            return ("class", frozenset([_regex_escape_char(escaped)]))
        if ch == ".":
            self.next()
            return ("class", _regex_dot_set())
        if ch is None or ch in ("|", ")", "*", "+", "?", "{"):
            raise _RegexSyntaxError("意外的字符 '%s'" % ch)
        self.next()
        return ("class", frozenset([ch]))

    def parse_class(self):
        """class := '[' '^'? (字符 | 字符'-'字符)* ']'"""
        self.next()  # '['
        negate = False
        if self.peek() == "^":
            self.next()
            negate = True
        chars = set()
        first = True
        while True:
            ch = self.peek()
            if ch is None:
                raise _RegexSyntaxError("字符集缺少右括号 ]")
            if (ch == "]") and not first:
                self.next()
                break
            first = False
            if ch == "\\":
                self.next()
                escaped = self.next()
                if escaped is None:
                    raise _RegexSyntaxError("字符集里的反斜杠后没有字符")
                shorthand = _regex_escape_set(escaped)
                if shorthand is not None:
                    chars |= shorthand
                    continue
                start = _regex_escape_char(escaped)
            else:
                self.next()
                start = ch
            if (self.peek() == "-") and (self.peek(1) not in (None, "]")):
                self.next()
                end_char = self.next()
                if end_char == "\\":
                    end_char = self.next()
                    if end_char is None:
                        raise _RegexSyntaxError("字符集区间不完整")
                    end_char = _regex_escape_char(end_char)
                if ord(start) > ord(end_char):
                    raise _RegexSyntaxError(
                        "字符集区间 %s-%s 顺序颠倒" % (start, end_char)
                    )
                chars |= set(chr(code)
                             for code in range(ord(start), ord(end_char) + 1))
            else:
                chars.add(start)
        if negate:
            chars = set(_REGEX_ALL_CHARS) - chars
        chars.discard("\0")
        if not chars:
            raise _RegexSyntaxError("字符集不能为空")
        return ("class", frozenset(chars))


def _regex_collect_class_sets(node, found):
    """收集 AST 里出现过的所有字符集合（用于划分字符类）。"""
    kind = node[0]
    if kind == "class":
        found.append(node[1])
    elif kind in ("concat", "alt"):
        _regex_collect_class_sets(node[1], found)
        _regex_collect_class_sets(node[2], found)
    elif kind in ("star", "plus", "opt"):
        _regex_collect_class_sets(node[1], found)


def _regex_char_classes(class_sets):
    """把若干字符集合划分成互不相交的字符类。

    返回 (cells, char_map)：
      cells：[(代表字符, 字符集合), ...]，每个类一个代表字符；
      char_map：长度 256 的列表，字符编码 -> 类编号。
    同一个类里的字符对每个集合的“属于/不属于”完全一致，所以判断归属
    只需要看代表字符。
    """
    groups = {}
    for code in range(256):
        ch = chr(code)
        signature = tuple(ch in chars for chars in class_sets)
        groups.setdefault(signature, []).append(ch)
    cells = []
    char_map = [0] * 256
    for signature, chars in sorted(groups.items(),
                                   key=lambda item: ord(item[1][0])):
        index = len(cells)
        cells.append((chars[0], frozenset(chars)))
        for ch in chars:
            char_map[ord(ch)] = index
    return cells, char_map


class _RegexNfa:
    """Thompson 构造的 NFA；转移符号是字符类编号。"""

    def __init__(self, cells):
        self.cells = cells
        self.trans = {}
        self.eps = {}
        self.start = None
        self.accept = None
        self.state_count = 0

    def new_state(self):
        state = self.state_count
        self.state_count += 1
        self.trans[state] = {}
        self.eps[state] = set()
        return state

    def symbols_of(self, char_set):
        """把字符集合映射成字符类编号集合。"""
        return [index for index, (rep, _) in enumerate(self.cells)
                if rep in char_set]

    def build(self, ast):
        self.start, self.accept = self._build(ast)
        return self

    def _build(self, node):
        kind = node[0]
        if kind == "class":
            start = self.new_state()
            end = self.new_state()
            for symbol in self.symbols_of(node[1]):
                self.trans[start].setdefault(symbol, set()).add(end)
            return start, end
        if kind == "empty":
            start = self.new_state()
            end = self.new_state()
            self.eps[start].add(end)
            return start, end
        if kind == "concat":
            start, mid = self._build(node[1])
            mid2, end = self._build(node[2])
            self.eps[mid].add(mid2)
            return start, end
        if kind == "alt":
            left_start, left_end = self._build(node[1])
            right_start, right_end = self._build(node[2])
            start = self.new_state()
            end = self.new_state()
            self.eps[start].add(left_start)
            self.eps[start].add(right_start)
            self.eps[left_end].add(end)
            self.eps[right_end].add(end)
            return start, end
        if kind == "star":
            sub_start, sub_end = self._build(node[1])
            start = self.new_state()
            end = self.new_state()
            self.eps[start].add(sub_start)
            self.eps[start].add(end)
            self.eps[sub_end].add(sub_start)
            self.eps[sub_end].add(end)
            return start, end
        if kind == "plus":
            sub_start, sub_end = self._build(node[1])
            start = self.new_state()
            end = self.new_state()
            self.eps[start].add(sub_start)
            self.eps[sub_end].add(sub_start)
            self.eps[sub_end].add(end)
            return start, end
        if kind == "opt":
            sub_start, sub_end = self._build(node[1])
            start = self.new_state()
            end = self.new_state()
            self.eps[start].add(sub_start)
            self.eps[start].add(end)
            self.eps[sub_end].add(end)
            return start, end
        raise _RegexSyntaxError("未知的正则节点 %s" % kind)


def _regex_epsilon_closure(nfa, states):
    """ε 闭包。"""
    stack = list(states)
    closure = set(states)
    while stack:
        state = stack.pop()
        for target in nfa.eps.get(state, ()):
            if target not in closure:
                closure.add(target)
                stack.append(target)
    return frozenset(closure)


def _regex_subset_construction(nfa, alphabet):
    """子集构造：NFA -> DFA（补齐陷阱状态，转移全覆盖）。"""
    start_closure = _regex_epsilon_closure(nfa, {nfa.start})
    state_map = {start_closure: 0}
    transitions = {}
    accept = set()
    queue = deque([start_closure])
    next_id = 1
    while queue:
        current = queue.popleft()
        current_id = state_map[current]
        if nfa.accept in current:
            accept.add(current_id)
        row = {}
        for symbol in alphabet:
            following = set()
            for state in current:
                if symbol in nfa.trans.get(state, {}):
                    following.update(nfa.trans[state][symbol])
            if not following:
                continue
            closure = _regex_epsilon_closure(nfa, following)
            if closure not in state_map:
                state_map[closure] = next_id
                next_id += 1
                queue.append(closure)
            row[symbol] = state_map[closure]
        transitions[current_id] = row
    trap = next_id
    transitions[trap] = dict((symbol, trap) for symbol in alphabet)
    for state in range(next_id):
        row = transitions.setdefault(state, {})
        for symbol in alphabet:
            row.setdefault(symbol, trap)
    return transitions, 0, accept, trap


def _regex_minimize(transitions, start, accept, trap, alphabet):
    """划分法最小化（保留陷阱状态）。"""
    reachable = set()
    stack = [start]
    while stack:
        state = stack.pop()
        if state in reachable:
            continue
        reachable.add(state)
        for symbol in alphabet:
            target = transitions[state][symbol]
            if target not in reachable:
                stack.append(target)
    accept = accept & reachable
    partitions = []
    if reachable - accept:
        partitions.append(reachable - accept)
    if accept:
        partitions.append(accept)
    while True:
        refined = []
        for group in partitions:
            buckets = {}
            for state in group:
                signature = tuple(
                    next(index for index, part in enumerate(partitions)
                         if transitions[state][symbol] in part)
                    for symbol in alphabet
                )
                buckets.setdefault(signature, set()).add(state)
            refined.extend(buckets.values())
        if len(refined) == len(partitions):
            break
        partitions = refined
    state_to_group = {}
    for index, group in enumerate(partitions):
        for state in group:
            state_to_group[state] = index
    new_transitions = {}
    for index, group in enumerate(partitions):
        representative = next(iter(group))
        new_transitions[index] = dict(
            (symbol, state_to_group[transitions[representative][symbol]])
            for symbol in alphabet
        )
    new_accept = set(state_to_group[state] for state in accept)
    return (new_transitions, state_to_group[start], new_accept,
            state_to_group[trap], len(partitions))


def _regex_build_dfa(pattern, cells):
    """把一条正则编译成极小 DFA（转移按字符类编号）。"""
    ast = _RegexParser(pattern).parse()
    nfa = _RegexNfa(cells).build(ast)
    alphabet = list(range(len(cells)))
    transitions, start, accept, trap = _regex_subset_construction(nfa, alphabet)
    transitions, start, accept, trap, count = _regex_minimize(
        transitions, start, accept, trap, alphabet
    )
    return {
        "pattern": pattern,
        "stateCount": count,
        "start": start,
        "trap": trap,
        "accept": accept,
        "transitions": transitions,
    }


def _regex_compile_all(regex_defs):
    """把 {名字: 正则} 全部编译成 DFA，共用一份字符类划分。

    返回 (cells, char_map, [{name, pattern, dfa}, ...])。
    """
    asts = []
    class_sets = []
    for name, pattern in regex_defs:
        ast = _RegexParser(pattern).parse()
        asts.append((name, pattern, ast))
        _regex_collect_class_sets(ast, class_sets)
    cells, char_map = _regex_char_classes(class_sets)
    compiled = []
    for name, pattern, _ in asts:
        compiled.append({
            "name": name,
            "pattern": pattern,
            "dfa": _regex_build_dfa(pattern, cells),
        })
    return cells, char_map, compiled


def _is_identifier_literal(text):
    """字面量是否像标识符（字母/下划线开头，其余是字母数字下划线）。"""
    if not text:
        return False
    if not (text[0].isalpha() or (text[0] == "_")):
        return False
    return all(ch.isalnum() or (ch == "_") for ch in text[1:])


def _generator_keyword_c(token_defs, static_prefix=""):
    """生成关键字表与 Parser_TokenKeyword（%define 里像标识符的字面量）。"""
    keywords = []
    for token_name, literals in token_defs.items():
        for literal in literals:
            if _is_identifier_literal(literal):
                keywords.append((literal, token_name))
    if not keywords:
        return (
            "/** @brief 关键字查表。 @param pText 文本。\n"
            " *  @retval 关键字对应的 Token；不是关键字时返回 TOKEN_INVALID。 */\n"
            "ParserTokenType_T Parser_TokenKeyword(const char *pText)\n"
            "{\n"
            "    (void)pText;\n"
            "    return TOKEN_INVALID;\n"
            "}\n\n"
        )
    rows = ",\n".join(
        "    { %s, TOKEN_%s }" % (_var_c_string(text), token_name)
        for text, token_name in keywords
    ) or "    { NULL, TOKEN_INVALID }"
    return (
        "/* 关键字表：%%define / %%token 里像标识符的字面量 */\n"
        "typedef struct ParserKeyword_T\n"
        "{\n"
        "    const char *pText;\n"
        "    ParserTokenType_T type;\n"
        "} ParserKeyword_T;\n"
        "\n"
        "#define PARSER_KEYWORD_COUNT %du\n"
        "%sconst ParserKeyword_T s_ParserKeywords[] =\n"
        "{\n"
        "%s\n"
        "};\n"
        "\n"
        "/** @brief 关键字查表。 @param pText 文本。\n"
        " *  @retval 关键字对应的 Token；不是关键字时返回 TOKEN_INVALID。 */\n"
        "ParserTokenType_T Parser_TokenKeyword(const char *pText)\n"
        "{\n"
        "    uint32_t index = 0u;\n"
        "    if (pText == NULL) { return TOKEN_INVALID; }\n"
        "    for (index = 0u; index < PARSER_KEYWORD_COUNT; index++) {\n"
        "        if (strcmp(pText, s_ParserKeywords[index].pText) == 0) {\n"
        "            return s_ParserKeywords[index].type;\n"
        "        }\n"
        "    }\n"
        "    return TOKEN_INVALID;\n"
        "}\n\n"
        % (len(keywords), static_prefix, rows)
    )


def _generator_lex_dfa_c(token_defs, regex_defs, identifier_pattern):
    """生成 %regex / %identifier 的 DFA 表、标识符识别与关键字接口。

    返回 (decls, tables)：
      decls：类型与函数声明（单文件布局内联在 token 模块，分文件布局同时
             写进 parser_token.h）；tables：表定义与函数实现。
    """
    entries = list(regex_defs.items())
    patterns = list(entries)
    if identifier_pattern is not None:
        patterns.append(("__identifier__", identifier_pattern))
    decls = [
        "#ifndef PARSER_LEX_DFA_DECLARED\n"
        "#define PARSER_LEX_DFA_DECLARED\n\n"
        "/* ============ 正则 Token 与标识符识别"
        "（由 %regex / %identifier 生成） ============ */\n"
        "/** @brief 关键字查表。 @param pText 文本。\n"
        " *  @retval 关键字对应的 Token；不是关键字时返回 TOKEN_INVALID。 */\n"
        "ParserTokenType_T Parser_TokenKeyword(const char *pText);\n"
    ]
    body = [_generator_keyword_c(token_defs)]
    if patterns:
        cells, char_map, compiled = _regex_compile_all(patterns)
        class_count = len(cells)
        decls.append(
            "/** DFA 描述：转移表按“状态 × 字符类”展平。 */\n"
            "typedef struct ParserLexDfa_T\n"
            "{\n"
            "    const uint16_t *pTrans;\n"
            "    const uint8_t *pAccept;\n"
            "    uint16_t stateCount;\n"
            "    uint16_t startState;\n"
            "    uint16_t trapState;\n"
            "} ParserLexDfa_T;\n\n"
            "/** @brief 用 DFA 做最长前缀匹配。 @param pDfa DFA。\n"
            " *  @param pText 文本。 @retval 匹配到的字节数（0 表示不匹配）。 */\n"
            "uint32_t Parser_LexDfaMatch(const ParserLexDfa_T *pDfa,"
            " const char *pText);\n\n"
            "extern const uint8_t s_ParserLexClassMap[256];\n"
            "extern const uint16_t s_ParserLexClassCount;\n"
        )
        if entries:
            decls.append(
                "#define PARSER_REGEX_COUNT %du\n"
                "extern const ParserLexDfa_T"
                " s_ParserRegexDfas[PARSER_REGEX_COUNT];\n"
                "extern const ParserTokenType_T"
                " s_ParserRegexTypes[PARSER_REGEX_COUNT];\n"
                % len(entries)
            )
        if identifier_pattern is not None:
            decls.append(
                "\n/** @brief 取最长标识符前缀长度。 @param pText 文本。\n"
                " *  @retval 长度（0 表示不是标识符开头）。 */\n"
                "uint32_t Parser_TokenIdentifierLength(const char *pText);\n\n"
                "/** @brief 整串是否是标识符。 @param pText 文本。\n"
                " *  @retval 1 是标识符。 */\n"
                "uint8_t Parser_TokenIsIdentifier(const char *pText);\n"
            )
        # 字符类映射：字节 -> 类编号
        rows = []
        for start in range(0, 256, 16):
            rows.append("    " + ", ".join(
                "%du" % char_map[code]
                for code in range(start, start + 16)
            ))
        body.append(
            "/* 字符类划分：共 %d 类，'\\0' 之外的 256 个字节各归一类 */\n"
            "const uint8_t s_ParserLexClassMap[256] =\n{\n%s\n};\n"
            "const uint16_t s_ParserLexClassCount = %du;\n\n"
            % (class_count, ",\n".join(rows), class_count)
        )
        descriptors = []
        for item in compiled:
            name = item["name"]
            pattern = item["pattern"]
            dfa = item["dfa"]
            identifier = (name == "__identifier__")
            prefix = ("s_ParserIdentifierDfa" if identifier
                      else "s_ParserDfa" + name)
            trans_name = prefix + "Trans"
            accept_name = prefix + "Accept"
            label = "标识符" if identifier else name
            trans_rows = ",\n".join(
                "    { %s }" % ", ".join(
                    "%du" % dfa["transitions"][state][cls]
                    for cls in range(class_count)
                )
                for state in range(dfa["stateCount"])
            )
            accept_row = ", ".join(
                "1u" if state in dfa["accept"] else "0u"
                for state in range(dfa["stateCount"])
            )
            body.append(
                "/* %s：%s（%d 状态 × %d 字符类） */\n"
                "static const uint16_t %s[%d][%d] =\n{\n%s\n};\n"
                "static const uint8_t %s[%d] = { %s };\n\n"
                % (label, pattern, dfa["stateCount"], class_count,
                   trans_name, dfa["stateCount"], class_count, trans_rows,
                   accept_name, dfa["stateCount"], accept_row)
            )
            descriptors.append((
                name,
                "{ &%s[0][0], %s, %du, %du, %du }"
                % (trans_name, accept_name, dfa["stateCount"],
                   dfa["start"], dfa["trap"])
            ))

        if entries:
            regex_rows = ",\n".join(
                "    " + text for name, text in descriptors
                if name != "__identifier__"
            )
            type_rows = ",\n".join(
                "    TOKEN_%s" % name for name, _ in entries
            )
            body.append(
                "/* %%regex 声明的 Token DFA：与固定字面量一起做最长匹配 */\n"
                "const ParserLexDfa_T"
                " s_ParserRegexDfas[PARSER_REGEX_COUNT] =\n{\n%s\n};\n"
                "const ParserTokenType_T"
                " s_ParserRegexTypes[PARSER_REGEX_COUNT] =\n{\n%s\n};\n\n"
                % (regex_rows, type_rows)
            )
        if identifier_pattern is not None:
            identifier_desc = next(
                text for name, text in descriptors
                if name == "__identifier__"
            )
            body.append(
                "/* %%identifier 声明的标识符 DFA */\n"
                "static const ParserLexDfa_T s_ParserIdentifierDfa =\n"
                "    %s;\n\n" % identifier_desc
            )

        body.append(
            "/** @brief 用 DFA 做最长前缀匹配。 @param pDfa DFA。\n"
            " *  @param pText 文本。 @retval 匹配到的字节数（0 表示不匹配）。 */\n"
            "uint32_t Parser_LexDfaMatch(const ParserLexDfa_T *pDfa,"
            " const char *pText)\n"
            "{\n"
            "    uint32_t length = 0u;\n"
            "    uint32_t best = 0u;\n"
            "    uint16_t state = 0u;\n"
            "    if ((pDfa == NULL) || (pText == NULL)) { return 0u; }\n"
            "    state = pDfa->startState;\n"
            "    while (pText[length] != '\\0') {\n"
            "        uint8_t cls ="
            " s_ParserLexClassMap[(unsigned char)pText[length]];\n"
            "        state = pDfa->pTrans[(uint32_t)state *"
            " (uint32_t)s_ParserLexClassCount + cls];\n"
            "        if (state == pDfa->trapState) { break; }\n"
            "        length++;\n"
            "        if (pDfa->pAccept[state] != 0u) { best = length; }\n"
            "    }\n"
            "    return best;\n"
            "}\n\n"
        )
        if identifier_pattern is not None:
            body.append(
                "/** @brief 取最长标识符前缀长度。 @param pText 文本。\n"
                " *  @retval 长度（0 表示不是标识符开头）。 */\n"
                "uint32_t Parser_TokenIdentifierLength(const char *pText)\n"
                "{\n"
                "    if (pText == NULL) { return 0u; }\n"
                "    return Parser_LexDfaMatch(&s_ParserIdentifierDfa,"
                " pText);\n"
                "}\n\n"
                "/** @brief 整串是否是标识符。 @param pText 文本。\n"
                " *  @retval 1 是标识符。 */\n"
                "uint8_t Parser_TokenIsIdentifier(const char *pText)\n"
                "{\n"
                "    uint32_t length = 0u;\n"
                "    if (pText == NULL) { return 0u; }\n"
                "    length = Parser_TokenIdentifierLength(pText);\n"
                "    return ((length > 0u) && (pText[length] == '\\0'))"
                " ? 1u : 0u;\n"
                "}\n\n"
            )
    decls.append("\n#endif /* PARSER_LEX_DFA_DECLARED */\n")
    return "".join(decls), "".join(body)


def _generator_lex_rules(token_defs):
    """生成按长度降序排列的固定字符串 Lexer 表。"""
    rules = []
    definition_order = 0
    for token_name, literals in token_defs.items():
        for literal in literals:
            rules.append((literal, token_name, definition_order))
            definition_order += 1
    rules.sort(key=lambda item: (-len(item[0]), item[2]))
    if not rules:
        return "    { NULL, 0u, TOKEN_END }"
    return ",\n".join(
        "    { %s, %du, TOKEN_%s }"
        % (_var_c_string(literal), len(literal.encode("utf-8")), token_name)
        for literal, token_name, _ in rules
    )


def _generator_action_cases(analysis):
    """生成每条最终产生式对应的语义动作分支。"""
    cases = []
    for prod in analysis["productions"]:
        if prod.get("transform") == "left_empty":
            continue
        action = prod.get("action", "").strip()
        translated = (_translate_action(action) if action
                      else "result.value = NULL;")
        cases.append(
            "        case %du:\n        {\n            %s\n"
            "            break;\n        }"
            % (prod["id"], translated.replace("\n", "\n            "))
        )
    return "\n".join(cases)


def _generator_expand_cases(analysis, terminals, nonterm_ids):
    """生成产生式展开分支，所有调用只操作显式分析栈。"""
    terminal_set = set(terminals)
    cases = []

    def push_symbol(symbol, slot, seed="PARSER_INVALID_INDEX"):
        """生成一个终结符或非终结符压栈语句。"""
        if symbol in terminal_set:
            kind = "PARSER_ITEM_TERMINAL"
            symbol_id = "TOKEN_" + symbol
        else:
            kind = "PARSER_ITEM_NONTERM"
            symbol_id = str(nonterm_ids[symbol]) + "u"
        return (
            "            if (Parser_Push(pCtx, %s, %s, frameIndex, "
            "%du, %s) != PARSER_OK) { return PARSER_ERR_STACK; }"
            % (kind, symbol_id, slot, seed)
        )

    for prod in analysis["productions"]:
        rhs = prod["rhs"]
        transform = prod.get("transform")
        lines = ["        case %du:" % prod["id"], "        {"]
        if transform == "left_base":
            base_length = prod["base_rhs_length"]
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_LEFT_LOOP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % nonterm_ids[rhs[-1]]
            )
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION_KEEP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
            symbols = rhs[:base_length]
            start_slot = 1
        elif transform == "left_continue":
            symbols = rhs[:-1]
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_LEFT_LOOP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % nonterm_ids[rhs[-1]]
            )
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION_KEEP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
            start_slot = 2
        elif transform == "left_empty":
            symbols = []
            start_slot = 1
            lines.append("            /* 空尾规则由 LEFT_LOOP 直接完成。 */")
        else:
            symbols = rhs
            start_slot = 1
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
        for index in range(len(symbols) - 1, -1, -1):
            lines.append(push_symbol(symbols[index], start_slot + index))
        lines.extend(["            break;", "        }"])
        cases.append("\n".join(lines))
    return "\n".join(cases)


def _generator_lexer_fixed_block_c(has_regex):
    """Lexer 里“固定字面量（必要时与 %regex DFA 一起）最长匹配”的代码。"""
    if has_regex:
        # 声明了 %regex 时，字面量的匹配挪到正则块里一起做最长匹配
        return ""
    return r'''    /* 固定规则按最长候选优先，并在关键字末尾检查标识符边界。 */
    for (index = 0u; index < (uint32_t)(sizeof(s_ParserLexRules) /
                                        sizeof(s_ParserLexRules[0])); index++) {
        const ParserLexRule_T *pRule = &s_ParserLexRules[index];
        if ((pRule->pText != NULL) &&
            (strncmp(pStart, pRule->pText, pRule->length) == 0)) {
            unsigned char last = (unsigned char)pRule->pText[pRule->length - 1u];
            unsigned char next = (unsigned char)pStart[pRule->length];
            if (((isalnum(last) != 0) || (last == '_')) &&
                ((isalnum(next) != 0) || (next == '_'))) {
                continue;
            }
            for (length = 0u; length < pRule->length; length++) {
                Parser_ReadChar(pCtx);
            }
            pToken->type = pRule->type;
            return PARSER_OK;
        }
    }
'''


def _generator_lexer_regex_block_c(has_regex):
    """Lexer 里 %regex 与固定字面量一起做最长匹配的代码（放在内建 NUMBER 之前）。"""
    if not has_regex:
        return ""
    return r'''    /* 固定字面量与 %regex 的 DFA 一起做最长匹配，字面量优先。 */
    {
        uint32_t bestLength = 0u;
        uint32_t regexIndex = 0u;
        ParserTokenType_T bestType = TOKEN_INVALID;
        uint8_t bestFromRegex = 0u;
        for (index = 0u; index < (uint32_t)(sizeof(s_ParserLexRules) /
                                            sizeof(s_ParserLexRules[0]));
             index++) {
            const ParserLexRule_T *pRule = &s_ParserLexRules[index];
            if ((pRule->pText != NULL) &&
                (strncmp(pStart, pRule->pText, pRule->length) == 0)) {
                unsigned char last =
                    (unsigned char)pRule->pText[pRule->length - 1u];
                unsigned char next = (unsigned char)pStart[pRule->length];
                if (((isalnum(last) != 0) || (last == '_')) &&
                    ((isalnum(next) != 0) || (next == '_'))) {
                    continue;
                }
                if ((uint32_t)pRule->length > bestLength) {
                    bestLength = (uint32_t)pRule->length;
                    bestType = pRule->type;
                    bestFromRegex = 0u;
                }
            }
        }
        for (regexIndex = 0u; regexIndex < PARSER_REGEX_COUNT;
             regexIndex++) {
            uint32_t matched = Parser_LexDfaMatch(
                &s_ParserRegexDfas[regexIndex], pStart);
            if (matched > bestLength) {
                bestLength = matched;
                bestType = s_ParserRegexTypes[regexIndex];
                bestFromRegex = 1u;
            }
        }
        if (bestType != TOKEN_INVALID) {
            if (bestLength >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
            for (length = 0u; length < bestLength; length++) {
                Parser_ReadChar(pCtx);
            }
            pToken->type = bestType;
            if (bestFromRegex != 0u) {
                memcpy(pCtx->lexeme, pStart, bestLength);
                pCtx->lexeme[bestLength] = '\0';
                pToken->pName = pCtx->lexeme;
            }
            return PARSER_OK;
        }
    }
'''


def _generator_lexer_identifier_block_c(has_identifier):
    """Lexer 里标识符（%identifier 的 DFA 或内建 C 标识符规则）的代码。"""
    if not has_identifier:
        return r'''    /* C 风格标识符默认映射到 VARIABLE。 */
    if ((isalpha((unsigned char)*pStart) != 0) || (*pStart == '_')) {
        while ((isalnum((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) ||
               (pCtx->pText[pCtx->textPosition] == '_')) {
            if (length + 1u >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
            pCtx->lexeme[length++] = Parser_ReadChar(pCtx);
        }
        pCtx->lexeme[length] = '\0';
        pToken->type = TOKEN_VARIABLE;
        pToken->pName = pCtx->lexeme;
        return PARSER_OK;
    }
'''
    return r'''    /* 标识符：用 %identifier 的 DFA 扫描，再查关键字表。 */
    {
        uint32_t identLength = Parser_TokenIdentifierLength(pStart);
        if (identLength > 0u) {
            ParserTokenType_T keyword = TOKEN_INVALID;
            if (identLength >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
            memcpy(pCtx->lexeme, pStart, identLength);
            pCtx->lexeme[identLength] = '\0';
            for (length = 0u; length < identLength; length++) {
                Parser_ReadChar(pCtx);
            }
            keyword = Parser_TokenKeyword(pCtx->lexeme);
            pToken->type = (keyword != TOKEN_INVALID) ? keyword
                                                      : TOKEN_VARIABLE;
            pToken->pName = pCtx->lexeme;
            return PARSER_OK;
        }
    }
'''


def _generator_lexer_c(has_regex=False, has_identifier=False):
    """返回生成式 Lexer 与 Token 前移函数的 C 模板。

    has_regex：文法里声明了 %regex，固定字面量与 DFA 一起做最长匹配；
    has_identifier：声明了 %identifier，标识符改由 DFA + 关键字表识别。
    """
    text = r'''
/** @brief 从脚本文本产生 Token。 @param pCtx 上下文。 @param pToken 输出 Token。 @retval 状态码。 */
static int32_t Parser_LexNext(ParserCtx_T *pCtx, ParserToken_T *pToken)
{
    const char *pStart = NULL;
    char *pEnd = NULL;
    uint32_t length = 0u;
    uint32_t index = 0u;
    uint32_t outputIndex = 0u;
    uint8_t hasDot = 0u;
    uint8_t hasExponent = 0u;
    char suffix = '\0';

    while (isspace((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
        Parser_ReadChar(pCtx);
    }
    memset(pToken, 0, sizeof(*pToken));
    pToken->line = pCtx->line;
    pToken->column = pCtx->column;
    pStart = &pCtx->pText[pCtx->textPosition];
    if (*pStart == '\0') { pToken->type = TOKEN_END; return PARSER_OK; }

    /* 双引号字符串在词法阶段完成反转义。 */
    if (*pStart == '"') {
        Parser_ReadChar(pCtx);
        while ((pCtx->pText[pCtx->textPosition] != '\0') &&
               (pCtx->pText[pCtx->textPosition] != '"')) {
            char ch = Parser_ReadChar(pCtx);
            if (ch == '\\') {
                char escaped = Parser_ReadChar(pCtx);
                switch (escaped) {
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    case '0': return PARSER_ERR_LEXER;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    default: return PARSER_ERR_LEXER;
                }
            }
            if (outputIndex + 1u >= sizeof(pCtx->lexeme)) {
                return PARSER_ERR_LEXER;
            }
            pCtx->lexeme[outputIndex++] = ch;
        }
        if (pCtx->pText[pCtx->textPosition] != '"') {
            return PARSER_ERR_LEXER;
        }
        Parser_ReadChar(pCtx);
        pCtx->lexeme[outputIndex] = '\0';
        pToken->type = TOKEN_STRING;
        pToken->pValue = Var_CreateString(pCtx->lexeme);
        return (pToken->pValue != NULL) ? PARSER_OK : VAR_RUNTIME_ERR_MEMORY;
    }

__LEX_REGEX_BLOCK__
    /* 小数点和指数决定 float，后缀 d 决定 double。 */
    if ((isdigit((unsigned char)*pStart) != 0) ||
        ((*pStart == '.') && (isdigit((unsigned char)pStart[1]) != 0))) {
        while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            Parser_ReadChar(pCtx);
        }
        if (pCtx->pText[pCtx->textPosition] == '.') {
            hasDot = 1u;
            Parser_ReadChar(pCtx);
            while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
                Parser_ReadChar(pCtx);
            }
        }
        if ((pCtx->pText[pCtx->textPosition] == 'e') ||
            (pCtx->pText[pCtx->textPosition] == 'E')) {
            hasExponent = 1u;
            Parser_ReadChar(pCtx);
            if ((pCtx->pText[pCtx->textPosition] == '+') ||
                (pCtx->pText[pCtx->textPosition] == '-')) {
                Parser_ReadChar(pCtx);
            }
            if (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) == 0) {
                return PARSER_ERR_LEXER;
            }
            while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
                Parser_ReadChar(pCtx);
            }
        }
        if (isalpha((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            suffix = Parser_ReadChar(pCtx);
        }
        if (isalpha((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            return PARSER_ERR_LEXER;
        }
        length = (uint32_t)(&pCtx->pText[pCtx->textPosition] - pStart);
        if (length >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
        memcpy(pCtx->lexeme, pStart, length);
        pCtx->lexeme[length] = '\0';
        if (suffix != '\0') { pCtx->lexeme[length - 1u] = '\0'; }
        errno = 0;
        if ((suffix == 'u') || (suffix == 'U')) {
            unsigned long value = 0ul;
            if ((hasDot != 0u) || (hasExponent != 0u)) {
                return PARSER_ERR_LEXER;
            }
            value = strtoul(pCtx->lexeme, &pEnd, 10);
            if ((errno == ERANGE) || (*pEnd != '\0') || (value > UINT32_MAX)) {
                return VAR_RUNTIME_ERR_RANGE;
            }
            pToken->pValue = Var_CreateUint32((uint32_t)value);
        } else if ((suffix == 'd') || (suffix == 'D')) {
            double value = 0.0;
            if ((hasDot == 0u) && (hasExponent == 0u)) {
                return PARSER_ERR_LEXER;
            }
            value = strtod(pCtx->lexeme, &pEnd);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (isfinite(value) == 0)) { return VAR_RUNTIME_ERR_RANGE; }
            pToken->pValue = Var_CreateDouble(value);
        } else if (suffix != '\0') {
            return PARSER_ERR_LEXER;
        } else if ((hasDot != 0u) || (hasExponent != 0u)) {
            float value = strtof(pCtx->lexeme, &pEnd);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (isfinite(value) == 0)) { return VAR_RUNTIME_ERR_RANGE; }
            pToken->pValue = Var_CreateFloat(value);
        } else {
            long value = strtol(pCtx->lexeme, &pEnd, 10);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (value < INT32_MIN) || (value > INT32_MAX)) {
                return VAR_RUNTIME_ERR_RANGE;
            }
            pToken->pValue = Var_CreateInt((int32_t)value);
        }
        pToken->type = TOKEN_NUMBER;
        return (pToken->pValue != NULL) ? PARSER_OK : VAR_RUNTIME_ERR_MEMORY;
    }

__LEX_FIXED_BLOCK__
__LEX_IDENTIFIER_BLOCK__
    return PARSER_ERR_LEXER;
}

/** @brief 前移 Token。 @param pCtx 上下文。 @retval 状态码。 */
static int32_t Parser_Advance(ParserCtx_T *pCtx)
{
    int32_t ret = PARSER_OK;
    if ((pCtx->ownsCurrentValue != 0u) && (pCtx->current.pValue != NULL)) {
        Var_Free(pCtx->current.pValue);
    }
    pCtx->ownsCurrentValue = 0u;
    if (pCtx->textMode != 0u) {
        ret = Parser_LexNext(pCtx, &pCtx->current);
        if (ret != PARSER_OK) {
            Parser_SetError(ret, pCtx->current.line, pCtx->current.column,
                            "词法解析失败");
            pCtx->current.type = TOKEN_INVALID;
            return ret;
        }
        pCtx->ownsCurrentValue = (pCtx->current.pValue != NULL) ? 1u : 0u;
    } else if (pCtx->tokenPosition < pCtx->tokenCount) {
        pCtx->current = pCtx->pTokens[pCtx->tokenPosition++];
    } else {
        memset(&pCtx->current, 0, sizeof(pCtx->current));
        pCtx->current.type = TOKEN_END;
    }
    return PARSER_OK;
}
'''
    text = text.replace(
        "__LEX_REGEX_BLOCK__", _generator_lexer_regex_block_c(has_regex)
    ).replace(
        "__LEX_FIXED_BLOCK__", _generator_lexer_fixed_block_c(has_regex)
    ).replace(
        "__LEX_IDENTIFIER_BLOCK__",
        _generator_lexer_identifier_block_c(has_identifier)
    )
    return text


def _generator_parse_loop_c():
    """返回显式分析栈主循环和两个公开入口。"""
    return r'''
/** @brief 执行非递归预测分析。 @param pCtx 上下文。 @retval 结果或 NULL。 */
static VarValue_T *Parser_ParseLoop(ParserCtx_T *pCtx)
{
    if (Parser_Push(pCtx, PARSER_ITEM_NONTERM, __START_ID__u,
                    PARSER_INVALID_INDEX, 0u,
                    PARSER_INVALID_INDEX) != PARSER_OK) {
        return NULL;
    }
    while (pCtx->stackTop > 0u) {
        ParserStackItem_T item = pCtx->stack[--pCtx->stackTop];
        if (item.kind == PARSER_ITEM_TERMINAL) {
            if ((uint16_t)pCtx->current.type != item.symbol) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "终结符不匹配");
                return NULL;
            }
            if (Parser_CaptureTerminal(pCtx, item.frameIndex,
                                       item.valueIndex) != PARSER_OK) {
                return NULL;
            }
            if (Parser_Advance(pCtx) != PARSER_OK) { return NULL; }
        } else if (item.kind == PARSER_ITEM_NONTERM) {
            int16_t production = -1;
            uint16_t frameIndex = 0u;
            ParserSemanticFrame_T *pFrame = NULL;
            if (((uint16_t)pCtx->current.type >= __TERMINAL_COUNT__u) ||
                (item.symbol >= __NONTERM_COUNT__u)) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "没有可用产生式");
                return NULL;
            }
            production = s_ParserTable[item.symbol][pCtx->current.type];
            if (production < 0) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "预测分析表没有匹配项");
                return NULL;
            }
            if (pCtx->frameTop >= PARSER_MAX_STACK_DEPTH) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "语义帧栈已满");
                return NULL;
            }
            frameIndex = pCtx->frameTop++;
            pFrame = &pCtx->frames[frameIndex];
            memset(pFrame, 0, sizeof(*pFrame));
            pFrame->parentFrameIndex = item.frameIndex;
            pFrame->parentValueIndex = item.valueIndex;
            if (item.seedFrameIndex != PARSER_INVALID_INDEX) {
                pFrame->values[1] =
                    pCtx->frames[item.seedFrameIndex].pResult;
            }
            if (Parser_ExpandProduction(pCtx, (uint16_t)production,
                                        frameIndex) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "分析栈已满");
                return NULL;
            }
        } else if (item.kind == PARSER_ITEM_LEFT_LOOP) {
            int16_t production = -1;
            ParserSemanticFrame_T *pFrame = &pCtx->frames[item.frameIndex];
            if (((uint16_t)pCtx->current.type >= __TERMINAL_COUNT__u) ||
                (item.symbol >= __NONTERM_COUNT__u)) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column,
                                "左递归尾部没有可用产生式");
                return NULL;
            }
            production = s_ParserTable[item.symbol][pCtx->current.type];
            if (production < 0) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "左递归尾部预测失败");
                return NULL;
            }
            if (s_ParserProductionIsLeftEmpty[production] != 0u) {
                if (Parser_CompleteFrame(pCtx, item.frameIndex) != PARSER_OK) {
                    return NULL;
                }
            } else {
                if (pFrame->pResult != NULL) {
                    pFrame->values[1] = Var_CloneValue(pFrame->pResult);
                    if (pFrame->values[1] == NULL) { return NULL; }
                    pFrame->valueOwned[1] = 1u;
                }
                if (Parser_ExpandProduction(pCtx, (uint16_t)production,
                                            item.frameIndex) != PARSER_OK) {
                    Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                    pCtx->current.column, "分析栈已满");
                    return NULL;
                }
            }
        } else {
            ParserSemanticFrame_T *pFrame = &pCtx->frames[item.frameIndex];
            if (item.kind == PARSER_ITEM_FORWARD) {
                if (Parser_ForwardFrame(pFrame, item.valueIndex) != PARSER_OK) {
                    return NULL;
                }
            } else if (Parser_ExecuteAction(pCtx, item.symbol,
                                            item.frameIndex) != PARSER_OK) {
                return NULL;
            }
            if ((item.kind != PARSER_ITEM_ACTION_KEEP) &&
                (Parser_CompleteFrame(pCtx, item.frameIndex) != PARSER_OK)) {
                return NULL;
            }
        }
    }
    if (pCtx->current.type != TOKEN_END) {
        Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                        pCtx->current.column, "输入尚未完全消费");
        return NULL;
    }
    return pCtx->pRootResult;
}

/** @brief 解析 Token 数组。 @param pTokens Token 数组。 @param count 数量。 @retval 结果或 NULL。 */
VarValue_T *Parser_Run(const ParserToken_T *pTokens, uint32_t count)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pTokens = pTokens;
    pContext->tokenCount = count;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoop(pContext);
    if (pResult == NULL) { Parser_ReleaseContext(pContext); }
    return pResult;
}

/** @brief 解析脚本文本。 @param pText 输入文本。 @retval 结果或 NULL。 */
VarValue_T *Parser_RunText(const char *pText)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    if (pText == NULL) { return NULL; }
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pText = pText;
    pContext->textMode = 1u;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoop(pContext);
    if ((pContext->ownsCurrentValue != 0u) &&
        (pContext->current.pValue != NULL)) {
        Var_Free(pContext->current.pValue);
    }
    if (pResult == NULL) { Parser_ReleaseContext(pContext); }
    return pResult;
}
'''


def _generator_ll_item_types_c():
    """LL(1) 专属结构：分析项目、显式栈项目与语义帧。"""
    return r'''typedef enum __ParserItemKind_T
{
    PARSER_ITEM_TERMINAL = 0,
    PARSER_ITEM_NONTERM,
    PARSER_ITEM_ACTION,
    PARSER_ITEM_ACTION_KEEP,
    PARSER_ITEM_FORWARD,
    PARSER_ITEM_LEFT_LOOP,
    PARSER_ITEM_TREE_END
} ParserItemKind_T;

typedef struct __ParserStackItem_T
{
    ParserItemKind_T kind;
    uint16_t symbol;
    uint16_t frameIndex;
    uint16_t valueIndex;
    uint16_t seedFrameIndex;
} ParserStackItem_T;

typedef struct __ParserSemanticFrame_T
{
    uint16_t parentFrameIndex;
    uint16_t parentValueIndex;
    VarValue_T *values[PARSER_VALUE_SLOTS];
    uint8_t valueOwned[PARSER_VALUE_SLOTS];
    char names[PARSER_VALUE_SLOTS][PARSER_MAX_NAME_LENGTH];
    VarValue_T *pResult;
    uint8_t resultOwned;
} ParserSemanticFrame_T;

'''


def _generator_ll_ctx_fields_c():
    """LL(1) 专属的上下文分析栈与语义帧字段。"""
    return """    ParserStackItem_T stack[PARSER_MAX_STACK_DEPTH];
    uint16_t stackTop;
    ParserSemanticFrame_T frames[PARSER_MAX_STACK_DEPTH];
    uint16_t frameTop;
"""


def _generator_ll_tables_c(analysis, terminals, nonterms):
    """LL(1) 的预测分析表与左递归空尾标记表。"""
    table_rows = []
    for nonterm in nonterms:
        row = [str(analysis["table"].get((nonterm, token), -1))
               for token in terminals]
        table_rows.append("    { %s }" % ", ".join(row))
    left_empty = ", ".join(
        "1u" if prod.get("transform") == "left_empty" else "0u"
        for prod in analysis["productions"]
    )
    return (
        "static const int16_t s_ParserTable[%d][%d] =\n{\n%s\n};\n"
        "static const uint8_t s_ParserProductionIsLeftEmpty[] =\n{\n"
        "    %s\n};\n"
        % (len(nonterms), len(terminals), ",\n".join(table_rows), left_empty)
    )


def _generator_lr_stack_c(mode_label="SLR(1)", mode_arg="slr", tree=False):
    """返回 LR 的状态栈 / 符号栈 / 语义值栈定义（SLR / LALR 共用）。

    默认参数保证 SLR 产物逐字节不变；LALR 只改注释里的模式名。
    tree=True 时（%tree）额外给每个栈槽位挂一个 CST 节点指针，
    移进时存叶子、归约时存父节点，供 Parser_ParseTreeText 取走根节点。
    """
    text = r'''
/* ============ __LR_LABEL__ 运行时（由 %mode __LR_ARG__ 启用） ============ */
typedef struct __ParserLRStack_T
{
    uint16_t states[PARSER_MAX_STACK_DEPTH];
    uint16_t symbols[PARSER_MAX_STACK_DEPTH];
    VarValue_T *values[PARSER_MAX_STACK_DEPTH];
    uint8_t valueOwned[PARSER_MAX_STACK_DEPTH];
    char names[PARSER_MAX_STACK_DEPTH][PARSER_MAX_NAME_LENGTH];
__LR_NODE_FIELD__
    uint16_t top;
} ParserLRStack_T;
'''
    return text.replace("__LR_LABEL__", mode_label).replace(
        "__LR_ARG__", mode_arg
    ).replace(
        "__LR_NODE_FIELD__",
        ("    VarValue_T *nodes[PARSER_MAX_STACK_DEPTH];\n" if tree else
         ""),
    )


def _generator_lr_tables_c(analysis):
    """生成 SLR(1) 的 ACTION / GOTO 表与产生式长度、左部编号表。"""
    terminals = analysis["terminals"]
    nonterms = analysis["nonterms"]
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    states = analysis["states"]
    action = analysis["action"]
    goto = analysis["goto"]
    productions = analysis["productions"]
    action_rows = []
    for state_id in range(len(states)):
        row = []
        for token in terminals:
            entry = action.get((state_id, token))
            if entry is None:
                row.append("0")
            elif entry[0] == "shift":
                row.append("%d" % (entry[1] + 1))
            elif entry[0] == "reduce":
                row.append("%d" % (-entry[1] - 1))
            else:
                row.append("INT32_MAX")
        action_rows.append("    { %s }" % ", ".join(row))
    goto_rows = []
    for state_id in range(len(states)):
        row = []
        for nonterm in nonterms:
            target = goto.get((state_id, nonterm_ids[nonterm]))
            row.append(str(target) if target is not None else "UINT16_MAX")
        goto_rows.append("    { %s }" % ", ".join(row))
    lengths = ", ".join(
        "%du" % len(prod["rhs"]) for prod in productions
    )
    lhs_ids = ", ".join(
        "%du" % nonterm_ids[prod["lhs"]] for prod in productions
    )
    return (
        "/* ACTION：0 出错，>0 移进到 action-1，<0 归约 -action-1，"
        "INT32_MAX 接受；GOTO：UINT16_MAX 出错。 */\n"
        "static const int32_t s_ParserAction[%d][%d] =\n{\n%s\n};\n"
        "static const uint16_t s_ParserGoto[%d][%d] =\n{\n%s\n};\n"
        "static const uint16_t s_ParserProductionLength[] =\n{\n    %s\n};\n"
        "static const uint16_t s_ParserProductionLhs[] =\n{\n    %s\n};\n"
        % (len(states), len(terminals), ",\n".join(action_rows),
           len(states), len(nonterms), ",\n".join(goto_rows),
           lengths, lhs_ids)
    )


def _generator_lr_action_cases(analysis):
    """生成 SLR / LALR 归约时执行的语义动作分支。

    $n 由 _ActionTranslator 的 LR 分支翻译成 LR 栈槽位访问，
    因此这里只负责把产生式编号映射到一段可执行语句。
    """
    translate_mode = analysis.get("mode", "slr")
    if translate_mode not in ("slr", "lalr"):
        translate_mode = "slr"
    cases = []
    for prod in analysis["productions"]:
        action = prod.get("action", "").strip()
        translated = (_translate_action(
                          action, translate_mode, len(prod["rhs"]))
                      if action else "result.value = NULL;")
        cases.append(
            "        case %du:\n        {\n            %s\n"
            "            break;\n        }"
            % (prod["id"], translated.replace("\n", "\n            "))
        )
    return "\n".join(cases)


def _lr_tree_markers(tree):
    """返回 LR 运行时模板里的 %tree 代码片段（tree=False 时全部为空串）。

    片段以 __LR_*__ 标记注入 _generator_lr_parse_loop_c 的模板，
    保证不开 %tree 时 SLR / LALR 的产物与既有版本逐字节一致。
    """
    values = dict.fromkeys([
        "__LR_RELEASE_NODES__",
        "__LR_SHIFT_MODE_PARAM__",
        "__LR_SHIFT_LEAF_DECL__",
        "__LR_SHIFT_LEAF_CREATE__",
        "__LR_SHIFT_LEAF_FREE__",
        "__LR_SHIFT_LEAF_STORE__",
        "__LR_REDUCE_MODE_PARAM__",
        "__LR_REDUCE_NODE_DECL__",
        "__LR_REDUCE_FREE_NODE__",
        "__LR_REDUCE_STORE_NODE__",
        "__LR_LOOP_TREE_PARAM__",
        "__LR_LOOP_TREE_DECL__",
        "__LR_LOOP_TREE_INIT__",
        "__LR_LOOP_MODE_ARG__",
        "__LR_LOOP_ACCEPT__",
        "__LR_LOOP_CALL__",
    ], "")
    values["__LR_REDUCE_BEGIN__"] = "    switch (production) {\n"
    values["__LR_REDUCE_END__"] = ("        default: return PARSER_ERR_SYNTAX;\n"
                                   "    }\n")
    if not tree:
        return values
    values.update({
        "__LR_RELEASE_NODES__": (
            "        if (pStack->nodes[index] != NULL) {\n"
            "            Var_Free(pStack->nodes[index]);\n"
            "            pStack->nodes[index] = NULL;\n"
            "        }\n"
        ),
        "__LR_SHIFT_MODE_PARAM__": ",\n                              uint8_t treeMode",
        "__LR_SHIFT_LEAF_DECL__": "    VarValue_T *pLeaf = NULL;\n",
        "__LR_SHIFT_LEAF_CREATE__": (
            "    if ((treeMode != 0u) &&\n"
            "        (Parser_TreeMakeLeaf(pCtx, &pLeaf) != PARSER_OK)) {\n"
            "        Var_Free(pValue);\n"
            "        return VAR_RUNTIME_ERR_MEMORY;\n"
            "    }\n"
        ),
        "__LR_SHIFT_LEAF_FREE__": "        Var_Free(pLeaf);\n",
        "__LR_SHIFT_LEAF_STORE__":
            "    pStack->nodes[pStack->top - 1u] = pLeaf;\n",
        "__LR_REDUCE_MODE_PARAM__": ",\n                               uint8_t treeMode",
        "__LR_REDUCE_NODE_DECL__": "    VarValue_T *pNode = NULL;\n",
        "__LR_REDUCE_BEGIN__": (
            "    if (treeMode != 0u) {\n"
            "        if (Parser_TreeMakeBranch(\n"
            "                stack, lhs, base, length, &pNode) != PARSER_OK) {\n"
            "            Parser_SetError(VAR_RUNTIME_ERR_MEMORY,\n"
            "                            pCtx->current.line,\n"
            "                            pCtx->current.column,\n"
            "                            \"语法树创建失败\");\n"
            "            return VAR_RUNTIME_ERR_MEMORY;\n"
            "        }\n"
            "    } else {\n"
            "        switch (production) {\n"
        ),
        "__LR_REDUCE_END__": (
            "        default: return PARSER_ERR_SYNTAX;\n"
            "        }\n"
            "    }\n"
        ),
        "__LR_REDUCE_FREE_NODE__": "        Var_Free(pNode);\n",
        "__LR_REDUCE_STORE_NODE__":
            "    stack->nodes[stack->top - 1u] = pNode;\n",
        "__LR_LOOP_TREE_PARAM__":
            ",\n                                     VarValue_T **ppTree",
        "__LR_LOOP_TREE_DECL__": "    uint8_t treeMode = 0u;\n",
        "__LR_LOOP_TREE_INIT__":
            "    if (ppTree != NULL) { *ppTree = NULL; treeMode = 1u; }\n",
        "__LR_LOOP_MODE_ARG__": ", treeMode",
        "__LR_LOOP_ACCEPT__": (
            "            if (ppTree != NULL) {\n"
            "                *ppTree = stack.nodes[stack.top - 1u];\n"
            "                stack.nodes[stack.top - 1u] = NULL;\n"
            "            }\n"
        ),
        "__LR_LOOP_CALL__": ", NULL",
    })
    return values


def _generator_lr_parse_loop_c(mode_label="SLR(1)", mode_name="SLR",
                               tree=False):
    """返回 LR 主循环和 Parser_Run / Parser_RunText 的 C 模板。

    SLR / LALR 共用同一份运行时；tree=False（缺省）时只替换注释与错误消息
    里的模式名，产物与既有版本逐字节一致。tree=True（%tree）时额外建 CST：
      - 移进：为当前 Token 建叶子节点，和语义值一起压栈；
      - 归约：把弹出的 k 个子节点合成左部节点，再随归约结果压栈；
      - Parser_ParseLoopLR 增加出参 ppTree：非 NULL 表示“只建树”，不执行
        语义动作，只把 CST 根节点交给 Parser_ParseTreeText，使 %tree 在
        SLR / LALR 下的行为与 LL(1) 产物一致。
    """
    markers = _lr_tree_markers(tree)
    text = r'''
/** @brief LR 分析栈入栈。 @param pStack 分析栈。 @param state 新状态。
 *  @param symbol 文法符号编号。 @param pValue 语义值。
 *  @param owned 语义值是否归运行时所有。 @param pName 变量名。
 *  @retval 状态码。 */
static int32_t Parser_LRPush(ParserLRStack_T *pStack, uint16_t state,
                             uint16_t symbol, VarValue_T *pValue,
                             uint8_t owned, const char *pName)
{
    if (pStack->top >= PARSER_MAX_STACK_DEPTH) { return PARSER_ERR_STACK; }
    pStack->states[pStack->top] = state;
    pStack->symbols[pStack->top] = symbol;
    pStack->values[pStack->top] = pValue;
    pStack->valueOwned[pStack->top] = owned;
    if (pName != NULL) {
        snprintf(pStack->names[pStack->top],
                 sizeof(pStack->names[pStack->top]), "%s", pName);
    } else {
        pStack->names[pStack->top][0] = '\0';
    }
    pStack->top++;
    return PARSER_OK;
}

/** @brief 释放栈上仍归运行时所有的语义值（pKeep 除外）并清空栈。
 *  @param pStack 分析栈。 @param pKeep 交给调用方接管的值。 @retval 无。 */
static void Parser_LRReleaseAll(ParserLRStack_T *pStack, VarValue_T *pKeep)
{
    uint32_t index = 0u;
    for (index = 0u; index < (uint32_t)pStack->top; index++) {
        if ((pStack->valueOwned[index] != 0u) &&
            (pStack->values[index] != NULL) &&
            (pStack->values[index] != pKeep)) {
            Var_Free(pStack->values[index]);
        }
        pStack->values[index] = NULL;
        pStack->valueOwned[index] = 0u;
__LR_RELEASE_NODES__    }
    pStack->top = 0u;
}

/** @brief 移进：把当前 Token 的语义值克隆后压栈。 @retval 状态码。 */
static int32_t Parser_LRShift(ParserCtx_T *pCtx, ParserLRStack_T *pStack,
                              uint16_t state__LR_SHIFT_MODE_PARAM__)
{
    VarValue_T *pValue = NULL;
    uint8_t owned = 0u;
    const char *pName = (pCtx->current.pName != NULL) ?
                        pCtx->current.pName : pCtx->current.name;
__LR_SHIFT_LEAF_DECL__    if (pCtx->current.pValue != NULL) {
        pValue = Var_CloneValue(pCtx->current.pValue);
        if (pValue == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
        owned = 1u;
    } else if (pCtx->current.type == TOKEN_NUMBER) {
        pValue = Var_CreateInt(pCtx->current.value);
        if (pValue == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
        owned = 1u;
    }
__LR_SHIFT_LEAF_CREATE__    if (Parser_LRPush(pStack, state, (uint16_t)pCtx->current.type,
                      pValue, owned, pName) != PARSER_OK) {
        Var_Free(pValue);
__LR_SHIFT_LEAF_FREE__        return PARSER_ERR_STACK;
    }
__LR_SHIFT_LEAF_STORE__    return PARSER_OK;
}

/** @brief 归约：弹出 k 个符号，执行语义动作，再按 GOTO 压回结果。
 *  @param pCtx 上下文。 @param stack 分析栈（动作里的 $n 直接读它的槽位）。
 *  @param production 产生式编号。 @retval 状态码。 */
static int32_t Parser_LRReduce(ParserCtx_T *pCtx, ParserLRStack_T *stack,
                               uint16_t production__LR_REDUCE_MODE_PARAM__)
{
    uint32_t length = s_ParserProductionLength[production];
    uint16_t lhs = s_ParserProductionLhs[production];
    uint32_t base = 0u;
    uint32_t index = 0u;
    uint16_t state = 0u;
    VarValue_T *pResult = NULL;
    uint8_t resultOwned = 0u;
    ParseResult_T result = { 0 };
    int32_t actionStatus = PARSER_OK;
__LR_REDUCE_NODE_DECL__    (void)base;

    if ((uint32_t)stack->top < length) { return PARSER_ERR_STACK; }
    base = (uint32_t)stack->top - length;
    (void)base;
__LR_REDUCE_BEGIN____LR_ACTION_CASES__
__LR_REDUCE_END__    if (actionStatus != PARSER_OK) {
        for (index = 0u; index < length; index++) {
            uint32_t slot = base + index;
            if ((stack->valueOwned[slot] != 0u) &&
                (stack->values[slot] != NULL)) {
                Var_Free(stack->values[slot]);
            }
            stack->values[slot] = NULL;
            stack->valueOwned[slot] = 0u;
        }
        stack->top = (uint16_t)base;
        Parser_SetError(actionStatus, pCtx->current.line,
                        pCtx->current.column, "变量定义或赋值失败");
        return actionStatus;
    }
    pResult = result.value;
    resultOwned = (pResult != NULL) ? 1u : 0u;
    for (index = 0u; index < length; index++) {
        uint32_t slot = base + index;
        if ((pResult != NULL) && (stack->values[slot] == pResult)) {
            if (stack->valueOwned[slot] != 0u) {
                /* $$ = $n：所有权从栈槽位转移到归约结果，避免重复释放 */
                stack->valueOwned[slot] = 0u;
            } else {
                pResult = Var_CloneValue(pResult);
                if (pResult == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
            }
            break;
        }
    }
    for (index = 0u; index < length; index++) {
        uint32_t slot = base + index;
        if ((stack->valueOwned[slot] != 0u) &&
            (stack->values[slot] != NULL)) {
            Var_Free(stack->values[slot]);
        }
        stack->values[slot] = NULL;
        stack->valueOwned[slot] = 0u;
    }
    stack->top = (uint16_t)base;
    if (stack->top == 0u) {
        Var_Free(pResult);
__LR_REDUCE_FREE_NODE__        return PARSER_ERR_STACK;
    }
    state = s_ParserGoto[stack->states[stack->top - 1u]][lhs];
    if (state == UINT16_MAX) {
        Var_Free(pResult);
__LR_REDUCE_FREE_NODE__        Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                        pCtx->current.column, "__LR_NAME__ GOTO 表没有匹配项");
        return PARSER_ERR_SYNTAX;
    }
    if (Parser_LRPush(stack, state, lhs, pResult, resultOwned, NULL)
        != PARSER_OK) {
        Var_Free(pResult);
__LR_REDUCE_FREE_NODE__        return PARSER_ERR_STACK;
    }
__LR_REDUCE_STORE_NODE__    return PARSER_OK;
}

/** @brief __LR_LABEL__ 主循环：移进 / 归约 / 接受。 @param pCtx 上下文。 @retval 结果或 NULL。 */
static VarValue_T *Parser_ParseLoopLR(ParserCtx_T *pCtx__LR_LOOP_TREE_PARAM__)
{
    ParserLRStack_T stack;
    VarValue_T *pResult = NULL;
__LR_LOOP_TREE_DECL__    memset(&stack, 0, sizeof(stack));
__LR_LOOP_TREE_INIT__    if (Parser_LRPush(&stack, 0u, PARSER_INVALID_INDEX, NULL, 0u, NULL)
        != PARSER_OK) {
        return NULL;
    }
    for (;;) {
        uint16_t state = stack.states[stack.top - 1u];
        int32_t action = 0;
        if ((state >= __LR_STATE_COUNT__u) ||
            ((uint32_t)pCtx->current.type >= __TERMINAL_COUNT__u)) {
            Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                            pCtx->current.column, "__LR_NAME__ 状态或 Token 越界");
            Parser_LRReleaseAll(&stack, NULL);
            return NULL;
        }
        action = s_ParserAction[state][pCtx->current.type];
        if (action == INT32_MAX) {
            pResult = stack.values[stack.top - 1u];
            stack.valueOwned[stack.top - 1u] = 0u;
__LR_LOOP_ACCEPT__            Parser_LRReleaseAll(&stack, pResult);
            return pResult;
        }
        if (action > 0) {
            if (Parser_LRShift(pCtx, &stack, (uint16_t)(action - 1)__LR_LOOP_MODE_ARG__)
                != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "__LR_NAME__ 分析栈已满");
                Parser_LRReleaseAll(&stack, NULL);
                return NULL;
            }
            if (Parser_Advance(pCtx) != PARSER_OK) {
                Parser_LRReleaseAll(&stack, NULL);
                return NULL;
            }
        } else if (action < 0) {
            if (Parser_LRReduce(pCtx, &stack, (uint16_t)(-action - 1)__LR_LOOP_MODE_ARG__)
                != PARSER_OK) {
                Parser_LRReleaseAll(&stack, NULL);
                return NULL;
            }
        } else {
            Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                            pCtx->current.column, "__LR_NAME__ 分析表没有匹配项");
            Parser_LRReleaseAll(&stack, NULL);
            return NULL;
        }
    }
}

/** @brief 解析 Token 数组（__LR_NAME__）。 @param pTokens Token 数组。 @param count 数量。 @retval 结果或 NULL。 */
VarValue_T *Parser_Run(const ParserToken_T *pTokens, uint32_t count)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pTokens = pTokens;
    pContext->tokenCount = count;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoopLR(pContext__LR_LOOP_CALL__);
    return pResult;
}

/** @brief 解析脚本文本（__LR_NAME__）。 @param pText 输入文本。 @retval 结果或 NULL。 */
VarValue_T *Parser_RunText(const char *pText)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    if (pText == NULL) { return NULL; }
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pText = pText;
    pContext->textMode = 1u;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoopLR(pContext__LR_LOOP_CALL__);
    if ((pContext->ownsCurrentValue != 0u) &&
        (pContext->current.pValue != NULL)) {
        Var_Free(pContext->current.pValue);
    }
    return pResult;
}
'''
    for marker, value in markers.items():
        text = text.replace(marker, value)
    return text.replace("__LR_LABEL__", mode_label).replace(
        "__LR_NAME__", mode_name
    )


# 三种模式共用的 CST 节点工具：建节点 / 挂子节点 / 读取当前 Token 文本
_TREE_NODE_HELPERS_C = r'''/** @brief 创建语法树节点。 @param pName 节点名。 @param pText 终结符文本。 @retval 新节点。 */
static VarValue_T *Parser_TreeCreateNode(const char *pName, const char *pText)
{
    VarValue_T *pNode = NULL;
    VarValue_T *pChildren = NULL;
    if (pName == NULL) { return NULL; }
    pNode = Var_CreateStruct();
    if (pNode == NULL) { return NULL; }
    pChildren = Var_CreateArray();
    if (pChildren == NULL) { Var_Free(pNode); return NULL; }
    if (Var_StructSetField(pNode, "name",
                           Var_CreateString(pName)) != VAR_RUNTIME_OK) {
        Var_Free(pNode);
        return NULL;
    }
    if (Var_StructSetField(pNode, "text",
                           Var_CreateString((pText != NULL) ? pText : ""))
        != VAR_RUNTIME_OK) {
        Var_Free(pNode);
        return NULL;
    }
    if (Var_StructSetField(pNode, "children", pChildren) != VAR_RUNTIME_OK) {
        Var_Free(pNode);
        return NULL;
    }
    return pNode;
}

/** @brief 把子节点挂到父节点的 children 数组。 @retval 状态码。 */
static int32_t Parser_TreeAttach(VarValue_T *pParent, VarValue_T *pChild)
{
    VarValue_T *pChildren = NULL;
    if ((pParent == NULL) || (pChild == NULL)) {
        return VAR_RUNTIME_ERR_TYPE;
    }
    pChildren = Var_StructGetField(pParent, "children");
    if (pChildren == NULL) { return VAR_RUNTIME_ERR_TYPE; }
    return Var_ArrayPush(pChildren, pChild);
}

'''

# 三种模式共用的 CST 查询接口：名字 / 文本 / 子节点 / 打印 / 释放
_TREE_ACCESSOR_FUNCTIONS_C = r'''/** @brief 返回语法树节点的 children 数组。 @retval 数组指针。 */
VarValue_T *Parser_TreeGetChildren(const VarValue_T *pNode)
{
    if (pNode == NULL) { return NULL; }
    return Var_StructGetField((VarValue_T *)pNode, "children");
}

/** @brief 返回语法树节点的名字。 @retval 只读字符串。 */
const char *Parser_TreeGetName(const VarValue_T *pNode)
{
    VarValue_T *pField = NULL;
    const char *pResult = NULL;
    if (pNode == NULL) { return NULL; }
    pField = Var_StructGetField((VarValue_T *)pNode, "name");
    if (pField == NULL) { return NULL; }
    if (Var_GetString(pField, &pResult) != VAR_RUNTIME_OK) { return NULL; }
    return pResult;
}

/** @brief 返回终结符文本。 @retval 只读字符串。 */
const char *Parser_TreeGetText(const VarValue_T *pNode)
{
    VarValue_T *pField = NULL;
    const char *pResult = NULL;
    if (pNode == NULL) { return NULL; }
    pField = Var_StructGetField((VarValue_T *)pNode, "text");
    if (pField == NULL) { return NULL; }
    if (Var_GetString(pField, &pResult) != VAR_RUNTIME_OK) { return NULL; }
    return pResult;
}

/** @brief 递归打印语法树。 @param pNode 当前节点。 @param depth 缩进深度。 */
static void Parser_TreePrintRecursive(const VarValue_T *pNode, uint32_t depth)
{
    VarValue_T *pChildren = NULL;
    const char *pName = NULL;
    const char *pText = NULL;
    uint32_t index = 0u;
    uint32_t childCount = 0u;
    if (pNode == NULL) { return; }
    pName = Parser_TreeGetName(pNode);
    pText = Parser_TreeGetText(pNode);
    for (index = 0u; index < depth; index++) { printf("  "); }
    printf("%s", (pName != NULL) ? pName : "?");
    if ((pText != NULL) && (pText[0] != '\0')) {
        printf("(%s)", pText);
    }
    printf("\n");
    pChildren = Parser_TreeGetChildren(pNode);
    if (pChildren == NULL) { return; }
    childCount = (uint32_t)pChildren->u.array.length;
    for (index = 0u; index < childCount; index++) {
        Parser_TreePrintRecursive(
            Var_ArrayGet(pChildren, (size_t)index), depth + 1u);
    }
}

/** @brief 打印语法树根节点。 @param pRoot 根节点。 @retval 无。 */
void Parser_TreePrint(const VarValue_T *pRoot)
{
    Parser_TreePrintRecursive(pRoot, 0u);
}

/** @brief 释放语法树。 @param pRoot 根节点。 @retval 无。 */
void Parser_TreeFree(VarValue_T *pRoot)
{
    Var_Free(pRoot);
}

'''

# Token 名称表与文本辅助：分文件布局下进 parser_token.c，单文件产物行内展开
_TOKEN_IMPL_C = r'''/* Token 名称表：下标与 ParserTokenType_T 枚举一一对应 */
static const char *const s_ParserTokenNames[] =
{
__TOKEN_NAMES__
};

/** @brief 取 Token 枚举名。 @param type Token 类型。 @retval 只读名字或 NULL。 */
const char *Parser_TokenName(ParserTokenType_T type)
{
    uint32_t index = (uint32_t)type;
    if (index >= (uint32_t)(sizeof(s_ParserTokenNames) /
                            sizeof(s_ParserTokenNames[0]))) {
        return NULL;
    }
    return s_ParserTokenNames[index];
}

/** @brief 把 Token 的文本写进缓冲区。 @param pToken Token。
 *  @param pBuffer 输出缓冲区。 @param size 缓冲区容量。 @retval 无。 */
void Parser_TokenText(const ParserToken_T *pToken, char *pBuffer,
                      uint32_t size)
{
    const char *pName = NULL;
    if ((pToken == NULL) || (pBuffer == NULL) || (size == 0u)) { return; }
    pBuffer[0] = '\0';
    if (pToken->pName != NULL) {
        snprintf(pBuffer, size, "%s", pToken->pName);
        return;
    }
    if (pToken->pValue != NULL) {
        switch (pToken->pValue->type) {
            case VAR_TYPE_INT:
                snprintf(pBuffer, size, "%d", pToken->pValue->u.intValue);
                return;
            case VAR_TYPE_UINT32:
                snprintf(pBuffer, size, "%u",
                         (unsigned int)pToken->pValue->u.uint32Value);
                return;
            case VAR_TYPE_FLOAT:
                snprintf(pBuffer, size, "%g",
                         (double)pToken->pValue->u.floatValue);
                return;
            case VAR_TYPE_DOUBLE:
                snprintf(pBuffer, size, "%g",
                         pToken->pValue->u.doubleValue);
                return;
            case VAR_TYPE_STRING:
                snprintf(pBuffer, size, "\"%s\"",
                         pToken->pValue->u.stringValue);
                return;
            default: break;
        }
    }
    if (pToken->name != NULL) {
        snprintf(pBuffer, size, "%s", pToken->name);
        return;
    }
    pName = Parser_TokenName(pToken->type);
    if (pName != NULL) { snprintf(pBuffer, size, "%s", pName); }
}

__LEX_DFA_DECLS__'''


_TOKEN_DFA_TABLES_C = r'''__LEX_DFA_TABLES__'''


def _generator_tree_code(analysis, terminals, nonterms, tree_enabled):
    """生成通用语法树元数据与 Parser_ParseTreeText 等 API（LL(1)）。"""
    if not tree_enabled:
        return ""
    terminal_set = set(terminals)
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    term_count = len(terminals)
    prod_starts = []
    prod_lengths = []
    prod_symbols = []
    for prod in analysis["productions"]:
        prod_starts.append(len(prod_symbols))
        prod_lengths.append(len(prod["rhs"]))
        for symbol in prod["rhs"]:
            if symbol in terminal_set:
                prod_symbols.append(terminals.index(symbol))
            else:
                prod_symbols.append(term_count + nonterm_ids[symbol])
    if not prod_symbols:
        prod_symbols.append(0)
    token_names = ",\n".join(
        "    %s" % _var_c_string(token) for token in terminals
    )
    nonterm_names = ",\n".join(
        "    %s" % _var_c_string(name) for name in nonterms
    )
    starts_text = ",\n".join(
        "    %du" % value for value in prod_starts
    )
    lengths_text = ",\n".join(
        "    %du" % value for value in prod_lengths
    )
    symbols_text = ",\n".join(
        "    %du" % value for value in prod_symbols
    )
    return r'''/* ============ 通用语法树 API（由 %tree 指令启用） ============ */

__SEC_TREE_TABLES_BEGIN__/* 语法树元数据：非终结符名表 + 产生式右部符号表 */
static const char *const s_ParserTreeNontermNames[] =
{
__TREE_NONTERM_NAMES__
};
static const uint16_t s_ParserTreeProdStart[] =
{
__TREE_PROD_STARTS__
};
static const uint16_t s_ParserTreeProdLength[] =
{
__TREE_PROD_LENGTHS__
};
static const uint16_t s_ParserTreeProdSymbols[] =
{
__TREE_PROD_SYMBOLS__
};
__SEC_TREE_TABLES_END__
__TREE_NODE_HELPERS__/** @brief 打开一个语法树节点。 @retval 状态码。 */
static int32_t Parser_TreeOpen(ParserCtx_T *pCtx, const char *pName)
{
    VarValue_T *pNode = NULL;
    if (pCtx->pTreeDepth >= PARSER_MAX_STACK_DEPTH) {
        return PARSER_ERR_STACK;
    }
    pNode = Parser_TreeCreateNode(pName, NULL);
    if (pNode == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    if (pCtx->pTreeDepth > 0u) {
        if (Parser_TreeAttach(pCtx->pTreePath[pCtx->pTreeDepth - 1u],
                              pNode) != PARSER_OK) {
            Var_Free(pNode);
            return VAR_RUNTIME_ERR_MEMORY;
        }
    } else {
        pCtx->pTreeRoot = pNode;
    }
    pCtx->pTreePath[pCtx->pTreeDepth++] = pNode;
    return PARSER_OK;
}

/** @brief 关闭当前语法树节点。 @retval 状态码。 */
static int32_t Parser_TreeClose(ParserCtx_T *pCtx)
{
    if (pCtx->pTreeDepth == 0u) {
        return PARSER_ERR_SYNTAX;
    }
    pCtx->pTreeDepth--;
    return PARSER_OK;
}

/** @brief 记录一个终结符叶子节点。 @retval 状态码。 */
static int32_t Parser_TreeAddTerminal(ParserCtx_T *pCtx,
                                      const char *pName,
                                      const char *pText)
{
    VarValue_T *pLeaf = NULL;
    if (pCtx->pTreeDepth == 0u) {
        return PARSER_ERR_SYNTAX;
    }
    pLeaf = Parser_TreeCreateNode(pName, pText);
    if (pLeaf == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    if (Parser_TreeAttach(pCtx->pTreePath[pCtx->pTreeDepth - 1u],
                          pLeaf) != PARSER_OK) {
        Var_Free(pLeaf);
        return VAR_RUNTIME_ERR_MEMORY;
    }
    return PARSER_OK;
}

/** @brief 语法树解析主循环。 @param pCtx 上下文。 @retval 根节点或 NULL。 */
static VarValue_T *Parser_TreeParseLoop(ParserCtx_T *pCtx)
{
    if (Parser_Push(pCtx, PARSER_ITEM_NONTERM, __START_ID__u,
                    PARSER_INVALID_INDEX, 0u,
                    PARSER_INVALID_INDEX) != PARSER_OK) {
        return NULL;
    }
    while (pCtx->stackTop > 0u) {
        ParserStackItem_T item = pCtx->stack[--pCtx->stackTop];
        if (item.kind == PARSER_ITEM_TERMINAL) {
            char textBuffer[96];
            if ((uint16_t)pCtx->current.type != item.symbol) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "终结符不匹配");
                return NULL;
            }
            Parser_TokenText(&pCtx->current, textBuffer,
                             (uint32_t)sizeof(textBuffer));
            if (Parser_TreeAddTerminal(
                    pCtx,
                    Parser_TokenName(pCtx->current.type),
                    textBuffer) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "语法树创建失败");
                return NULL;
            }
            if (Parser_Advance(pCtx) != PARSER_OK) { return NULL; }
        } else if (item.kind == PARSER_ITEM_NONTERM) {
            int16_t production = -1;
            uint16_t rhsIndex = 0u;
            uint32_t rhsStart = 0u;
            uint32_t rhsLength = 0u;
            if (((uint16_t)pCtx->current.type >= __TERMINAL_COUNT__u) ||
                (item.symbol >= __NONTERM_COUNT__u)) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "没有可用产生式");
                return NULL;
            }
            production = s_ParserTable[item.symbol][pCtx->current.type];
            if (production < 0) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "预测分析表没有匹配项");
                return NULL;
            }
            if (Parser_TreeOpen(
                    pCtx,
                    s_ParserTreeNontermNames[item.symbol]) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "语法树节点栈已满");
                return NULL;
            }
            if (Parser_Push(pCtx, PARSER_ITEM_TREE_END, 0u,
                            PARSER_INVALID_INDEX, 0u,
                            PARSER_INVALID_INDEX) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "分析栈已满");
                return NULL;
            }
            rhsStart = s_ParserTreeProdStart[production];
            rhsLength = s_ParserTreeProdLength[production];
            for (rhsIndex = (uint16_t)rhsLength; rhsIndex > 0u; rhsIndex--) {
                uint16_t symbol = s_ParserTreeProdSymbols[rhsStart +
                                                          rhsIndex - 1u];
                if (symbol < __TERMINAL_COUNT__u) {
                    if (Parser_Push(pCtx, PARSER_ITEM_TERMINAL, symbol,
                                    PARSER_INVALID_INDEX, 0u,
                                    PARSER_INVALID_INDEX) != PARSER_OK) {
                        Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                        pCtx->current.column, "分析栈已满");
                        return NULL;
                    }
                } else {
                    if (Parser_Push(pCtx, PARSER_ITEM_NONTERM,
                                    (uint16_t)(symbol - __TERMINAL_COUNT__u),
                                    PARSER_INVALID_INDEX, 0u,
                                    PARSER_INVALID_INDEX) != PARSER_OK) {
                        Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                        pCtx->current.column, "分析栈已满");
                        return NULL;
                    }
                }
            }
        } else if (item.kind == PARSER_ITEM_TREE_END) {
            if (Parser_TreeClose(pCtx) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "语法树闭合失败");
                return NULL;
            }
        } else {
            Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                            pCtx->current.column, "语法树分析出现未知项目");
            return NULL;
        }
    }
    if ((pCtx->pTreeRoot == NULL) || (pCtx->pTreeDepth != 0u) ||
        (pCtx->current.type != TOKEN_END)) {
        Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                        pCtx->current.column, "输入尚未完全消费");
        return NULL;
    }
    return pCtx->pTreeRoot;
}

/** @brief 直接解析脚本文本并返回语法树根节点。 @param pText 输入。 @retval 根节点或 NULL。 */
VarValue_T *Parser_ParseTreeText(const char *pText)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pTree = NULL;
    if (pText == NULL) { return NULL; }
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pText = pText;
    pContext->textMode = 1u;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pTree = Parser_TreeParseLoop(pContext);
    if ((pContext->ownsCurrentValue != 0u) &&
        (pContext->current.pValue != NULL)) {
        Var_Free(pContext->current.pValue);
    }
    if (pTree == NULL) {
        Var_Free(pContext->pTreeRoot);
        pContext->pTreeRoot = NULL;
        pContext->pTreeDepth = 0u;
        pContext->stackTop = 0u;
    }
    return pTree;
}

__TREE_ACCESSORS__/* ============ 通用语法树 API 结束 ============ */
''' .replace("__TREE_NODE_HELPERS__", _TREE_NODE_HELPERS_C) \
      .replace("__TREE_ACCESSORS__", _TREE_ACCESSOR_FUNCTIONS_C) \
      .replace("__TREE_TOKEN_NAMES__", token_names) \
      .replace("__TREE_NONTERM_NAMES__", nonterm_names) \
      .replace("__TREE_PROD_STARTS__", starts_text) \
      .replace("__TREE_PROD_LENGTHS__", lengths_text) \
      .replace("__TREE_PROD_SYMBOLS__", symbols_text)


# SLR(1) / LALR(1) 的 CST 名字表：移进给叶子取名，归约给左部节点取名
# Token 名表由 parser_token 模块提供，这里只需要非终结符名表
_LR_TREE_HEADER_C = """/* ============ 通用语法树 API（由 %tree 指令启用） ============ */

__SEC_TREE_TABLES_BEGIN__/* 语法树元数据：非终结符名表 */
static const char *const s_ParserTreeNontermNames[] =
{
__TREE_NONTERM_NAMES__
};
__SEC_TREE_TABLES_END__
"""

# LR 建树钩子：节点随栈槽位维护，因此不需要产生式右部符号表
_LR_TREE_BODY_C = r'''/** @brief 移进：为当前 Token 建叶子节点。 @param pCtx 上下文。
 *  @param ppLeaf 输出叶子节点。 @retval 状态码。 */
static int32_t Parser_TreeMakeLeaf(ParserCtx_T *pCtx, VarValue_T **ppLeaf)
{
    char textBuffer[96];
    VarValue_T *pLeaf = NULL;
    if ((pCtx == NULL) || (ppLeaf == NULL)) { return VAR_RUNTIME_ERR_TYPE; }
    Parser_TokenText(&pCtx->current, textBuffer,
                     (uint32_t)sizeof(textBuffer));
    pLeaf = Parser_TreeCreateNode(Parser_TokenName(pCtx->current.type),
                                  textBuffer);
    if (pLeaf == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    *ppLeaf = pLeaf;
    return PARSER_OK;
}

/** @brief 归约：把弹出的 k 个子节点合成左部节点。 @param pStack LR 分析栈。
 *  @param lhs 左部非终结符编号。 @param base 首个右部符号的槽位。
 *  @param length 右部长度。 @param ppNode 输出父节点。 @retval 状态码。 */
static int32_t Parser_TreeMakeBranch(ParserLRStack_T *pStack, uint16_t lhs,
                                     uint32_t base, uint32_t length,
                                     VarValue_T **ppNode)
{
    VarValue_T *pNode = NULL;
    uint32_t index = 0u;
    if (ppNode == NULL) { return VAR_RUNTIME_ERR_TYPE; }
    pNode = Parser_TreeCreateNode(s_ParserTreeNontermNames[lhs], NULL);
    if (pNode == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    for (index = 0u; index < length; index++) {
        VarValue_T *pChild = pStack->nodes[base + index];
        if (pChild == NULL) { continue; }
        if (Parser_TreeAttach(pNode, pChild) != PARSER_OK) {
            Var_Free(pNode);
            return VAR_RUNTIME_ERR_MEMORY;
        }
        pStack->nodes[base + index] = NULL;
    }
    *ppNode = pNode;
    return PARSER_OK;
}

'''

# LR 的树入口放在 LR 运行时之后：它要用 Parser_ParseLoopLR 取 CST 根节点
_LR_TREE_API_C = r'''/** @brief 直接解析脚本文本并返回语法树根节点（LR 模式）。
 *  @param pText 输入。 @retval 根节点或 NULL。 */
VarValue_T *Parser_ParseTreeText(const char *pText)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pTree = NULL;
    VarValue_T *pValue = NULL;
    if (pText == NULL) { return NULL; }
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pText = pText;
    pContext->textMode = 1u;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pValue = Parser_ParseLoopLR(pContext, &pTree);
    if (pValue != NULL) { Var_Free(pValue); }
    if ((pContext->ownsCurrentValue != 0u) &&
        (pContext->current.pValue != NULL)) {
        Var_Free(pContext->current.pValue);
        pContext->current.pValue = NULL;
        pContext->ownsCurrentValue = 0u;
    }
    if (pTree == NULL) { return NULL; }
    return pTree;
}

'''


def _generator_lr_tree_code(analysis, terminals, nonterms, tree_enabled):
    """生成 SLR(1) / LALR(1) 的 CST 名字表与建树钩子（%tree）。

    在 LR 运行时之前发射，供 Parser_LRShift / Parser_LRReduce 调用。
    """
    if not tree_enabled:
        return ""
    token_names = ",\n".join(
        "    %s" % _var_c_string(token) for token in terminals
    )
    nonterm_names = ",\n".join(
        "    %s" % _var_c_string(name) for name in nonterms
    )
    return (_LR_TREE_HEADER_C
            .replace("__TREE_TOKEN_NAMES__", token_names)
            .replace("__TREE_NONTERM_NAMES__", nonterm_names)
            + _TREE_NODE_HELPERS_C
            + _LR_TREE_BODY_C)


def _generator_lr_tree_api_c(tree_enabled):
    """生成 SLR(1) / LALR(1) 的 Parser_ParseTreeText 与查询接口（%tree）。

    在 LR 运行时之后发射：树入口依赖 Parser_ParseLoopLR。
    """
    if not tree_enabled:
        return ""
    return _LR_TREE_API_C + _TREE_ACCESSOR_FUNCTIONS_C


def _generator_ast_markers_c(ast_skip, ast_fold, ast_label):
    """生成 %ast_skip / %ast_fold / %ast_label 的节点查询元数据。

    只提供 CST→AST 转换器需要的查询接口，不实现转换本身：
      Parser_AstShouldSkip：节点消失，子节点提升；
      Parser_AstShouldFold：节点消失，子树左结合折叠；
      Parser_AstShouldLabel：节点消失，首个终结符子节点转为标签。
    """
    if not (ast_skip or ast_fold or ast_label):
        return ""

    def array_text(symbol, names):
        if not names:
            return "static const char *const %s[] = { NULL };" % symbol
        body = ", ".join(_var_c_string(name) for name in sorted(names))
        return ("static const char *const %s[] = { %s, NULL };"
                % (symbol, body))

    return r'''/* ============ AST 标记元数据（由 %ast_* 指令生成） ============ */
__AST_SKIP_ARRAY__
__AST_FOLD_ARRAY__
__AST_LABEL_ARRAY__

/** @brief 判断节点名是否落在名称表内。 @param pName 节点名。
 *  @param ppNames 名称表；@param count 有效名字个数。 @retval 1 命中。 */
static uint8_t Parser_AstNameIn(const char *pName,
                                const char *const *ppNames,
                                uint32_t count)
{
    uint32_t index = 0u;
    if (pName == NULL) { return 0u; }
    for (index = 0u; index < count; index++) {
        if (strcmp(pName, ppNames[index]) == 0) { return 1u; }
    }
    return 0u;
}

/** @brief 该 CST 节点是否应在 AST 中消失并提升子节点。 @param pName 节点名。
 *  @retval 1 需要跳过，0 保留。 */
uint8_t Parser_AstShouldSkip(const char *pName)
{
    return Parser_AstNameIn(pName, s_ParserAstSkipNames,
        (uint32_t)(sizeof(s_ParserAstSkipNames) /
                   sizeof(s_ParserAstSkipNames[0])) - 1u);
}

/** @brief 该 CST 节点是否应折叠成左结合二叉树。 @param pName 节点名。
 *  @retval 1 需要折叠，0 保留。 */
uint8_t Parser_AstShouldFold(const char *pName)
{
    return Parser_AstNameIn(pName, s_ParserAstFoldNames,
        (uint32_t)(sizeof(s_ParserAstFoldNames) /
                   sizeof(s_ParserAstFoldNames[0])) - 1u);
}

/** @brief 该 CST 节点是否应取首个终结符子节点作标签。 @param pName 节点名。
 *  @retval 1 需要打标签，0 保留。 */
uint8_t Parser_AstShouldLabel(const char *pName)
{
    return Parser_AstNameIn(pName, s_ParserAstLabelNames,
        (uint32_t)(sizeof(s_ParserAstLabelNames) /
                   sizeof(s_ParserAstLabelNames[0])) - 1u);
}
/* ============ AST 标记元数据结束 ============ */
''' .replace("__AST_SKIP_ARRAY__",
            array_text("s_ParserAstSkipNames", ast_skip)) \
      .replace("__AST_FOLD_ARRAY__",
               array_text("s_ParserAstFoldNames", ast_fold)) \
      .replace("__AST_LABEL_ARRAY__",
               array_text("s_ParserAstLabelNames", ast_label))


def _generator_demo_main_c(standalone=False, tree=False):
    """生成带 main() 的演示程序，编译后可直接解析表达式并打印结果。

    standalone=True（分文件布局的 main.c）时去掉 PARSER_SKIP_DEMO_MAIN 包裹：
    演示代码独占一个文件，不需要再用宏跳过。
    tree=True（%tree 文法）时，如果文法不写语义动作、Parser_RunText 没有可
    打印的求值结果，就改打印语法树，保证演示程序照样有输出。
    """
    text = r'''
#ifndef PARSER_SKIP_DEMO_MAIN

#ifndef PARSER_DEMO_INPUT
#define PARSER_DEMO_INPUT "1+2*3"
#endif

/** @brief 递归打印一个动态变量值。 @param pValue 变量值。 @retval 无。 */
static void Parser_DemoPrintValue(const VarValue_T *pValue)
{
    if (pValue == NULL) { printf("NULL"); return; }
    switch (pValue->type) {
        case VAR_TYPE_UINT8:  printf("%u", (unsigned int)pValue->u.uint8Value); break;
        case VAR_TYPE_INT8:   printf("%d", (int)pValue->u.int8Value); break;
        case VAR_TYPE_UINT16: printf("%u", (unsigned int)pValue->u.uint16Value); break;
        case VAR_TYPE_INT16:  printf("%d", (int)pValue->u.int16Value); break;
        case VAR_TYPE_UINT32: printf("%u", (unsigned int)pValue->u.uint32Value); break;
        case VAR_TYPE_UINT64: printf("%llu", (unsigned long long)pValue->u.uint64Value); break;
        case VAR_TYPE_INT64:  printf("%lld", (long long)pValue->u.int64Value); break;
        case VAR_TYPE_INT:    printf("%d", (int)pValue->u.intValue); break;
        case VAR_TYPE_FLOAT:  printf("%g", (double)pValue->u.floatValue); break;
        case VAR_TYPE_DOUBLE: printf("%g", pValue->u.doubleValue); break;
        case VAR_TYPE_CHAR:   printf("'%c'", pValue->u.charValue); break;
        case VAR_TYPE_STRING: printf("\"%s\"", pValue->u.stringValue); break;
        case VAR_TYPE_ARRAY: {
            size_t index = 0u;
            printf("[");
            for (index = 0u; index < pValue->u.array.length; index++) {
                if (index > 0u) { printf(", "); }
                Parser_DemoPrintValue(pValue->u.array.items[index]);
            }
            printf("]");
            break;
        }
        case VAR_TYPE_DICT: {
            size_t index = 0u;
            printf("{");
            for (index = 0u; index < pValue->u.dict.length; index++) {
                if (index > 0u) { printf(", "); }
                printf("\"%s\": ", pValue->u.dict.entries[index].key);
                Parser_DemoPrintValue(pValue->u.dict.entries[index].value);
            }
            printf("}");
            break;
        }
        case VAR_TYPE_STRUCT: {
            size_t index = 0u;
            printf("{");
            for (index = 0u; index < pValue->u.object.length; index++) {
                if (index > 0u) { printf(", "); }
                printf("\"%s\": ", pValue->u.object.members[index].name);
                Parser_DemoPrintValue(pValue->u.object.members[index].value);
            }
            printf("}");
            break;
        }
        default: printf("%s", Var_TypeName(pValue->type)); break;
    }
}

/** @brief 演示程序入口：解析 argv[1] 或默认表达式并打印结果。 @retval 0 表示成功。 */
int main(int argc, char **argv)
{
    const char *pInput = NULL;
    VarValue_T *pResult = NULL;
__DEMO_TREE_DECL__    const ParserError_T *pError = NULL;

    if (argc > 1) {
        pInput = argv[1];
    } else {
        pInput = PARSER_DEMO_INPUT;
    }
    printf("解析输入: %s\n", pInput);
    pResult = Parser_RunText(pInput);
    if (pResult == NULL) {
__DEMO_TREE_FALLBACK__        pError = Parser_GetLastError();
        if (pError->code == PARSER_OK) {
            printf("解析成功，但没有可返回的值"
                   "（文法未写语义动作，或运算结果为空，例如除以 0）\n");
            return 0;
        }
        printf("解析失败: code=%d line=%u column=%u message=%s\n",
               pError->code, pError->line, pError->column, pError->message);
        return 1;
    }
    printf("解析结果[%s]: ", Var_TypeName(pResult->type));
    Parser_DemoPrintValue(pResult);
    printf("\n");
    Var_Free(pResult);
    return 0;
}

#endif /* PARSER_SKIP_DEMO_MAIN */
'''
    if tree:
        text = text.replace(
            "__DEMO_TREE_DECL__", "    VarValue_T *pTree = NULL;\n"
        ).replace(
            "__DEMO_TREE_FALLBACK__",
            "        if (Parser_GetLastError()->code == PARSER_OK) {\n"
            "            /* %tree 文法可以不写语义动作：没有结果时打印语法树 */\n"
            "            pTree = Parser_ParseTreeText(pInput);\n"
            "            if (pTree != NULL) {\n"
            "                printf(\"语法树:\\n\");\n"
            "                Parser_TreePrint(pTree);\n"
            "                Parser_TreeFree(pTree);\n"
            "                return 0;\n"
            "            }\n"
            "        }\n"
        )
    else:
        text = text.replace("__DEMO_TREE_DECL__", "").replace(
            "__DEMO_TREE_FALLBACK__", ""
        )
    if not standalone:
        return text
    return text.replace("#ifndef PARSER_SKIP_DEMO_MAIN\n\n", "").replace(
        "\n#endif /* PARSER_SKIP_DEMO_MAIN */\n", "\n"
    )


def _generator_token_names_c(terminals):
    """Token 名称表内容：与 ParserTokenType_T 枚举同序，末尾补 INVALID。"""
    names = list(terminals) + ["INVALID"]
    return ",\n".join("    %s" % _var_c_string(name) for name in names)


# 分文件布局的分段标记名；单文件布局里这些标记全部替换成空串
_SECTION_NAMES = (
    "RUNTIME_H", "RUNTIME_C", "DEFINES", "TOKEN_TYPES", "TOKEN_IMPL",
    "ERROR_TYPE", "RESULT_TYPE", "TABLES", "TREE_TABLES", "DEMO",
)


def _section_marker(name, edge):
    """拼出一个分段标记，例如 __SEC_TABLES_BEGIN__。"""
    return "__SEC_%s_%s__" % (name, edge)


def _apply_markers(text, replacements):
    """按标记替换表展开拼装结果（每个产物文件各展开一次）。"""
    for marker, value in replacements.items():
        text = text.replace(marker, value)
    return text


def _replace_section(text, name, replacement):
    """把 __SEC_<name>_BEGIN__…__SEC_<name>_END__ 整段换成 replacement。"""
    begin = _section_marker(name, "BEGIN")
    end = _section_marker(name, "END")
    if begin not in text:
        return text
    start = text.index(begin)
    stop = text.index(end, start) + len(end)
    return text[:start] + replacement + text[stop:]


def _extract_section(text, name):
    """取 __SEC_<name>_BEGIN__…__SEC_<name>_END__ 之间的内容（不含标记）。"""
    begin = _section_marker(name, "BEGIN")
    end = _section_marker(name, "END")
    start = text.index(begin) + len(begin)
    return text[start:text.index(end, start)]


def _parser_analysis(grammar, tokens, mode, priority=None):
    """按生成模式构造分析结果：LL(1) 预测表 / SLR(1) / LALR(1) 表。"""
    if mode == "lalr":
        return _lalr_analyze(grammar, tokens, priority)
    if mode == "slr":
        return _slr_analyze(grammar, tokens, priority)
    return prepare_grammar(grammar, tokens)


# 分配器配置：默认走 C 运行库，用户可在 parser_config.h 里换成自己的实现
_ALLOCATOR_MACROS_C = r'''#ifndef PARSER_MALLOC
#define PARSER_MALLOC(size) malloc(size)
#endif
#ifndef PARSER_CALLOC
#define PARSER_CALLOC(count, size) calloc((count), (size))
#endif
#ifndef PARSER_REALLOC
#define PARSER_REALLOC(pointer, size) realloc((pointer), (size))
#endif
#ifndef PARSER_FREE
#define PARSER_FREE(pointer) free((pointer))
#endif
'''


def _with_allocator_macros(text):
    """把运行时里的 malloc/calloc/realloc/free 全部换成 PARSER_* 配置宏。

    只做函数调用形式的替换，Var_Free / Var_FreeContent 这类名字不受影响。
    """
    return (text.replace("malloc(", "PARSER_MALLOC(")
                .replace("calloc(", "PARSER_CALLOC(")
                .replace("realloc(", "PARSER_REALLOC(")
                .replace("free(", "PARSER_FREE("))


def _with_allocator_include(text):
    """在 variable_runtime.h 里补上 #include "parser_config.h"。"""
    marker = "#include <stddef.h>\n"
    if marker in text:
        return text.replace(
            marker, marker + '\n#include "parser_config.h"\n', 1
        )
    return text


def _assemble_parser_code(grammar, tokens, variables=None, token_defs=None,
                          tree=False, var_max_count=32,
                          ast_skip=None, ast_fold=None, ast_label=None,
                          mode="ll1", priority=None, lex_spec=None):
    """拼装产物源码，返回 (带 __ 标记的源码, 标记替换表)。

    单文件与分文件布局共用这一份拼装逻辑：分文件布局按 __SEC_*__ 分段标记
    切片，单文件布局把所有标记替换成空串后直接输出。
    """
    variables = variables or {}
    token_defs = token_defs or {}
    lex_spec = _normalize_lex_spec(lex_spec)
    mode = str(mode or "ll1").lower()
    if mode not in ("ll1", "slr", "lalr"):
        raise GrammarLoadError(
            "未知的生成模式：%s（只支持 ll1、slr 或 lalr）" % mode
        )
    ast_skip = set(ast_skip or ())
    ast_fold = set(ast_fold or ())
    ast_label = set(ast_label or ())
    var_max_count = max(1, int(var_max_count if var_max_count else 32))
    analysis = _parser_analysis(grammar, tokens, mode, priority)
    terminals = analysis["terminals"]
    nonterms = analysis["nonterms"]
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    max_slots = max(
        [len(prod["rhs"]) + 3 for prod in analysis["productions"]] + [4]
    )
    token_enum = ", ".join("TOKEN_" + token for token in terminals)
    token_enum += ", TOKEN_INVALID"
    # SLR / LALR 共用 LR 栈与 ACTION/GOTO 表生成器，只有模式标签不同
    lr_label = "LALR(1)" if mode == "lalr" else "SLR(1)"
    lr_name = "LALR" if mode == "lalr" else "SLR"
    if mode in ("slr", "lalr"):
        mode_item_types = (
            _generator_lr_stack_c(lr_label, mode, tree).lstrip("\n") + "\n"
        )
        ctx_mode_fields = ""
        mode_tables = _generator_lr_tables_c(analysis) + "\n"
    else:
        mode_item_types = _generator_ll_item_types_c()
        ctx_mode_fields = _generator_ll_ctx_fields_c()
        mode_tables = _generator_ll_tables_c(analysis, terminals, nonterms)
    replacements = {
        "__MODE_NAME__": ("LL(1)" if mode == "ll1" else lr_label),
        "__TOKEN_ENUM__": token_enum,
        "__MAX_SLOTS__": str(max_slots),
        "__NONTERM_COUNT__": str(len(nonterms)),
        "__TERMINAL_COUNT__": str(len(terminals)),
        "__LR_STATE_COUNT__": str(len(analysis.get("states", []))),
        "__MODE_ITEM_TYPES__": mode_item_types,
        "__CTX_MODE_FIELDS__": ctx_mode_fields,
        "__MODE_TABLES__": mode_tables,
        "__LEX_RULES__": _generator_lex_rules(token_defs),
        "__REGISTRY__": _generator_registry_block(variables, var_max_count),
        "__EXPAND_CASES__": _generator_expand_cases(
            analysis, terminals, nonterm_ids
        ),
        "__ACTION_CASES__": _generator_action_cases(analysis),
        "__LR_ACTION_CASES__": _generator_lr_action_cases(analysis),
        "__START_ID__": str(nonterm_ids[analysis["grammar"]["start"]]),
        "__RUNTIME_HEADER__": VARIABLE_RUNTIME_HEADER,
        "__RUNTIME_SOURCE__": _with_allocator_macros(
            VARIABLE_RUNTIME_SOURCE_EMBEDDED
        ),
        "__RUNTIME_ALLOCATOR_CONFIG__": _ALLOCATOR_MACROS_C,
    }
    # %regex / %identifier：DFA 表与标识符识别接口
    lex_decls, lex_tables = _generator_lex_dfa_c(
        token_defs, lex_spec["regex"], lex_spec["identifier"]
    )
    # 分文件布局用的 token 模块与分段标记；单文件布局里标记一律替换成空串
    replacements["__TOKEN_IMPL__"] = _TOKEN_IMPL_C + _TOKEN_DFA_TABLES_C
    replacements["__TOKEN_NAMES__"] = _generator_token_names_c(terminals)
    # 注意顺序：DFA 占位符在 __TOKEN_IMPL__ 展开之后才能被替换到
    replacements["__LEX_DFA_DECLS__"] = lex_decls
    replacements["__LEX_DFA_TABLES__"] = lex_tables
    for section_name in _SECTION_NAMES:
        replacements[_section_marker(section_name, "BEGIN")] = ""
        replacements[_section_marker(section_name, "END")] = ""
    code = r'''/* 由 Chapter-script-engine/parser.py 自动生成：内嵌 variable_runtime 且含演示 main 的非递归 __MODE_NAME__ 解析器 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__SEC_RUNTIME_H_BEGIN__/* ============ 嵌入式 variable_runtime.h 开始 ============ */
__RUNTIME_HEADER__
/* ============ 嵌入式 variable_runtime.h 结束 ============ */
__SEC_RUNTIME_H_END__

__SEC_RUNTIME_C_BEGIN__/* ============ 嵌入式 variable_runtime.c 开始 ============ */
/* ============ 分配器配置（分文件布局里对应 parser_config.h） ============ */
__RUNTIME_ALLOCATOR_CONFIG__
__RUNTIME_SOURCE__
/* ============ 嵌入式 variable_runtime.c 结束 ============ */
__SEC_RUNTIME_C_END__

__SEC_DEFINES_BEGIN__#ifndef PARSER_MAX_STACK_DEPTH
#define PARSER_MAX_STACK_DEPTH 64u
#endif
#ifndef PARSER_MAX_NAME_LENGTH
#define PARSER_MAX_NAME_LENGTH 64u
#endif
#ifndef PARSER_MAX_STRING_LENGTH
#define PARSER_MAX_STRING_LENGTH 128u
#endif

#define PARSER_OK 0
#define PARSER_ERR_STACK -20
#define PARSER_ERR_SYNTAX -21
#define PARSER_ERR_LEXER -22
#define PARSER_ERR_DUPLICATE -23
#define PARSER_ERR_UNDEFINED -24
#define PARSER_ERR_SYSTEM -25
#define PARSER_INVALID_INDEX UINT16_MAX
#define PARSER_VALUE_SLOTS __MAX_SLOTS__u
#if defined(__GNUC__)
#define PARSER_UNUSED __attribute__((unused))
#else
#define PARSER_UNUSED
#endif
__SEC_DEFINES_END__

__SEC_TOKEN_TYPES_BEGIN__typedef enum __ParserTokenType_T { __TOKEN_ENUM__ } ParserTokenType_T;

typedef struct __ParserToken_T
{
    ParserTokenType_T type;
    int32_t value;
    const char *name;
    VarValue_T *pValue;
    const char *pName;
    uint32_t line;
    uint32_t column;
} ParserToken_T;
__SEC_TOKEN_TYPES_END__

__SEC_TOKEN_IMPL_BEGIN____TOKEN_IMPL____SEC_TOKEN_IMPL_END__
__SEC_ERROR_TYPE_BEGIN__typedef struct __ParserError_T
{
    int32_t code;
    uint32_t line;
    uint32_t column;
    char message[96];
} ParserError_T;
__SEC_ERROR_TYPE_END__

__SEC_RESULT_TYPE_BEGIN__typedef struct __ParseResult_T
{
    VarValue_T *value;
} ParseResult_T;
__SEC_RESULT_TYPE_END__

__MODE_ITEM_TYPES__typedef struct __ParserCtx_T
{
    const ParserToken_T *pTokens;
    uint32_t tokenCount;
    uint32_t tokenPosition;
    const char *pText;
    uint32_t textPosition;
    uint32_t line;
    uint32_t column;
    uint8_t textMode;
    uint8_t ownsCurrentValue;
    char lexeme[PARSER_MAX_STRING_LENGTH];
    ParserToken_T current;
__CTX_MODE_FIELDS__    VarValue_T *pRootResult;
    VarValue_T *pTreeRoot;
    VarValue_T *pTreePath[PARSER_MAX_STACK_DEPTH];
    uint16_t pTreeDepth;
} ParserCtx_T;

typedef struct __ParserLexRule_T
{
    const char *pText;
    uint32_t length;
    ParserTokenType_T type;
} ParserLexRule_T;

static ParserError_T s_ParserLastError;
static ParserCtx_T s_ParserContext;
__SEC_TABLES_BEGIN____MODE_TABLES____SEC_TABLES_END__static const ParserLexRule_T s_ParserLexRules[] =
{
__LEX_RULES__
};

__REGISTRY__

/** @brief 设置最近错误。 @param code 错误码。 @param line 行。 @param column 列。 @param pMessage 说明。 @retval 无。 */
static void Parser_SetError(int32_t code, uint32_t line, uint32_t column,
                            const char *pMessage)
{
    s_ParserLastError.code = code;
    s_ParserLastError.line = line;
    s_ParserLastError.column = column;
    snprintf(s_ParserLastError.message, sizeof(s_ParserLastError.message),
             "%s", (pMessage != NULL) ? pMessage : "解析错误");
}

/** @brief 获取最近错误。 @retval 只读错误结构。 */
const ParserError_T *Parser_GetLastError(void)
{
    return &s_ParserLastError;
}

/** @brief 创建简单类型模板。 @param type 类型。 @retval 新模板或 NULL。 */
static VarValue_T *Parser_CreateScalarTemplate(VarType_T type)
{
    switch (type) {
        case VAR_TYPE_INT: return Var_CreateInt(0);
        case VAR_TYPE_UINT8: return Var_CreateUint8(0u);
        case VAR_TYPE_INT8: return Var_CreateInt8(0);
        case VAR_TYPE_UINT16: return Var_CreateUint16(0u);
        case VAR_TYPE_INT16: return Var_CreateInt16(0);
        case VAR_TYPE_UINT32: return Var_CreateUint32(0u);
        case VAR_TYPE_UINT64: return Var_CreateUint64(0u);
        case VAR_TYPE_INT64: return Var_CreateInt64(0);
        case VAR_TYPE_FLOAT: return Var_CreateFloat(0.0f);
        case VAR_TYPE_DOUBLE: return Var_CreateDouble(0.0);
        case VAR_TYPE_CHAR: return Var_CreateChar('\0');
        case VAR_TYPE_STRING: return Var_CreateString("");
        default: return NULL;
    }
}

/** @brief 创建数组类型模板。 @param type 元素类型。 @param pDimensions 维度。 @param count 维数。 @param level 当前层。 @retval 新模板或 NULL。 */
static VarValue_T *Parser_CreateArrayTemplate(VarType_T type,
                                              const uint32_t *pDimensions,
                                              uint32_t count,
                                              uint32_t level)
{
    VarValue_T *pArray = NULL;
    uint32_t index = 0u;
    if (level >= count) { return Parser_CreateScalarTemplate(type); }
    pArray = Var_CreateArray();
    if (pArray == NULL) { return NULL; }
    for (index = 0u; index < pDimensions[level]; index++) {
        VarValue_T *pItem = Parser_CreateArrayTemplate(
            type, pDimensions, count, level + 1u
        );
        if ((pItem == NULL) ||
            (Var_ArrayPush(pArray, pItem) != VAR_RUNTIME_OK)) {
            Var_Free(pItem);
            Var_Free(pArray);
            return NULL;
        }
    }
    return pArray;
}

/** @brief 按显式类型定义变量。 @param pName 名称。 @param pValue 初值。 @param type 类型。 @param pDimensions 维度。 @param count 维数。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_DefineTyped(
    const char *pName, const VarValue_T *pValue, VarType_T type,
    const uint32_t *pDimensions, uint32_t count)
{
    VarValue_T *pTemplate = NULL;
    int32_t ret = VAR_RUNTIME_OK;
    if (count == 0u) {
        pTemplate = Parser_CreateScalarTemplate(type);
    } else if (pDimensions != NULL) {
        pTemplate = Parser_CreateArrayTemplate(type, pDimensions, count, 0u);
    }
    if (pTemplate == NULL) { return VAR_RUNTIME_ERR_TYPE; }
    ret = Parser_DefineVariable(pName, pValue, pTemplate);
    Var_Free(pTemplate);
    return ret;
}

/** @brief 读取字符并维护行列号。 @param pCtx 上下文。 @retval 当前字符。 */
static char Parser_ReadChar(ParserCtx_T *pCtx)
{
    char ch = pCtx->pText[pCtx->textPosition++];
    if (ch == '\n') { pCtx->line++; pCtx->column = 1u; }
    else { pCtx->column++; }
    return ch;
}
'''
    code += _generator_lexer_c(
        bool(lex_spec["regex"]), lex_spec["identifier"] is not None
    )
    if mode in ("slr", "lalr"):
        # SLR / LALR：%tree 时先发名字表与建树钩子，再发 LR 运行时，
        # 最后发依赖 Parser_ParseLoopLR 的 Parser_ParseTreeText
        code += _generator_lr_tree_code(analysis, terminals, nonterms, tree)
        code += _generator_lr_parse_loop_c(lr_label, lr_name, tree)
    else:
        code += r'''
/** @brief 压入分析项目。 @retval 状态码。 */
static int32_t Parser_Push(ParserCtx_T *pCtx, ParserItemKind_T kind,
                           uint16_t symbol, uint16_t frameIndex,
                           uint16_t valueIndex, uint16_t seedFrameIndex)
{
    ParserStackItem_T *pItem = NULL;
    if (pCtx->stackTop >= PARSER_MAX_STACK_DEPTH) { return PARSER_ERR_STACK; }
    pItem = &pCtx->stack[pCtx->stackTop++];
    pItem->kind = kind;
    pItem->symbol = symbol;
    pItem->frameIndex = frameIndex;
    pItem->valueIndex = valueIndex;
    pItem->seedFrameIndex = seedFrameIndex;
    return PARSER_OK;
}

/** @brief 展开产生式。 @retval 状态码。 */
static int32_t Parser_ExpandProduction(ParserCtx_T *pCtx,
                                       uint16_t production,
                                       uint16_t frameIndex)
{
    switch (production) {
__EXPAND_CASES__
        default: return PARSER_ERR_SYNTAX;
    }
    return PARSER_OK;
}

/** @brief 执行语义动作。 @retval 状态码。 */
static int32_t Parser_ExecuteAction(ParserCtx_T *pCtx,
                                    uint16_t production,
                                    uint16_t frameIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    VarValue_T **values = pFrame->values;
    const char *names[PARSER_VALUE_SLOTS] = { 0 };
    ParseResult_T result = { 0 };
    int32_t actionStatus = PARSER_OK;
    uint32_t index = 0u;
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        names[index] = pFrame->names[index];
    }
    (void)values;
    (void)names;
    result.value = pFrame->pResult;
    switch (production) {
__ACTION_CASES__
        default: return PARSER_ERR_SYNTAX;
    }
    if (actionStatus != PARSER_OK) {
        if ((result.value != NULL) && (result.value != pFrame->pResult)) {
            Var_Free(result.value);
        }
        Parser_SetError(actionStatus, pCtx->current.line,
                        pCtx->current.column, "变量定义或赋值失败");
        return actionStatus;
    }
    if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL) &&
        (pFrame->pResult != result.value)) {
        Var_Free(pFrame->pResult);
    }
    pFrame->resultOwned = (result.value != NULL) ? 1u : 0u;
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        if (pFrame->values[index] == result.value) {
            if (pFrame->valueOwned[index] != 0u) {
                pFrame->valueOwned[index] = 0u;
            } else if (result.value != NULL) {
                result.value = Var_CloneValue(result.value);
                if (result.value == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
            }
            break;
        }
    }
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        if ((pFrame->valueOwned[index] != 0u) &&
            (pFrame->values[index] != NULL)) {
            Var_Free(pFrame->values[index]);
        }
        pFrame->values[index] = NULL;
        pFrame->valueOwned[index] = 0u;
    }
    pFrame->pResult = result.value;
    return PARSER_OK;
}

/** @brief 转发语义槽并处理所有权。 @retval 状态码。 */
static int32_t Parser_ForwardFrame(ParserSemanticFrame_T *pFrame,
                                   uint16_t valueIndex)
{
    VarValue_T *pValue = pFrame->values[valueIndex];
    uint8_t owned = pFrame->valueOwned[valueIndex];
    if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL) &&
        (pFrame->pResult != pValue)) {
        Var_Free(pFrame->pResult);
    }
    pFrame->pResult = pValue;
    pFrame->resultOwned = owned;
    pFrame->valueOwned[valueIndex] = 0u;
    return PARSER_OK;
}

/** @brief 完成语义帧。 @retval 状态码。 */
static int32_t Parser_CompleteFrame(ParserCtx_T *pCtx, uint16_t frameIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    if ((pCtx->frameTop == 0u) ||
        (frameIndex != (uint16_t)(pCtx->frameTop - 1u))) {
        return PARSER_ERR_STACK;
    }
    if (pFrame->parentFrameIndex == PARSER_INVALID_INDEX) {
        pCtx->pRootResult = pFrame->pResult;
        pFrame->resultOwned = 0u;
    } else {
        pCtx->frames[pFrame->parentFrameIndex]
            .values[pFrame->parentValueIndex] = pFrame->pResult;
        pCtx->frames[pFrame->parentFrameIndex]
            .valueOwned[pFrame->parentValueIndex] = pFrame->resultOwned;
        pFrame->resultOwned = 0u;
    }
    memset(pFrame, 0, sizeof(*pFrame));
    pCtx->frameTop--;
    return PARSER_OK;
}

/** @brief 释放失败路径仍由语义帧持有的值。 @retval 无。 */
static void Parser_ReleaseContext(ParserCtx_T *pCtx)
{
    uint16_t frameIndex = 0u;
    uint16_t valueIndex = 0u;
    for (frameIndex = 0u; frameIndex < pCtx->frameTop; frameIndex++) {
        ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
        for (valueIndex = 0u; valueIndex < PARSER_VALUE_SLOTS; valueIndex++) {
            if ((pFrame->valueOwned[valueIndex] != 0u) &&
                (pFrame->values[valueIndex] != NULL)) {
                Var_Free(pFrame->values[valueIndex]);
            }
        }
        if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL)) {
            Var_Free(pFrame->pResult);
        }
        memset(pFrame, 0, sizeof(*pFrame));
    }
    pCtx->frameTop = 0u;
    pCtx->stackTop = 0u;
}

/** @brief 捕获终结符语义。 @retval 状态码。 */
static int32_t Parser_CaptureTerminal(ParserCtx_T *pCtx,
                                      uint16_t frameIndex,
                                      uint16_t valueIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    if (valueIndex >= PARSER_VALUE_SLOTS) { return PARSER_ERR_STACK; }
    if ((pCtx->current.pName != NULL) || (pCtx->current.name != NULL)) {
        snprintf(pFrame->names[valueIndex],
                 sizeof(pFrame->names[valueIndex]), "%s",
                 (pCtx->current.pName != NULL) ? pCtx->current.pName :
                                                pCtx->current.name);
    }
    if (pCtx->current.pValue != NULL) {
        pFrame->values[valueIndex] = Var_CloneValue(pCtx->current.pValue);
        if (pFrame->values[valueIndex] == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        pFrame->valueOwned[valueIndex] = 1u;
    } else if (pCtx->current.type == TOKEN_NUMBER) {
        pFrame->values[valueIndex] = Var_CreateInt(pCtx->current.value);
        if (pFrame->values[valueIndex] == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        pFrame->valueOwned[valueIndex] = 1u;
    }
    return PARSER_OK;
}
'''
        code += _generator_parse_loop_c()
    if mode == "ll1":
        code += _generator_tree_code(
            analysis, terminals, nonterms, tree
        )
    else:
        # SLR / LALR 的树入口与查询接口，和 LL(1) 产物同名同语义
        code += _generator_lr_tree_api_c(tree)
    code += _generator_ast_markers_c(ast_skip, ast_fold, ast_label)
    code += ("__SEC_DEMO_BEGIN__" + _generator_demo_main_c(tree=tree) +
             "__SEC_DEMO_END__")
    return code, replacements


def generate_parser_c(grammar, tokens, variables=None, token_defs=None,
                      tree=False, var_max_count=32,
                      ast_skip=None, ast_fold=None, ast_label=None,
                      mode="ll1", priority=None, lex_spec=None):
    """生成单文件、内嵌 variable_runtime 且含演示 main 的解析器 C 代码。

    mode="ll1"（默认）走预测分析表；mode="slr" 走 LR(0) 项集 + FOLLOW 的
    SLR(1) 分析表；mode="lalr" 走 LR(1) 项集 + 同心合并的 LALR(1) 分析表。
    SLR / LALR 复用同一套 LR 运行时与栈结构，可直接处理左递归文法，
    冲突先按 %left / %right / %nonassoc / %prec 消解；剩余冲突按 %expect
    处理，没有 %expect 或数量不符时报错（不产出 C 代码）。
    priority 是 load_grammar_text 的优先级元数据（_priority_from_cfg(cfg)）。
    """
    code, replacements = _assemble_parser_code(
        grammar, tokens, variables, token_defs, tree, var_max_count,
        ast_skip, ast_fold, ast_label, mode, priority, lex_spec
    )
    return _apply_markers(code, replacements)


# =====================================================================
# 分文件布局产物：parser.h / parser.c / parser_token.h / parser_token.c /
# variable_runtime.h / variable_runtime.c / parser_table.c / main.c /
# RESULT.md，与 out/ 目录下的固定结构一一对应。
# =====================================================================

PROJECT_FILES = (
    "parser_config.h",
    "parser.h", "parser.c", "parser_token.h", "parser_token.c",
    "variable_runtime.h", "variable_runtime.c", "parser_table.c",
    "main.c", "RESULT.md",
)


def _collapse_blank_lines(text):
    """把连续 3 个以上的换行压成一个空行（去掉分段留下的空档）。"""
    return re.sub(r"\n{3,}", "\n\n", text)


def _file_banner(file_name, brief):
    """分文件产物的统一抬头。"""
    return ("/* %s —— %s\n"
            " * 由 Chapter-script-engine/parser.py 自动生成（__MODE_NAME__）；"
            "请勿手工修改。\n"
            " */\n\n" % (file_name, brief))


def _generator_parser_config_h_c():
    """生成 parser_config.h：用户可自定义的分配器宏。"""
    return (
        "/* parser_config.h —— 分配器配置（自动生成，可直接手工修改）\n"
        " *\n"
        " * 运行时与解析器的所有动态内存都走下面四个宏，默认是 C 运行库的\n"
        " * malloc/calloc/realloc/free。换成自己的分配器（静态池、内存统计、\n"
        " * RTOS 堆、带对齐的分配器……）时只改这四个宏，parser.c 不用动。\n"
        " *\n"
        " * 推荐写法：分配器实现放在你自己的 .c 里，这里只写声明和宏：\n"
        " *\n"
        " *   void *MyPool_Alloc(size_t size);\n"
        " *   void *MyPool_Calloc(size_t count, size_t size);\n"
        " *   void *MyPool_Realloc(void *ptr, size_t size);\n"
        " *   void  MyPool_Free(void *pointer);\n"
        " *\n"
        " *   #define PARSER_MALLOC(size)        MyPool_Alloc((size))\n"
        " *   #define PARSER_CALLOC(count, size) MyPool_Calloc((count), (size))\n"
        " *   #define PARSER_REALLOC(ptr, size)  MyPool_Realloc((ptr), (size))\n"
        " *   #define PARSER_FREE(pointer)       MyPool_Free((pointer))\n"
        " *\n"
        " * 四个宏必须配套：PARSER_CALLOC / PARSER_REALLOC 返回的块要能被\n"
        " * PARSER_FREE 释放。只想换一部分也可以，其余宏有 #ifndef 兜底。\n"
        " * 本头文件会被多个 .c 包含：直接写在头里的辅助函数要用 static inline，\n"
        " * 否则要么报 -Wunused-function，要么多个 .c 重复定义。\n"
        " * 解析栈深等其他开关在 parser.h 里用 -D 覆盖。\n"
        " */\n\n"
        "#ifndef PARSER_CONFIG_H\n"
        "#define PARSER_CONFIG_H\n\n"
        "#include <stdlib.h>\n"
        "\n"
        + _ALLOCATOR_MACROS_C +
        "\n#endif /* PARSER_CONFIG_H */\n"
    )


def _generator_table_decls_c(mode, analysis):
    """parser.c 对 parser_table.c 里那几张表的外部声明（分文件布局）。"""
    terminals = analysis["terminals"]
    nonterms = analysis["nonterms"]
    if mode in ("slr", "lalr"):
        return (
            "/* 分析表定义在 parser_table.c，由链接器解析 */\n"
            "extern const int32_t s_ParserAction[%d][%d];\n"
            "extern const uint16_t s_ParserGoto[%d][%d];\n"
            "extern const uint16_t s_ParserProductionLength[];\n"
            "extern const uint16_t s_ParserProductionLhs[];\n"
            % (len(analysis["states"]), len(terminals),
               len(analysis["states"]), len(nonterms))
        )
    return (
        "/* 预测分析表定义在 parser_table.c，由链接器解析 */\n"
        "extern const int16_t s_ParserTable[%d][%d];\n"
        "extern const uint8_t s_ParserProductionIsLeftEmpty[];\n"
        % (len(nonterms), len(terminals))
    )


def _generator_tree_table_decls_c(mode, tree_enabled):
    """parser.c 对 parser_table.c 里树元数据表的外部声明。"""
    if not tree_enabled:
        return ""
    lines = [
        "/* 树元数据表定义在 parser_table.c，由链接器解析 */",
        "extern const char *const s_ParserTreeNontermNames[];",
    ]
    if mode == "ll1":
        lines += [
            "extern const uint16_t s_ParserTreeProdStart[];",
            "extern const uint16_t s_ParserTreeProdLength[];",
            "extern const uint16_t s_ParserTreeProdSymbols[];",
        ]
    return "\n".join(lines) + "\n"


def _generator_parser_h_c(defines, error_type, tree, ast_enabled):
    """生成 parser.h：公开 API（解析入口 / 错误 / 树 / AST 元数据）。"""
    text = _file_banner(
        "parser.h", "公开 API：Parser_Run / Parser_RunText / Tree / Ast / Error"
    )
    text += "#ifndef PARSER_H\n#define PARSER_H\n\n"
    text += "#include <stdint.h>\n\n"
    text += '#include "variable_runtime.h"\n#include "parser_token.h"\n\n'
    text += defines + "\n"
    text += error_type + "\n"
    text += (
        "/** @brief 解析 Token 数组。 @param pTokens Token 数组。\n"
        " *  @param count 数量。 @retval 结果或 NULL。 */\n"
        "VarValue_T *Parser_Run(const ParserToken_T *pTokens,"
        " uint32_t count);\n\n"
        "/** @brief 解析脚本文本。 @param pText 输入文本。\n"
        " *  @retval 结果或 NULL。 */\n"
        "VarValue_T *Parser_RunText(const char *pText);\n\n"
        "/** @brief 取最近一次错误。 @retval 只读错误结构。 */\n"
        "const ParserError_T *Parser_GetLastError(void);\n"
    )
    if tree:
        text += (
            "\n/* ============ 通用语法树 API（由 %tree 指令启用） ============ */\n"
            "/** @brief 解析文本并返回语法树根节点。 @param pText 输入。\n"
            " *  @retval 根节点或 NULL。 */\n"
            "VarValue_T *Parser_ParseTreeText(const char *pText);\n\n"
            "/** @brief 取节点的 children 数组。 @retval 数组指针。 */\n"
            "VarValue_T *Parser_TreeGetChildren(const VarValue_T *pNode);\n\n"
            "/** @brief 取节点名。 @retval 只读字符串。 */\n"
            "const char *Parser_TreeGetName(const VarValue_T *pNode);\n\n"
            "/** @brief 取终结符文本。 @retval 只读字符串。 */\n"
            "const char *Parser_TreeGetText(const VarValue_T *pNode);\n\n"
            "/** @brief 打印语法树。 @param pRoot 根节点。 @retval 无。 */\n"
            "void Parser_TreePrint(const VarValue_T *pRoot);\n\n"
            "/** @brief 释放语法树。 @param pRoot 根节点。 @retval 无。 */\n"
            "void Parser_TreeFree(VarValue_T *pRoot);\n"
        )
    if ast_enabled:
        text += (
            "\n/* ============ AST 标记查询（由 %ast_* 指令启用） ============ */\n"
            "/** @brief 该节点是否在 AST 中消失并提升子节点。"
            " @retval 1 需要。 */\n"
            "uint8_t Parser_AstShouldSkip(const char *pName);\n\n"
            "/** @brief 该节点是否折叠成左结合二叉树。 @retval 1 需要。 */\n"
            "uint8_t Parser_AstShouldFold(const char *pName);\n\n"
            "/** @brief 该节点是否取首个终结符子节点作标签。"
            " @retval 1 需要。 */\n"
            "uint8_t Parser_AstShouldLabel(const char *pName);\n"
        )
    text += "\n#endif /* PARSER_H */\n"
    return text


def _generator_parser_token_h_c(token_types, lex_decls=""):
    """生成 parser_token.h：Token 枚举、ParserToken_T 与名称接口。"""
    text = _file_banner(
        "parser_token.h", "Token 枚举、ParserToken_T 与 Token 名称接口"
    )
    text += "#ifndef PARSER_TOKEN_H\n#define PARSER_TOKEN_H\n\n"
    text += "#include <stdint.h>\n\n"
    text += '#include "variable_runtime.h"\n\n'
    text += token_types + "\n"
    text += (
        "/** @brief 取 Token 枚举名。 @param type Token 类型。\n"
        " *  @retval 只读名字或 NULL。 */\n"
        "const char *Parser_TokenName(ParserTokenType_T type);\n\n"
        "/** @brief 把 Token 的文本写进缓冲区。 @param pToken Token。\n"
        " *  @param pBuffer 输出缓冲区。 @param size 容量。 @retval 无。 */\n"
        "void Parser_TokenText(const ParserToken_T *pToken, char *pBuffer,\n"
        "                      uint32_t size);\n"
    )
    if lex_decls:
        text += "\n" + lex_decls
    text += "\n#endif /* PARSER_TOKEN_H */\n"
    return text


def _generator_parser_token_c(token_impl):
    """生成 parser_token.c：Token 名称表与 Token 文本辅助函数。"""
    text = _file_banner(
        "parser_token.c", "Token 名称表与 Token 文本辅助函数"
    )
    text += "#include <stdio.h>\n#include <string.h>\n\n"
    text += '#include "parser_token.h"\n\n'
    text += token_impl
    return text


def _generator_parser_table_c(tree, table_text, tree_tables_text):
    """生成 parser_table.c：分析表 / 产生式表 / 树元数据表。"""
    text = _file_banner(
        "parser_table.c",
        "预测分析表 / ACTION / GOTO / 产生式表 / 树元数据表"
    )
    text += "#include <stdint.h>\n\n"
    text += '#include "parser.h"\n\n'
    # 分文件布局下表不再是 static：parser.c 通过 extern 引用它们
    text += re.sub(r"(?m)^static const ", "const ", table_text)
    if tree:
        text += "\n"
        text += re.sub(r"(?m)^static const ", "const ", tree_tables_text)
    return text


def _generator_main_c(tree=False):
    """生成 main.c：调用 Parser_RunText 的独立演示程序。"""
    text = _file_banner("main.c", "演示 main，调用 Parser_RunText")
    text += "#include <stdio.h>\n\n"
    text += '#include "parser.h"\n'
    text += _generator_demo_main_c(standalone=True, tree=tree)
    return text


def _generator_result_md(mode, tree, ast_skip, ast_fold, ast_label,
                         source_name, report_text, var_max_count,
                         analysis=None, lex_spec=None):
    """生成 RESULT.md：生成信息、产物清单、编译运行说明与分析报告。"""
    mode_label = {"slr": "SLR(1)", "lalr": "LALR(1)"}.get(mode, "LL(1)")
    ast_names = []
    for name, values in (("skip", ast_skip), ("fold", ast_fold),
                         ("label", ast_label)):
        if values:
            ast_names.append("%s: %s" % (name, " ".join(sorted(values))))
    priority = (analysis or {}).get("priority") or {}
    lex_spec = _normalize_lex_spec(lex_spec)
    precedence = priority.get("precedence") or {}
    associativity = priority.get("associativity") or {}
    prec_text = "；".join(
        "%%%s %s" % (
            {"left": "left", "right": "right",
             "nonassoc": "nonassoc"}.get(associativity.get(token), "left"),
            token,
        )
        for token in sorted(precedence, key=lambda name: (precedence[name],
                                                          name))
    )
    lines = [
        "# 生成结果",
        "",
        "## 生成信息",
        "",
        "| 项目 | 值 |",
        "| --- | --- |",
        "| 生成器 | Chapter-script-engine/parser.py（单文件 "
        "LL(1) / SLR(1) / LALR(1) 生成器） |",
        "| 源文法 | %s |" % (source_name or "(命令行内联文法)"),
        "| 生成模式 | %s |" % mode_label,
        "| %%tree | %s |" % ("启用" if tree else "未启用"),
        "| %%ast_* | %s |" % ("；".join(ast_names) if ast_names else "未启用"),
        "| 变量表容量 | %d |" % int(var_max_count),
        "| 优先级指令 | %s |" % (prec_text or "未使用"),
        "| %%expect | %s |" % (
            priority.get("expect_conflicts")
            if priority.get("expect_conflicts") is not None else "未使用"
        ),
        "| %%regex | %s |" % ("；".join(
            "%s: %s" % (name, pattern)
            for name, pattern in lex_spec["regex"].items()
        ) or "未使用"),
        "| %%identifier | %s |" % (lex_spec["identifier"] or "未使用"),
        "",
        "## 产物文件",
        "",
    ]
    for name in PROJECT_FILES:
        lines.append("- `%s`" % name)
    lines += [
        "",
        "## 编译运行",
        "",
        "```sh",
        "# 在 out/ 目录下整体编译（main.c 是演示程序，不需要时删掉即可）",
        "gcc -std=c99 -Wall -Wextra -O2 *.c -lm -o parser_demo",
        "./parser_demo                 # 用默认输入",
        './parser_demo "<你的输入>"    # 用命令行输入',
        "```",
        "",
        "只当解析库用时去掉 `main.c`；链接时 parser.c 需要 parser_token.c、",
        "parser_table.c、variable_runtime.c 一起编译。",
        "",
        "## 自定义分配器",
        "",
        "运行时与解析器的所有动态内存都走 `parser_config.h` 里的四个宏，",
        "默认是 C 运行库的 malloc/calloc/realloc/free：",
        "",
        "```c",
        "#define PARSER_MALLOC(size)        MyPool_Alloc((size))",
        "#define PARSER_CALLOC(count, size) MyPool_Calloc((count), (size))",
        "#define PARSER_REALLOC(ptr, size)  MyPool_Realloc((ptr), (size))",
        "#define PARSER_FREE(pointer)       MyPool_Free((pointer))",
        "```",
        "",
        "改成自己的分配器后重新编译即可，parser.c / variable_runtime.c 不用动。",
        "",
    ]
    if tree:
        lines += [
            "文法是纯 `%tree`（不写语义动作）时 `Parser_RunText` 没有求值结果，"
            "演示程序会自动改打印语法树。直接用 API 拿 CST：",
            "",
            "```c",
            'VarValue_T *pTree = Parser_ParseTreeText("...");',
            "if (pTree != NULL) { Parser_TreePrint(pTree); }",
            "Parser_TreeFree(pTree);",
            "```",
            "",
        ]
    lines += [
        "## 冲突消解",
        "",
    ]
    resolutions = (analysis or {}).get("resolutions") or []
    if resolutions:
        for entry in resolutions:
            lines.append("- %s" % _resolution_line(entry))
    else:
        lines.append("（无）")
    lines += [
        "",
        "## 未消解冲突",
        "",
    ]
    unresolved = (analysis or {}).get("unresolved") or []
    if unresolved:
        for item in unresolved:
            lines.append("- %s" % item["text"])
    else:
        lines.append("（无）")
    lines += [""]
    lines += ["## 文法分析报告", "", "```text",
              report_text.rstrip(), "```", ""]
    return "\n".join(lines)


def generate_parser_project(grammar, tokens, variables=None, token_defs=None,
                            tree=False, var_max_count=32,
                            ast_skip=None, ast_fold=None, ast_label=None,
                            mode="ll1", source_name=None, report_text="",
                            priority=None, lex_spec=None):
    """生成分文件工程，返回 {文件名: 文本}（顺序与 out/ 目录一致）。

    与 generate_parser_c 共用同一份拼装逻辑，这里按 __SEC_*__ 分段标记把
    Token 模块、分析表、树元数据表、演示 main 拆到各自文件，
    parser.c 只留词法器、上下文、语义动作、主循环与树 / AST 实现。
    """
    mode = str(mode or "ll1").lower()
    if mode not in ("ll1", "slr", "lalr"):
        raise GrammarLoadError(
            "未知的生成模式：%s（只支持 ll1、slr 或 lalr）" % mode
        )
    tree = bool(tree)
    ast_skip = set(ast_skip or ())
    ast_fold = set(ast_fold or ())
    ast_label = set(ast_label or ())
    raw, replacements = _assemble_parser_code(
        grammar, tokens, variables, token_defs, tree, var_max_count,
        ast_skip, ast_fold, ast_label, mode, priority, lex_spec
    )
    analysis = _parser_analysis(grammar, tokens, mode, priority)
    defines = _extract_section(raw, "DEFINES")
    token_types = _extract_section(raw, "TOKEN_TYPES")
    token_impl = _extract_section(raw, "TOKEN_IMPL")
    error_type = _extract_section(raw, "ERROR_TYPE")
    # 表先展开再进 parser_table.c：定义在分文件布局里不再是 static
    tables = _apply_markers(_extract_section(raw, "TABLES"), replacements)
    tree_tables = _apply_markers(
        _extract_section(raw, "TREE_TABLES"), replacements
    ) if tree else ""
    ast_enabled = bool(ast_skip or ast_fold or ast_label)

    # parser.c：去掉 Token 模块、表与演示 main，换成对 parser_table.c 的声明
    body = raw[raw.index(_section_marker("RUNTIME_H", "BEGIN")):]
    parser_c = _replace_section(body, "RUNTIME_H", "")
    parser_c = _replace_section(parser_c, "RUNTIME_C", "")
    parser_c = _replace_section(parser_c, "DEFINES", "")
    parser_c = _replace_section(parser_c, "TOKEN_TYPES", "")
    parser_c = _replace_section(parser_c, "TOKEN_IMPL", "")
    parser_c = _replace_section(parser_c, "ERROR_TYPE", "")
    parser_c = _replace_section(
        parser_c, "TABLES", _generator_table_decls_c(mode, analysis)
    )
    parser_c = _replace_section(
        parser_c, "TREE_TABLES", _generator_tree_table_decls_c(mode, tree)
    )
    parser_c = _replace_section(parser_c, "DEMO", "")
    parser_c = (
        _file_banner(
            "parser.c",
            "词法器、ParserCtx_T、语义动作、主循环、树 API、AST 元数据"
        )
        + "#include <ctype.h>\n#include <errno.h>\n#include <math.h>\n"
        + "#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n"
        + "#include <string.h>\n\n"
        + '#include "parser.h"\n#include "parser_token.h"\n'
        + parser_c
    )

    project = {
        "parser_config.h": _generator_parser_config_h_c(),
        "parser.h": _generator_parser_h_c(defines, error_type, tree,
                                          ast_enabled),
        "parser.c": parser_c,
        "parser_token.h": _generator_parser_token_h_c(
            token_types,
            _apply_markers(replacements.get("__LEX_DFA_DECLS__", ""),
                           replacements),
        ),
        "parser_token.c": _generator_parser_token_c(token_impl),
        "variable_runtime.h": _with_allocator_include(VARIABLE_RUNTIME_HEADER),
        "variable_runtime.c": _with_allocator_macros(VARIABLE_RUNTIME_SOURCE),
        "parser_table.c": _generator_parser_table_c(
            tree, tables, tree_tables
        ),
        "main.c": _generator_main_c(tree),
        "RESULT.md": _generator_result_md(
            mode, tree, ast_skip, ast_fold, ast_label,
            source_name, report_text, var_max_count, analysis, lex_spec
        ),
    }
    return {name: _collapse_blank_lines(_apply_markers(project[name],
                                                       replacements))
            for name in PROJECT_FILES}


def write_parser_project(out_dir, project):
    """把 generate_parser_project 的产物写进 out_dir（UTF-8，自动建目录）。"""
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    written = []
    for name in PROJECT_FILES:
        path = os.path.join(out_dir, name)
        with io.open(path, "w", encoding="utf-8", newline="\n") as file_handle:
            file_handle.write(project[name])
        written.append(path)
    return written


def main():
    parser = argparse.ArgumentParser(
        description="Yacc 风格非递归 LL(1) / SLR(1) / LALR(1) 解析器生成器"
    )
    parser.add_argument("-o", "--output", default="parser.c",
                        help="输出 C 文件名")
    parser.add_argument("--out-dir", default=None,
                        help="按分文件布局输出到该目录：parser_config.h / "
                             "parser.h / parser.c / parser_token.h / "
                             "parser_token.c / "
                             "variable_runtime.h / variable_runtime.c / "
                             "parser_table.c / main.c / RESULT.md")
    parser.add_argument("--split", action="store_true",
                        help="等价于 --out-dir out（分文件布局）")
    parser.add_argument("--grammar-file", default=None,
                        help="文法文件（仅支持 .gram 文本格式）")
    parser.add_argument("--report", default=None,
                        help="把文法分析报告另存为 UTF-8 文本")
    parser.add_argument("--mode", choices=("ll1", "slr", "lalr"), default=None,
                        help="生成模式（默认 ll1；与 .gram 的 %%mode 冲突时报错）")
    parser.add_argument("--dump-lr-automaton", action="store_true",
                        help="打印 SLR(1) 的 LR(0) / LALR(1) 的状态、项集与转移")
    parser.add_argument("--dump-slr-tables", action="store_true",
                        help="打印 SLR(1) / LALR(1) 的 ACTION / GOTO 表")
    args = parser.parse_args()

    if args.grammar_file:
        cfg = load_grammar_file(args.grammar_file)
    else:
        cfg = load_grammar_text(DEFALUT_LOAD_TEXT)
    grammar = cfg["grammar"]
    tokens = cfg["tokens"]
    variables = cfg.get("variables", {})

    token_defs = cfg.get("token_defs", {})

    # 模式策略：--mode 与 .gram 里的 %mode 同时存在时必须一致，
    # 不一致直接报错，不做“命令行优先”的隐式覆盖；
    # 文法没写 %mode 时以命令行为准（缺省仍是 ll1）。
    mode = cfg.get("mode", "ll1")
    mode_declared = bool(cfg.get("mode_declared", False))
    if mode_declared and (args.mode is not None) and (args.mode != mode):
        parser.error(
            "--mode %s 与 .gram 中的 %%mode %s 不一致；"
            "请删除其一或改成一致" % (args.mode, mode)
        )
    if args.mode is not None:
        mode = args.mode

    # 优先级元数据：%left / %right / %nonassoc / %prec / %expect
    priority = _priority_from_cfg(cfg)
    # 词法元数据：%regex / %identifier
    lex_spec = _lex_spec_from_cfg(cfg)
    if mode == "lalr":
        analysis = _lalr_analyze(grammar, tokens, priority)
        report = format_lalr_report(analysis)
    elif mode == "slr":
        analysis = _slr_analyze(grammar, tokens, priority)
        report = format_slr_report(analysis)
    else:
        analysis = prepare_grammar(grammar, tokens)
        report = format_grammar_report(analysis)
        if priority["precedence"] or (
                priority["expect_conflicts"] is not None):
            report += ("\n[提示] LL(1) 模式忽略 %left / %right / %nonassoc / "
                       "%prec 优先级指令与 %expect\n")
    print(report, end="")
    if args.report is not None:
        with open(args.report, "w", encoding="utf-8") as file_handle:
            file_handle.write(report)
    if args.dump_lr_automaton or args.dump_slr_tables:
        if mode not in ("slr", "lalr"):
            parser.error(
                "--dump-lr-automaton / --dump-slr-tables 只在 "
                "--mode slr 或 --mode lalr 下可用"
            )
        dump_automaton = (format_lalr_automaton if mode == "lalr"
                          else format_slr_automaton)
        dump_tables = (format_lalr_tables if mode == "lalr"
                       else format_slr_tables)
        if args.dump_lr_automaton:
            print(dump_automaton(analysis), end="")
        if args.dump_slr_tables:
            print(dump_tables(analysis), end="")
    code = generate_parser_c(
        grammar, tokens, variables, token_defs,
        tree=bool(cfg.get("tree", False)),
        var_max_count=cfg.get("var_max_count") or 32,
        ast_skip=cfg.get("ast_skip"),
        ast_fold=cfg.get("ast_fold"),
        ast_label=cfg.get("ast_label"),
        mode=mode,
        priority=priority,
        lex_spec=lex_spec,
    )
    if args.split or (args.out_dir is not None):
        out_dir = args.out_dir if args.out_dir is not None else "out"
        project = generate_parser_project(
            grammar, tokens, variables, token_defs,
            tree=bool(cfg.get("tree", False)),
            var_max_count=cfg.get("var_max_count") or 32,
            ast_skip=cfg.get("ast_skip"),
            ast_fold=cfg.get("ast_fold"),
            ast_label=cfg.get("ast_label"),
            mode=mode,
            source_name=args.grammar_file,
            report_text=report,
            priority=priority,
            lex_spec=lex_spec,
        )
        written = write_parser_project(out_dir, project)
        print("生成成功（分文件布局）: %s（%d 个文件）"
              % (out_dir, len(written)))
        for path in written:
            print("  %s" % path)
        return 0
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(code)
    print(f"生成成功: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
