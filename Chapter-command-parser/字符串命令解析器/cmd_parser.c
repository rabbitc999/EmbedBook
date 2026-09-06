/**
 * @file    cmd_parser.c
 * @brief   基于词法切分 + 哈希表的字符串命令解析器（支持动态类型参数）
 * @details 三个层次：
 *          1. 字符串级：把一行命令切成 argc 个参数；
 *          2. 动态类型解析：把每个参数字符串识别为 INT/UINT/FLOAT/STRING，
 *             并同时保存原始文本与解析后的数值；
 *          3. 函数列表适配：handler 通过 INT_PARAM/FLOAT_PARAM/STR_PARAM
 *             宏读取参数，再把数值传给具体业务函数。
 */

#include <string.h>
#include <stdlib.h>
#include "cmd_parser.h"

/**
 * @brief  计算命令名的哈希值并映射到表下标
 * @param  pName 命令名字符串
 * @retval 0~CMD_TABLE_CAP-1
 * @note   FNV-1a 算法，仅用于均匀散列，不承担安全用途
 */
static uint8_t Cmd_CalcHash(const char *pName)
{
    uint32_t hash = 2166136261u;
    const uint8_t *p = (const uint8_t *)pName;

    while (*p != '\0') {
        hash ^= (uint32_t)*p++;
        hash *= 16777619u;
    }
    return (uint8_t)(hash % CMD_TABLE_CAP);
}

/**
 * @brief  把一段参数字符串识别为动态类型
 * @param  pToken 以 '\0' 结尾的参数文本
 * @param  pParam 输出参数
 * @retval 错误码
 */
static int8_t Cmd_ParseParam(const char *pToken, Param_T *pParam)
{
    char *pEnd = NULL;
    long intValue = 0;
    float floatValue = 0.0f;

    /* 复制原始字符串 */
    strncpy(pParam->text, pToken, CMD_PARAM_TEXT_MAX - 1u);
    pParam->text[CMD_PARAM_TEXT_MAX - 1u] = '\0';

    if (pToken[0] == '\0') {
        pParam->type = PARAM_TYPE_NONE;
        return CMD_ERR_OK;
    }

    /* 十六进制：0x10 */
    if ((pToken[0] == '0') &&
        ((pToken[1] == 'x') || (pToken[1] == 'X'))) {
        pParam->uintValue = strtoul(pToken, &pEnd, 0);
        if (*pEnd == '\0') {
            pParam->type = PARAM_TYPE_UINT;
            pParam->intValue = (int32_t)pParam->uintValue;
            pParam->floatValue = (float)pParam->uintValue;
            return CMD_ERR_OK;
        }
    }

    /* 先按十进制整数解析 */
    intValue = strtol(pToken, &pEnd, 10);
    if ((*pEnd == '\0') && (pEnd != pToken)) {
        pParam->type = PARAM_TYPE_INT;
        pParam->intValue = (int32_t)intValue;
        pParam->uintValue = (uint32_t)intValue;
        pParam->floatValue = (float)intValue;
        return CMD_ERR_OK;
    }

    /* 再按浮点数解析：3.5、1e2 */
    floatValue = strtof(pToken, &pEnd);
    if ((*pEnd == '\0') && (pEnd != pToken)) {
        pParam->type = PARAM_TYPE_FLOAT;
        pParam->floatValue = floatValue;
        pParam->intValue = (int32_t)floatValue; /* 供 INT_PARAM 转换 */
        pParam->uintValue = (uint32_t)floatValue;
        return CMD_ERR_OK;
    }

    /* 不是数值：按字符串处理 */
    pParam->type = PARAM_TYPE_STRING;
    pParam->intValue = 0;
    pParam->uintValue = 0u;
    pParam->floatValue = 0.0f;
    return CMD_ERR_OK;
}

/**
 * @brief  把一行命令切分为动态类型参数数组
 * @param  pLine   可修改的命令行缓冲
 * @param  argv    参数数组
 * @param  maxArgs 最大参数个数
 * @retval 参数个数
 */
static uint8_t Cmd_Tokenize(char *pLine, Param_T *argv, uint8_t maxArgs)
{
    uint8_t argc = 0;
    char *p = pLine;
    char *pStart = NULL;
    size_t length = 0;

    while (*p != '\0') {
        /* 跳过空格 */
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (argc >= maxArgs) {
            break;
        }

        pStart = p;
        while ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
            p++;
        }

        /* 截断过长的单个参数，避免写入越界 */
        length = (size_t)(p - pStart);
        if (length >= CMD_PARAM_TEXT_MAX) {
            length = CMD_PARAM_TEXT_MAX - 1u;
        }
        memcpy(argv[argc].text, pStart, length);
        argv[argc].text[length] = '\0';
        Cmd_ParseParam(argv[argc].text, &argv[argc]);
        argc++;

        if (*p != '\0') {
            *p = '\0';   /* 用结束符切开参数 */
            p++;
        }
    }
    return argc;
}

/**
 * @brief  在哈希表中查找命令
 * @param  pParser 解析器句柄
 * @param  pName   命令名
 * @retval 表项指针；未找到返回 NULL
 */
static CmdItem_T *Cmd_FindItem(CmdParser_T *pParser, const char *pName)
{
    uint8_t start = Cmd_CalcHash(pName);
    uint8_t index = start;
    uint8_t loop = 0;

    while (loop < CMD_TABLE_CAP) {
        if ((pParser->table[index].used != 0u) &&
            (strcmp(pParser->table[index].name, pName) == 0)) {
            return &pParser->table[index];
        }
        index = (uint8_t)((index + 1u) % CMD_TABLE_CAP);
        loop++;
    }
    return NULL;
}

/**
 * @brief  初始化解析器
 * @param  pParser 解析器句柄
 * @retval 错误码
 */
int8_t Cmd_ParserInit(CmdParser_T *pParser)
{
    uint8_t index = 0;

    if (pParser == NULL) {
        return CMD_ERR_EMPTY;
    }
    memset(pParser, 0, sizeof(CmdParser_T));
    for (index = 0; index < CMD_TABLE_CAP; index++) {
        pParser->table[index].used = 0u;
    }
    return CMD_ERR_OK;
}

/**
 * @brief  注册一条命令
 * @param  pParser   解析器句柄
 * @param  pCmdName  命令名
 * @param  handler   处理函数
 * @retval 错误码
 */
int8_t Cmd_ParserRegister(CmdParser_T *pParser,
                          const char *pCmdName,
                          CmdHandler_F handler)
{
    uint8_t start = 0;
    uint8_t index = 0;
    uint8_t loop = 0;

    if ((pParser == NULL) || (pCmdName == NULL) || (handler == NULL)) {
        return CMD_ERR_EMPTY;
    }
    if (pParser->count >= CMD_TABLE_CAP) {
        return CMD_ERR_TABLE_FULL;
    }
    if (strlen(pCmdName) >= CMD_NAME_MAX) {
        return CMD_ERR_TOO_LONG;
    }

    start = Cmd_CalcHash(pCmdName);
    index = start;

    while (loop < CMD_TABLE_CAP) {
        if (pParser->table[index].used == 0u) {
            strncpy(pParser->table[index].name, pCmdName, CMD_NAME_MAX - 1u);
            pParser->table[index].name[CMD_NAME_MAX - 1u] = '\0';
            pParser->table[index].handler = handler;
            pParser->table[index].used = 1u;
            pParser->count++;
            return CMD_ERR_OK;
        }
        if (strcmp(pParser->table[index].name, pCmdName) == 0) {
            pParser->table[index].handler = handler;
            return CMD_ERR_OK;
        }
        index = (uint8_t)((index + 1u) % CMD_TABLE_CAP);
        loop++;
    }
    return CMD_ERR_TABLE_FULL;
}

/**
 * @brief  解析并执行一行命令
 * @param  pParser 解析器句柄
 * @param  pLine   原始命令行，如 "car 1.0 2.0 0.5"
 * @retval 错误码
 */
int8_t Cmd_ParserRun(CmdParser_T *pParser, const char *pLine)
{
    char lineBuffer[CMD_LINE_MAX];
    Param_T argv[CMD_ARGS_MAX];
    uint8_t argc = 0;
    CmdItem_T *pItem = NULL;

    if ((pParser == NULL) || (pLine == NULL)) {
        return CMD_ERR_EMPTY;
    }
    if (strlen(pLine) >= CMD_LINE_MAX) {
        return CMD_ERR_TOO_LONG;
    }

    strcpy(lineBuffer, pLine);
    argc = Cmd_Tokenize(lineBuffer, argv, CMD_ARGS_MAX);
    if (argc == 0u) {
        return CMD_ERR_EMPTY;
    }
    if (argc >= CMD_ARGS_MAX) {
        return CMD_ERR_TOO_MANY_ARGS;
    }

    pItem = Cmd_FindItem(pParser, argv[0].text);
    if (pItem == NULL) {
        return CMD_ERR_NOT_FOUND;
    }
    if (pItem->handler != NULL) {
        return pItem->handler(argc, argv);
    }
    return CMD_ERR_EXEC;
}

/**
 * @brief  获取参数类型的字符串名
 * @param  type 参数类型
 * @retval 类型名字符串
 */
const char *Cmd_ParamTypeName(ParamType_T type)
{
    switch (type) {
        case PARAM_TYPE_INT:    return "INT";
        case PARAM_TYPE_UINT:   return "UINT";
        case PARAM_TYPE_FLOAT:  return "FLOAT";
        case PARAM_TYPE_STRING: return "STRING";
        default:                return "NONE";
    }
}
