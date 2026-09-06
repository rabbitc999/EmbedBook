#ifndef __EXPR_PARSER_H
#define __EXPR_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 配置常量 */
#define EXPR_VAR_MAX         16u
#define EXPR_NAME_MAX        16u
#define EXPR_MAX_PARSE_DEPTH 64u

/* 错误码 */
#define EXPR_ERR_OK            0
#define EXPR_ERR_NULL         -1
#define EXPR_ERR_SYNTAX       -2
#define EXPR_ERR_UNKNOWN_VAR  -3
#define EXPR_ERR_DIV_ZERO     -4
#define EXPR_ERR_DEPTH_LIMIT  -5
#define EXPR_ERR_VAR_FULL     -6

/* 脚本变量 */
typedef struct __ExprVar_T
{
    char name[EXPR_NAME_MAX]; /* 变量名 */
    int32_t value;            /* 变量值 */
    uint8_t used;             /* 是否已使用 */
} ExprVar_T;

/* 脚本运行上下文 */
typedef struct __ExprContext_T
{
    ExprVar_T vars[EXPR_VAR_MAX];
    uint16_t varCount;
} ExprContext_T;

/* 设置变量值 */
int8_t Expr_SetVariable(ExprContext_T *pCtx,
                        const char *pName,
                        int32_t value);

/* 获取变量值 */
int8_t Expr_GetVariable(const ExprContext_T *pCtx,
                        const char *pName,
                        int32_t *pValue);

/* 解析并计算表达式 */
int8_t Expr_Evaluate(ExprContext_T *pCtx,
                     const char *pText,
                     int32_t *pResult);

#ifdef __cplusplus
}
#endif

#endif /* __EXPR_PARSER_H */
