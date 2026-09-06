/**
 * @file    variable_runtime_test.c
 * @brief   VarValue_T 动态变量运行时测试
 * @details 验证 string/array/dict/struct、切片、拼接、类型检查与嵌套。
 */

#include <stdio.h>
#include <string.h>
#include "variable_runtime.h"

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

static void Test_StringSliceConcat(void)
{
    VarValue_T *pHello = Var_CreateString("Hello ");
    VarValue_T *pWorld = Var_CreateString("World");
    VarValue_T *pResult = NULL;
    VarValue_T *pSlice = NULL;

    Check(Var_Concat(pHello, pWorld, &pResult) == VAR_RUNTIME_OK,
          "字符串拼接：类型检查通过");
    Check(strcmp(pResult->u.stringValue, "Hello World") == 0,
          "字符串拼接：内容正确");

    Check(Var_Slice(pResult, 1, 4, &pSlice) == VAR_RUNTIME_OK,
          "字符串切片：调用成功");
    Check(strcmp(pSlice->u.stringValue, "ell") == 0,
          "字符串切片：内容正确");

    Var_Free(pHello);
    Var_Free(pWorld);
    Var_Free(pResult);
    Var_Free(pSlice);
}

static void Test_ArrayTempSliceConcat(void)
{
    VarValue_T *pArray = Var_CreateArray();
    VarValue_T *pSlice = NULL;
    VarValue_T *pConcat = NULL;
    VarValue_T *pString = NULL;
    size_t index = 0u;

    /* 临时数组 [1,2,3,4,5] */
    for (index = 0; index < 5; index++) {
        Var_ArrayPush(pArray, Var_CreateInt((int32_t)(index + 1)));
    }

    /* 切片 [1:3] -> [2,3] */
    Check(Var_Slice(pArray, 1, 3, &pSlice) == VAR_RUNTIME_OK,
          "数组切片：调用成功");
    Check(pSlice->u.array.length == 2u, "数组切片：长度正确");
    Check(pSlice->u.array.items[0]->u.intValue == 2,
          "数组切片：首元素正确");

    /* 数组拼接 */
    Check(Var_Concat(pArray, pSlice, &pConcat) == VAR_RUNTIME_OK,
          "数组拼接：类型检查通过");
    Check(pConcat->u.array.length == 7u, "数组拼接：长度正确");

    /* 类型检查：字符串 + 数组应失败 */
    pString = Var_CreateString("x");
    Check(Var_CanConcat(pString, pArray) != VAR_RUNTIME_OK,
          "类型检查：string+array 拒绝");
    Var_Free(pString);

    Var_Free(pArray);
    Var_Free(pSlice);
    Var_Free(pConcat);
}

static void Test_DictStructNested(void)
{
    VarValue_T *pDict = Var_CreateDict();
    VarValue_T *pStruct = Var_CreateStruct();
    VarValue_T *pArray = Var_CreateArray();
    VarValue_T *pResult = NULL;

    /* 字典 */
    Var_DictSet(pDict, "name", Var_CreateString("rabbit"));
    Var_DictSet(pDict, "score", Var_CreateDouble(99.5));
    pResult = Var_DictGet(pDict, "name");
    Check((pResult != NULL) &&
          (strcmp(pResult->u.stringValue, "rabbit") == 0),
          "字典：按 key 读取字符串");

    /* 结构体 + 内嵌数组 */
    Var_StructSetField(pStruct, "x", Var_CreateFloat(1.5f));
    Var_StructSetField(pStruct, "arr", pArray);
    Var_ArrayPush(pArray, Var_CreateInt(10));
    Var_ArrayPush(pArray, Var_CreateInt(20));

    pResult = Var_StructGetField(pStruct, "arr");
    Check((pResult != NULL) &&
          (pResult->type == VAR_TYPE_ARRAY) &&
          (pResult->u.array.length == 2u),
          "结构体：内嵌数组访问成功");

    Var_Free(pDict);
    Var_Free(pStruct);
}

int main(void)
{
    Test_StringSliceConcat();
    Test_ArrayTempSliceConcat();
    Test_DictStructNested();

    if (s_FailCount == 0) {
        printf("\nVarValue runtime test passed\n");
        return 0;
    }
    printf("\nVarValue runtime test failed: %d\n", s_FailCount);
    return 1;
}
