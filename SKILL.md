---
name: converting-ll1-grammar
description: Use when a user provides a grammar, informal syntax description, EBNF, BNF, JSON, or examples and wants a standard LL(1) grammar file for this project's parser generator.
---

# 转换为 LL(1) 文法

## 目标

把用户给定的格式转换为本项目 `Chapter-script-engine/parser_gen.py` 可读取的 `.gram` 文件。输出必须是完整文件内容，而不是只讲转换思路。

## 转换规则

1. 保留用户语言的语义、优先级、结合性及语义动作。信息不足时采用最小合理假设，并在输出后的“转换说明”中列出。
2. 默认终结符 `STRING`、`NUMBER`、`VARIABLE` 不需要用 `%token` 重复声明。其他终结符用 `%define TOKEN "字面量"` 声明；多个固定字面量仅用 `|` 连接，例如 `%define EQ "==|="`。不要把 `%define` 写成正则表达式。
3. 用 `%start nonterminal` 指定唯一入口。每个候选式独占一行，格式为 `lhs := rhs { action }`；空产生式写作 `%empty`，不要使用同一行的 YACC 式候选符。
4. 消除直接和间接左递归，并提取导致 FIRST 集合相交的左公因子。为新非终结符使用能表达用途的名称，例如 `expr_tail`。
5. 计算 FIRST、FOLLOW 和 SELECT 集合，确保预测表的每个单元最多对应一条产生式。若文法无法保持原语义并转换为 LL(1)，停止生成文件，准确指出冲突产生式和冲突 Token。
6. 语义动作沿用 `$$`、`$1`、`$2`。改写产生式后同步调整参数序号；需要保存左结合累积值时，通过尾部非终结符的继承或项目生成器支持的等价动作表达，不得悄悄改变计算顺序。
7. 变量定义使用 `def[$1]`、`def[$1, int32]`、`def[$1, int32[4]]` 或 `def[$1, $2]`；已有变量赋值使用 `[$1] = value`。变量一经定义，后续赋值只改变数值，不改变类型。

## 输出约定

先给出建议文件名，然后输出一个且仅一个可直接保存的 `text` 代码块：

```text
%define PLUS "+"
%start expr

expr := term expr_tail { $$ = $1 + $2; }
expr_tail := PLUS term expr_tail { $$ = $2 + $3; }
expr_tail := %empty { $$ = int32(0); }
term := NUMBER { $$ = $1; }
```

代码块之后简要列出：

- 为完成转换所作的假设；
- 消除左递归或提取左公因子的结果；
- FIRST/FOLLOW 冲突检查结论。

用户明确要求写入项目时，将文件保存到用户指定路径；未指定路径时保存为项目根目录下语义明确的 `*.gram` 文件。写入后调用 `parser_gen.py --grammar-file` 验证文法；若用户只要求输出文本，则不要擅自写文件。

## 常见错误

- 把词法正则放进 `%define`；该指令只接受固定字符串和 `|` 分支。
- 仅改写产生式而没有同步修改 `$n` 序号与语义动作。
- FIRST/FIRST 无冲突便宣称是 LL(1)，却遗漏可空产生式的 FIRST/FOLLOW 冲突。
- 为了通过 LL(1) 检查而改变运算符优先级、结合性或变量类型规则。
