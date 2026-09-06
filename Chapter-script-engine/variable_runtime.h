#ifndef __VARIABLE_RUNTIME_H
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
VarValue_T *Var_Neg(VarValue_T *pValue);
VarValue_T *Var_Not(VarValue_T *pValue);
VarValue_T *Var_And(VarValue_T *pLeft, VarValue_T *pRight);
VarValue_T *Var_Or(VarValue_T *pLeft, VarValue_T *pRight);

/* 返回码 */
#define VAR_RUNTIME_OK               0
#define VAR_RUNTIME_ERR_TYPE        -1
#define VAR_RUNTIME_ERR_BOUND       -2
#define VAR_RUNTIME_ERR_NOT_FOUND   -3
#define VAR_RUNTIME_ERR_MEMORY      -4

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
