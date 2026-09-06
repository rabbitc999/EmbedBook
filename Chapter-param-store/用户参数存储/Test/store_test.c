/**
 * @file    store_test.c
 * @brief   第12章 store.c 参数存储模块测试
 * @details 在 PC 上通过模拟 Flash/FatFS 测试：
 *          默认参数、魔数恢复、分组保存、文件导入导出、外部数据接口。
 */

#include <stdio.h>
#include <string.h>
#include "store.h"

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

/* 测试默认参数 + 魔数恢复 */
static void Test_DefaultAndMagic(void)
{
    Param_Clear();
    Param_Init();

    Check(g_ParamData.MagicNumber == PARAM_MAGIC_NUMBER,
          "默认恢复：MagicNumber 正确");
    Check(g_ParamData.car_wheelBase == 16.0f,
          "默认恢复：wheelBase = 16.0");
    Check(g_Car.remote.wheelBase == 16.0f,
          "默认恢复：wheelBase 同步到运行全局变量");
    Check(g_ParamData.car_MaxSpeedX == 0.3f,
          "默认恢复：MaxSpeedX = 0.3");
}

/* 测试分组保存 */
static void Test_GroupSave(void)
{
    g_Car.remote.pidLocationX.Kp = 9.9f;
    g_Car.remote.pidLocationX.Ki = 1.1f;

    Param_Save(PARAM_SAVE_LOC_X_PID_DATA);

    Check(g_ParamData.MagicNumber == PARAM_MAGIC_NUMBER,
          "分组保存：MagicNumber 被写入");
    Check(g_ParamData.car_pidLocationX.Kp == 9.9f,
          "分组保存：Kp 被同步到参数结构体");
    Check(g_ParamData.car_pidLocationX.Ki == 1.1f,
          "分组保存：Ki 被同步到参数结构体");
}

/* 测试文件导出/导入 */
static void Test_FileExportImport(void)
{
    const char *pFilePath = "param.bin";

    /* 先写入一个便于验证的数值 */
    g_ParamData.car_MaxSpeedX = 0.8f;
    Param_Export((char *)pFilePath);

    /* 修改当前值后再从文件导入，应恢复 0.8 */
    g_ParamData.car_MaxSpeedX = 0.1f;
    g_Car.remote.remoteMaxSpeedX = 0.1f;

    Param_Load((char *)pFilePath);

    Check(g_ParamData.car_MaxSpeedX == 0.8f,
          "文件导入：MaxSpeedX 恢复为 0.8");
    Check(g_Car.remote.remoteMaxSpeedX == 0.8f,
          "文件导入：MaxSpeedX 同步到运行全局变量");
}

/* 测试外部数据结构接口 */
static void Test_ExternalData(void)
{
    ParamData_T data;

    memset(&data, 0, sizeof(data));

    /* 空指针应被忽略 */
    Param_LoadFromData(NULL);
    Param_ExportToData(NULL);

    /* 魔数错误应拒绝 */
    data.MagicNumber = 0xDEADBEEFu;
    Param_LoadFromData(&data);
    Check(g_ParamData.car_MaxSpeedX == 0.8f,
          "外部数据：错误魔数被拒绝，原参数不变");

    /* 合法数据 */
    g_ParamData.car_MaxSpeedX = 1.5f;
    Param_ExportToData(&data);
    Check(data.MagicNumber == PARAM_MAGIC_NUMBER,
          "外部数据：导出后 MagicNumber 正确");
    Check(data.car_MaxSpeedX == 1.5f,
          "外部数据：导出后 MaxSpeedX 正确");

    data.car_MaxSpeedX = 2.5f;
    data.MagicNumber = PARAM_MAGIC_NUMBER;
    Param_LoadFromData(&data);
    Check(g_Car.remote.remoteMaxSpeedX == 2.5f,
          "外部数据：合法数据可加载并同步到全局变量");
}

int main(void)
{
    Test_DefaultAndMagic();
    Test_GroupSave();
    Test_FileExportImport();
    Test_ExternalData();

    if (s_FailCount == 0) {
        printf("\n所有 store 参数存储测试通过\n");
        return 0;
    }
    printf("\nstore 测试失败 %d 项\n", s_FailCount);
    return 1;
}
