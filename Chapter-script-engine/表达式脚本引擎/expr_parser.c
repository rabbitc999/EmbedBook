/**
 * @file    expr_parser.c
 * @brief   递归下降表达式解析器（支持变量、四则运算与括号）
 * @details 文法：
 *          expr   := term (('+'|'-') term)*
 *          term   := factor (('*'|'/') factor)*
 *          factor := number | ident | '(' expr ')' | ('-'|'+') factor
 *          解析器即时求值，并用递归深度限制防止脚本把栈耗尽。
 */

#include <string.h>
#include <stdlib.h>
#include "expr_parser.h"

/* 数值文本最大长度 */
#define EXPR_NUMBER_TEXT_MAX 24u

/* Token 类型 */
typedef enum __ExprTokenKind_T
{
    EXPR_TOKEN_NONE = 0,
    EXPR_TOKEN_NUMBER,
    EXPR_TOKEN_IDENT,
    EXPR_TOKEN_PLUS,
    EXPR_TOKEN_MINUS,
    EXPR_TOKEN_STAR,
    EXPR_TOKEN_SLASH,
    EXPR_TOKEN_LPAREN,
    EXPR_TOKEN_RPAREN,
    EXPR_TOKEN_END
} ExprTokenKind_T;

/* 扫描器 */
typedef struct __ExprScanner_T
{
    const char *pText;                    /* 原始文本 */
    ExprTokenKind_T token;                /* 当前 lookahead token */
    char tokenText[EXPR_NUMBER_TEXT_MAX]; /* 数字或变量名文本 */
    uint16_t depth;                       /* 当前递归深度 */
} ExprScanner_T;

/* 向前读取一个 token */
static int8_t Expr_ScanNext(ExprScanner_T *pScanner)
{
    const char *p = pScanner->pText;
    uint16_t length = 0;

    /* 跳过空白字符 */
    while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) {
        p++;
    }

    pScanner->pText = p;
    pScanner->tokenText[0] = '\0';

    if (*p == '\0') {
        pScanner->token = EXPR_TOKEN_END;
        return EXPR_ERR_OK;
    }

    /* 数字 */
    if ((*p >= '0') && (*p <= '9')) {
        while ((*p >= '0') && (*p <= '9')) {
            if (length < (EXPR_NUMBER_TEXT_MAX - 1u)) {
                pScanner->tokenText[length++] = *p;
            }
            p++;
        }
        pScanner->tokenText[length] = '\0';
        pScanner->token = EXPR_TOKEN_NUMBER;
        pScanner->pText = p;
        return EXPR_ERR_OK;
    }

    /* 变量名 */
    if (((*p >= 'a') && (*p <= 'z')) ||
        ((*p >= 'A') && (*p <= 'Z')) || (*p == '_')) {
        while (((*p >= 'a') && (*p <= 'z')) ||
               ((*p >= 'A') && (*p <= 'Z')) ||
               ((*p >= '0') && (*p <= '9')) || (*p == '_')) {
            if (length < (EXPR_NAME_MAX - 1u)) {
                pScanner->tokenText[length++] = *p;
            }
            p++;
        }
        pScanner->tokenText[length] = '\0';
        pScanner->token = EXPR_TOKEN_IDENT;
        pScanner->pText = p;
        return EXPR_ERR_OK;
    }

    /* 单字符运算符 */
    pScanner->pText = p + 1;
    switch (*p) {
        case '+':
            pScanner->token = EXPR_TOKEN_PLUS;
            return EXPR_ERR_OK;
        case '-':
            pScanner->token = EXPR_TOKEN_MINUS;
            return EXPR_ERR_OK;
        case '*':
            pScanner->token = EXPR_TOKEN_STAR;
            return EXPR_ERR_OK;
        case '/':
            pScanner->token = EXPR_TOKEN_SLASH;
            return EXPR_ERR_OK;
        case '(':
            pScanner->token = EXPR_TOKEN_LPAREN;
            return EXPR_ERR_OK;
        case ')':
            pScanner->token = EXPR_TOKEN_RPAREN;
            return EXPR_ERR_OK;
        default:
            pScanner->token = EXPR_TOKEN_NONE;
            return EXPR_ERR_SYNTAX;
    }
}

/* 查找变量 */
static ExprVar_T *Expr_FindVariable(ExprContext_T *pCtx, const char *pName)
{
    uint16_t index = 0;

    for (index = 0; index < pCtx->varCount; index++) {
        if ((pCtx->vars[index].used != 0u) &&
            (strcmp(pCtx->vars[index].name, pName) == 0)) {
            return &pCtx->vars[index];
        }
    }
    return NULL;
}

/* 前向声明 */
static int8_t Expr_ParseExpr(ExprContext_T *pCtx,
                             ExprScanner_T *pScanner,
                             int32_t *pValue);

/* factor := number | ident | '(' expr ')' | ('-'|'+') factor */
static int8_t Expr_ParseFactor(ExprContext_T *pCtx,
                               ExprScanner_T *pScanner,
                               int32_t *pValue)
{
    int8_t ret = EXPR_ERR_OK;
    ExprVar_T *pVar = NULL;

    if (pScanner->depth >= EXPR_MAX_PARSE_DEPTH) {
        return EXPR_ERR_DEPTH_LIMIT;
    }
    pScanner->depth++;

    switch (pScanner->token) {
        case EXPR_TOKEN_NUMBER:
            *pValue = (int32_t)strtol(pScanner->tokenText, NULL, 10);
            ret = Expr_ScanNext(pScanner);
            break;

        case EXPR_TOKEN_IDENT:
            pVar = Expr_FindVariable(pCtx, pScanner->tokenText);
            if (pVar == NULL) {
                ret = EXPR_ERR_UNKNOWN_VAR;
                break;
            }
            *pValue = pVar->value;
            ret = Expr_ScanNext(pScanner);
            break;

        case EXPR_TOKEN_MINUS:
            ret = Expr_ScanNext(pScanner);
            if (ret == EXPR_ERR_OK) {
                ret = Expr_ParseFactor(pCtx, pScanner, pValue);
                *pValue = -*pValue;
            }
            break;

        case EXPR_TOKEN_PLUS:
            ret = Expr_ScanNext(pScanner);
            if (ret == EXPR_ERR_OK) {
                ret = Expr_ParseFactor(pCtx, pScanner, pValue);
            }
            break;

        case EXPR_TOKEN_LPAREN:
            ret = Expr_ScanNext(pScanner);
            if (ret == EXPR_ERR_OK) {
                ret = Expr_ParseExpr(pCtx, pScanner, pValue);
            }
            if ((ret == EXPR_ERR_OK) &&
                (pScanner->token != EXPR_TOKEN_RPAREN)) {
                ret = EXPR_ERR_SYNTAX;
            }
            if (ret == EXPR_ERR_OK) {
                ret = Expr_ScanNext(pScanner);
            }
            break;

        default:
            ret = EXPR_ERR_SYNTAX;
            break;
    }

    pScanner->depth--;
    return ret;
}

/* term := factor (('*'|'/') factor)* */
static int8_t Expr_ParseTerm(ExprContext_T *pCtx,
                             ExprScanner_T *pScanner,
                             int32_t *pValue)
{
    int8_t ret = Expr_ParseFactor(pCtx, pScanner, pValue);
    ExprTokenKind_T op = EXPR_TOKEN_NONE;
    int32_t right = 0;

    while ((ret == EXPR_ERR_OK) &&
           ((pScanner->token == EXPR_TOKEN_STAR) ||
            (pScanner->token == EXPR_TOKEN_SLASH))) {
        op = pScanner->token;
        ret = Expr_ScanNext(pScanner);
        if (ret != EXPR_ERR_OK) {
            break;
        }
        ret = Expr_ParseFactor(pCtx, pScanner, &right);
        if (ret != EXPR_ERR_OK) {
            break;
        }
        if (op == EXPR_TOKEN_STAR) {
            *pValue = *pValue * right;
        } else {
            if (right == 0) {
                return EXPR_ERR_DIV_ZERO;
            }
            *pValue = *pValue / right;
        }
    }
    return ret;
}

/* expr := term (('+'|'-') term)* */
static int8_t Expr_ParseExpr(ExprContext_T *pCtx,
                             ExprScanner_T *pScanner,
                             int32_t *pValue)
{
    int8_t ret = Expr_ParseTerm(pCtx, pScanner, pValue);
    ExprTokenKind_T op = EXPR_TOKEN_NONE;
    int32_t right = 0;

    while ((ret == EXPR_ERR_OK) &&
           ((pScanner->token == EXPR_TOKEN_PLUS) ||
            (pScanner->token == EXPR_TOKEN_MINUS))) {
        op = pScanner->token;
        ret = Expr_ScanNext(pScanner);
        if (ret != EXPR_ERR_OK) {
            break;
        }
        ret = Expr_ParseTerm(pCtx, pScanner, &right);
        if (ret != EXPR_ERR_OK) {
            break;
        }
        if (op == EXPR_TOKEN_PLUS) {
            *pValue = *pValue + right;
        } else {
            *pValue = *pValue - right;
        }
    }
    return ret;
}

/* 设置变量 */
int8_t Expr_SetVariable(ExprContext_T *pCtx,
                        const char *pName,
                        int32_t value)
{
    ExprVar_T *pVar = NULL;

    if ((pCtx == NULL) || (pName == NULL)) {
        return EXPR_ERR_NULL;
    }
    if (strlen(pName) >= EXPR_NAME_MAX) {
        return EXPR_ERR_SYNTAX;
    }

    pVar = Expr_FindVariable(pCtx, pName);
    if (pVar != NULL) {
        pVar->value = value;
        return EXPR_ERR_OK;
    }
    if (pCtx->varCount >= EXPR_VAR_MAX) {
        return EXPR_ERR_VAR_FULL;
    }

    pVar = &pCtx->vars[pCtx->varCount];
    strncpy(pVar->name, pName, EXPR_NAME_MAX - 1u);
    pVar->name[EXPR_NAME_MAX - 1u] = '\0';
    pVar->value = value;
    pVar->used = 1u;
    pCtx->varCount++;
    return EXPR_ERR_OK;
}

/* 获取变量 */
int8_t Expr_GetVariable(const ExprContext_T *pCtx,
                        const char *pName,
                        int32_t *pValue)
{
    ExprVar_T *pVar = NULL;

    if ((pCtx == NULL) || (pName == NULL) || (pValue == NULL)) {
        return EXPR_ERR_NULL;
    }
    pVar = Expr_FindVariable((ExprContext_T *)pCtx, pName);
    if (pVar == NULL) {
        return EXPR_ERR_UNKNOWN_VAR;
    }
    *pValue = pVar->value;
    return EXPR_ERR_OK;
}

/* 解析并计算表达式 */
int8_t Expr_Evaluate(ExprContext_T *pCtx,
                     const char *pText,
                     int32_t *pResult)
{
    ExprScanner_T scanner;
    int8_t ret = EXPR_ERR_OK;

    if ((pCtx == NULL) || (pText == NULL) || (pResult == NULL)) {
        return EXPR_ERR_NULL;
    }

    memset(&scanner, 0, sizeof(scanner));
    scanner.pText = pText;

    ret = Expr_ScanNext(&scanner);
    if (ret != EXPR_ERR_OK) {
        return ret;
    }
    ret = Expr_ParseExpr(pCtx, &scanner, pResult);
    if (ret != EXPR_ERR_OK) {
        return ret;
    }
    if (scanner.token != EXPR_TOKEN_END) {
        return EXPR_ERR_SYNTAX;
    }
    return EXPR_ERR_OK;
}
