#include <stdio.h>
#include <string.h>
#include "expr_parser.h"

static const char *Expr_ErrorString(int8_t err)
{
    switch (err) {
        case EXPR_ERR_OK:           return "OK";
        case EXPR_ERR_SYNTAX:       return "语法错误";
        case EXPR_ERR_UNKNOWN_VAR:  return "未知变量";
        case EXPR_ERR_DIV_ZERO:     return "除数为零";
        case EXPR_ERR_DEPTH_LIMIT:  return "嵌套过深";
        case EXPR_ERR_VAR_FULL:     return "变量表已满";
        default:                    return "其他错误";
    }
}

int main(void)
{
    ExprContext_T ctx;
    const char *exprList[] = {
        "10 + 2 * (3 + x)",
        "(a + b) * 2",
        "100 / 4 - 3 * 2",
        "1 + (2 + (3 + (4 + (5 + (6 + (7))))))",
        "10 / 0",
        "x + y"
    };
    uint8_t index = 0;
    int32_t result = 0;
    int8_t ret = EXPR_ERR_OK;

    memset(&ctx, 0, sizeof(ctx));
    Expr_SetVariable(&ctx, "x", 10);
    Expr_SetVariable(&ctx, "y", 3);
    Expr_SetVariable(&ctx, "a", 7);
    Expr_SetVariable(&ctx, "b", 5);

    for (index = 0; index < sizeof(exprList) / sizeof(exprList[0]); index++) {
        result = 0;
        ret = Expr_Evaluate(&ctx, exprList[index], &result);
        if (ret == EXPR_ERR_OK) {
            printf("%-28s = %d\n", exprList[index], result);
        } else {
            printf("%-28s -> %s\n", exprList[index], Expr_ErrorString(ret));
        }
    }
    return 0;
}
