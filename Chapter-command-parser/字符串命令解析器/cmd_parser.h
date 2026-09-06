#ifndef __CMD_PARSER_H
#define __CMD_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 参数与表容量 */
#define CMD_NAME_MAX          16u
#define CMD_LINE_MAX          128u
#define CMD_ARGS_MAX           8u
#define CMD_TABLE_CAP         16u
#define CMD_PARAM_TEXT_MAX     32u

/* 返回码 */
#define CMD_ERR_OK              0
#define CMD_ERR_EMPTY          -1
#define CMD_ERR_TOO_LONG       -2
#define CMD_ERR_TOO_MANY_ARGS  -3
#define CMD_ERR_TABLE_FULL     -4
#define CMD_ERR_NOT_FOUND      -5
#define CMD_ERR_EXEC           -6

/* 动态参数类型 */
typedef enum __ParamType_T
{
    PARAM_TYPE_NONE = 0,
    PARAM_TYPE_INT,       /* 整数，如 12、-3 */
    PARAM_TYPE_UINT,      /* 无符号整数，如 0x10、255 */
    PARAM_TYPE_FLOAT,     /* 浮点数，如 3.5、1e2 */
    PARAM_TYPE_STRING     /* 字符串，如 led、on */
} ParamType_T;

/* 动态类型参数：既保留原始字符串，也保留解析后的数值。
 * 三个数值字段刻意不放进联合体，因为 INT_PARAM 可能希望把
 * "3.9" 直接当 3 读取，而 FLOAT_PARAM 又希望把 "12" 当 12.0 读取。 */
typedef struct __Param_T
{
    ParamType_T type;              /* 识别出的参数类型 */
    char text[CMD_PARAM_TEXT_MAX]; /* 原始字符串 */
    int32_t intValue;              /* 按整数解释 */
    uint32_t uintValue;            /* 按无符号整数解释 */
    float floatValue;              /* 按浮点数解释 */
} Param_T;

/* 在 handler 中读取参数的便捷宏：
 * INT_PARAM(argv, 1) 表示“把 argv[1] 当 int 读取” */
#define INT_PARAM(argv, index)   ((argv)[(index)].intValue)
#define UINT_PARAM(argv, index)  ((argv)[(index)].uintValue)
#define FLOAT_PARAM(argv, index) ((argv)[(index)].floatValue)
#define STR_PARAM(argv, index)   ((argv)[(index)].text)

/* 命令处理函数类型：argv[0] 为命令名，argv[1..] 为动态类型参数 */
typedef int8_t (*CmdHandler_F)(uint8_t argc, Param_T *argv);

/* 命令表项 */
typedef struct __CmdItem_T
{
    char name[CMD_NAME_MAX];  /* 命令名 */
    CmdHandler_F handler;     /* 处理函数 */
    uint8_t used;             /* 是否已被注册 */
} CmdItem_T;

/* 命令解析器结构体 */
typedef struct __CmdParser_T
{
    CmdItem_T table[CMD_TABLE_CAP]; /* 哈希表：线性探测 */
    uint8_t count;                  /* 已注册命令数 */
} CmdParser_T;

/* 初始化解析器 */
int8_t Cmd_ParserInit(CmdParser_T *pParser);

/* 注册命令 */
int8_t Cmd_ParserRegister(CmdParser_T *pParser,
                          const char *pCmdName,
                          CmdHandler_F handler);

/* 解析并执行一行命令 */
int8_t Cmd_ParserRun(CmdParser_T *pParser, const char *pLine);

/* 获取参数类型的字符串名（调试输出用） */
const char *Cmd_ParamTypeName(ParamType_T type);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_PARSER_H */
