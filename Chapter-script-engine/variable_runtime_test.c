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

/**
 * @brief  验证强类型 Getter 的转换、截断和范围检查。
 * @retval 无。
 * @note   转换失败时输出参数必须保持调用前的值。
 */
static void Test_StrictGetters(void)
{
    VarValue_T *pLarge = Var_CreateInt(300);
    VarValue_T *pFloat = Var_CreateFloat(1.9f);
    VarValue_T *pString = Var_CreateString("12");
    uint8_t uint8Result = 77u;
    int32_t int32Result = -1;
    double doubleResult = 0.0;

    Check(Var_GetUint8(pLarge, &uint8Result) == VAR_RUNTIME_ERR_RANGE,
          "Getter：uint8 越界被拒绝");
    Check(uint8Result == 77u, "Getter：失败后输出参数不变");
    Check(Var_GetInt32(pFloat, &int32Result) == VAR_RUNTIME_OK,
          "Getter：float 可以转换为 int32");
    Check(int32Result == 1, "Getter：浮点转整数截断小数");
    Check(Var_GetDouble(pLarge, &doubleResult) == VAR_RUNTIME_OK,
          "Getter：整数可以转换为 double");
    Check(doubleResult == 300.0, "Getter：double 转换结果正确");
    Check(Var_GetInt32(pString, &int32Result) == VAR_RUNTIME_ERR_TYPE,
          "Getter：字符串不能隐式转换为数字");

    Var_Free(pLarge);
    Var_Free(pFloat);
    Var_Free(pString);
}

/**
 * @brief  验证数组转换失败时不会产生部分结果。
 * @retval 无。
 */
static void Test_ArrayConversionTransaction(void)
{
    VarValue_T *pSource = Var_CreateArray();
    VarValue_T *pTemplate = Var_CreateArray();
    VarValue_T *pResult = NULL;

    /* 第二个字符串元素不能转换为 int32，整次转换应失败。 */
    Var_ArrayPush(pSource, Var_CreateFloat(1.5f));
    Var_ArrayPush(pSource, Var_CreateString("bad"));
    Var_ArrayPush(pTemplate, Var_CreateInt(0));
    Var_ArrayPush(pTemplate, Var_CreateInt(0));
    Check(Var_ConvertValue(pSource, pTemplate, &pResult) ==
          VAR_RUNTIME_ERR_TYPE,
          "数组转换：任一元素失败则整体失败");
    Check(pResult == NULL, "数组转换：失败时不返回部分数组");
    Check(pTemplate->u.array.items[0]->u.intValue == 0,
          "数组转换：失败时类型模板保持不变");

    Var_Free(pSource);
    Var_Free(pTemplate);
}

int main(void)
{
    Test_StringSliceConcat();
    Test_ArrayTempSliceConcat();
    Test_DictStructNested();
    Test_StrictGetters();
    Test_ArrayConversionTransaction();

    if (s_FailCount == 0) {
        printf("\nVarValue runtime test passed\n");
        return 0;
    }
    printf("\nVarValue runtime test failed: %d\n", s_FailCount);
    return 1;
}
