#include <stdio.h>
#include <string.h>
#include "cmd_parser.h"

/* ============================================================
 * 实际业务函数：它们只关心参数类型，不关心命令文本
 * ============================================================ */

/* LED 命令：LED_CMD(int n) */
static void Led_Cmd(int n)
{
    printf("LED_CMD(%d)\n", n);
}

/* 小车运动命令：Car_Run(float x, float y, float speed) */
static void Car_Run(float x, float y, float speed)
{
    printf("Car_Run(x=%.2f, y=%.2f, speed=%.2f)\n", x, y, speed);
}

/* ============================================================
 * 适配器层：把命令行参数转换成具体业务函数的实参
 * ============================================================ */

/* help */
static int8_t Cmd_HandleHelp(uint8_t argc, Param_T *argv)
{
    (void)argc;
    (void)argv;
    printf("可用命令:\n");
    printf("  help               显示帮助\n");
    printf("  led on/off         字符串参数示例\n");
    printf("  intcmd n           LED_CMD(int n)\n");
    printf("  car x y speed      Car_Run(float, float, float)\n");
    printf("  add a b            整数参数示例\n");
    printf("  types v1 v2 ...    显示参数动态类型\n");
    return CMD_ERR_OK;
}

/* 字符串参数示例：led on / led off */
static int8_t Cmd_HandleLed(uint8_t argc, Param_T *argv)
{
    if (argc < 2u) {
        printf("用法: led on|off\n");
        return CMD_ERR_EXEC;
    }
    if (strcmp(STR_PARAM(argv, 1), "on") == 0) {
        printf("LED 已打开\n");
        return CMD_ERR_OK;
    }
    if (strcmp(STR_PARAM(argv, 1), "off") == 0) {
        printf("LED 已关闭\n");
        return CMD_ERR_OK;
    }
    printf("未知状态: %s\n", STR_PARAM(argv, 1));
    return CMD_ERR_EXEC;
}

/* 整数参数示例：intcmd 7 -> LED_CMD(7) */
static int8_t Cmd_HandleIntCmd(uint8_t argc, Param_T *argv)
{
    if (argc < 2u) {
        printf("用法: intcmd n\n");
        return CMD_ERR_EXEC;
    }
    Led_Cmd(INT_PARAM(argv, 1));      /* 以 int 类型读取 argv[1] */
    return CMD_ERR_OK;
}

/* 浮点参数示例：car 1.5 2.5 0.8 -> Car_Run(...) */
static int8_t Cmd_HandleCar(uint8_t argc, Param_T *argv)
{
    if (argc < 4u) {
        printf("用法: car x y speed\n");
        return CMD_ERR_EXEC;
    }
    Car_Run(FLOAT_PARAM(argv, 1),     /* x */
            FLOAT_PARAM(argv, 2),     /* y */
            FLOAT_PARAM(argv, 3));    /* speed */
    return CMD_ERR_OK;
}

/* 整数加法：add 12 30 */
static int8_t Cmd_HandleAdd(uint8_t argc, Param_T *argv)
{
    int a = 0;
    int b = 0;

    if (argc < 3u) {
        printf("用法: add a b\n");
        return CMD_ERR_EXEC;
    }
    a = INT_PARAM(argv, 1);
    b = INT_PARAM(argv, 2);
    printf("%d + %d = %d\n", a, b, a + b);
    return CMD_ERR_OK;
}

/* 查看动态类型识别结果 */
static int8_t Cmd_HandleShowTypes(uint8_t argc, Param_T *argv)
{
    uint8_t index = 1;

    if (argc < 2u) {
        printf("用法: types v1 v2 ...\n");
        return CMD_ERR_EXEC;
    }
    for (index = 1; index < argc; index++) {
        printf("argv[%u] = %-8s type = %s\n",
               index,
               STR_PARAM(argv, index),
               Cmd_ParamTypeName(argv[index].type));
    }
    return CMD_ERR_OK;
}

int main(void)
{
    CmdParser_T parser;
    const char *testLines[] = {
        "help",
        "led on",
        "intcmd 7",
        "car 1.5 2.5 0.8",
        "car 1 2 3",            /* 整数也会自动按 float 读取 */
        "add 12 30",
        "types 12 -3 0x10 3.5 led",
        "unknown_cmd",
        "  "
    };
    uint8_t index = 0;
    int8_t ret = CMD_ERR_OK;

    Cmd_ParserInit(&parser);
    Cmd_ParserRegister(&parser, "help", Cmd_HandleHelp);
    Cmd_ParserRegister(&parser, "led", Cmd_HandleLed);
    Cmd_ParserRegister(&parser, "intcmd", Cmd_HandleIntCmd);
    Cmd_ParserRegister(&parser, "car", Cmd_HandleCar);
    Cmd_ParserRegister(&parser, "add", Cmd_HandleAdd);
    Cmd_ParserRegister(&parser, "types", Cmd_HandleShowTypes);

    for (index = 0; index < sizeof(testLines) / sizeof(testLines[0]); index++) {
        printf(">>> %s\n", testLines[index]);
        ret = Cmd_ParserRun(&parser, testLines[index]);
        if (ret == CMD_ERR_NOT_FOUND) {
            printf("未知命令\n");
        } else if (ret == CMD_ERR_EMPTY) {
            printf("空命令\n");
        }
    }
    return 0;
}
