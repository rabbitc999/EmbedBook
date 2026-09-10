# parser.py —— 单文件 Yacc 风格解析器生成器

一份 `.gram` 文法，三种生成模式，输出可直接编译的 C99 解析器：

| 模式 | 分析表 | 适用场景 |
| --- | --- | --- |
| **LL(1)**（默认） | 预测分析表 | 已消除左递归、提取左公因子的 DSL 文法，产物小、表直观 |
| **SLR(1)** | LR(0) 项集 + FOLLOW | 含直接左递归的文法（表达式优先级链等），无需改写文法 |
| **LALR(1)** | LR(1) 项集 + 同心合并 lookahead | 复用 SLR 的 LR 运行时，能处理“LR(0) 项集相同但 lookahead 更细”的文法 |

生成器本身只有 `parser.py` 一个文件（无需安装、无第三方依赖，Python 3.8+），
产物是自包含的 C 代码：内嵌 `variable_runtime`（动态变量运行时），不依赖外部库。

## 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [`.gram` 文法写法](#gram-文法写法)
- [语义动作](#语义动作)
- [冲突处理与优先级](#冲突处理与优先级)
- [词法：固定字面量 / 正则 Token / 标识符识别](#词法固定字面量--正则-token--标识符识别)
- [生成产物](#生成产物)
- [生成代码的 API](#生成代码的-api)
- [自定义分配器](#自定义分配器)
- [示例](#示例)
- [命令行选项](#命令行选项)
- [目录结构](#目录结构)
- [许可证](#许可证)

## 特性

- **三种模式共用一套前端**：词法、`.gram` 加载、`%define` / `%var` / `%tree` / `%ast_*`
  与语义动作翻译完全共用，只换分析表构造。
- **非递归执行**：LL(1) 用显式分析栈 + 语义帧；SLR/LALR 用 LR 状态栈 + 语义值栈，
  栈深由 `PARSER_MAX_STACK_DEPTH` 控制。
- **语义动作自动翻译**：`$1 + $3`、`$$ = $1`、`[$1] = $3`、`def[$1, int32] = $3`、
  `delete[$1]` 等写法会翻译成 `Var_*` 运行时调用，不用手写内存管理。
- **可选语法树（`%tree`）**：生成 CST API，三种模式的接口与节点结构完全一致
  （`{ name, text, children }`）。
- **可选 AST 元数据（`%ast_skip` / `%ast_fold` / `%ast_label`）**：给 CST→AST
  转换器查询节点去留，不改动 CST 本身。
- **优先级消解（`%left` / `%right` / `%nonassoc` / `%prec` / `%expect`）**：
  移进/归约冲突按 yacc 规则在生成期消解，未消解的冲突可用 `%expect` 显式放行。
- **正则 Token（`%regex`）与标识符识别（`%identifier`）**：正则在生成期编译成极小
  DFA（正则 → NFA → 子集构造 → 最小化），运行期只查表；标识符识别接口生成在
  `parser_token` 模块里。
- **两种产物布局**：单文件 `-o parser.c`，或 `out/` 扁平工程（10 个文件）。
- **可自定义分配器**：所有动态内存走 `parser_config.h` 里的四个宏。

## 快速开始

### 1. 单文件产物

```sh
python3 parser.py --grammar-file exmaple/calc.gram -o calc.c
gcc -std=c99 -Wall -Wextra -O2 calc.c -lm -o calc
./calc "1+2*3"        # 7
./calc "X:=3*4; X/2"  # 6
```

### 2. 分文件工程（`out/` 扁平结构）

```sh
python3 parser.py --grammar-file exmaple/calc.gram --out-dir exmaple/calc_out
cd exmaple/calc_out
gcc -std=c99 -Wall -Wextra -O2 *.c -lm -o parser_demo
./parser_demo "1+2*3"
```

`--split` 等价于 `--out-dir out`（在当前目录下生成工程）。
只把生成代码当库用时去掉 `main.c` 即可；链接时需要 `parser.c`、`parser_token.c`、
`parser_table.c`、`variable_runtime.c` 一起编译。

### 3. 最小文法

```gram
%define PLUS "+"
%start expr

expr := expr PLUS term { $$ = $1 + $3; }
expr := term           { $$ = $1; }
term := NUMBER         { $$ = $1; }
```

## `.gram` 文法写法

规则一行一条，**不用 `|`**，同名左部写多行即可；动作可写 `{ ... }`，也可省掉花括号
写到行尾。`%empty` 表示空产生式。文件里的 `//` 与 `/* */` 注释会被跳过。

| 指令 | 说明 |
| --- | --- |
| `%define NAME "字面量"` | 固定字符串 Token，允许用 `\|` 分隔多个候选：`%define EQ "==|="` |
| `%token A B C` | 声明没有固定字面量的 Token（由词法层外部产生） |
| `%start name` | 起始非终结符（缺省取第一条规则的左部） |
| `%mode ll1 \| slr \| lalr` | 生成模式，与命令行 `--mode` 必须一致 |
| `%var X int32 = 0` | 预声明变量；支持标量、数组 `int32[16]`、结构体 `struct { kp float }` |
| `%var_max_count N` | 生成代码里变量表的固定容量（缺省 32） |
| `%tree` | 生成通用语法树 API（三种模式都支持） |
| `%ast_skip  a b`<br>`%ast_fold  c`<br>`%ast_label d` | CST→AST 转换器的节点去留元数据 |
| `%left` / `%right` / `%nonassoc` | 声明 Token 的结合性与优先级，行内自左向右递增，后声明的行更高 |
| `%prec TOKEN` | 写在产生式右部末尾，覆盖该产生式的优先级来源 |
| `%expect N` | 允许保留 N 个未消解的移进/归约冲突 |
| `%regex NAME "正则"` | 用简单正则定义的 Token（生成期编译成 DFA） |
| `%identifier "正则"` | 声明标识符正则，生成标识符识别与关键字查表接口 |

## 语义动作

| 写法 | 含义 |
| --- | --- |
| `$$` | 本条产生式的返回值 |
| `$1` `$2` … | 第 n 个右部符号的返回值 |
| `[$1]` | 按变量名查表（`Var_FindVariable`），可读也可赋值：`[$1] = $3;` |
| `def[$1, int32] = $3;` | 定义变量（可带类型/维度），`delete[$1];` 删除变量 |
| `int32(0)` / `float(1.5)` | 类型构造 |
| `$1.int32` / `$1.float` | 按类型字段读写数值 |
| `max/min/abs/pow/sqrt/floor/ceil(...)` | 运行时函数，直接写在表达式里 |

算术/逻辑运算符（`+ - * / % && || 比较 位运算`）会自动改写为 `Var_*` 调用；
未对运行时值定义的运算符会在生成期给出警告。

每种模式都要求“每条产生式要么写动作，要么文法启用 `%tree`”，否则报
“规则 X 缺少语义动作；请写动作或启用 %tree”。

## 冲突处理与优先级

- **默认（不写优先级指令）**：SLR(1) / LALR(1) 遇到移进/归约、归约/归约冲突一律在
  生成期抛错，不消解、不产出 C 代码。
- **写了优先级指令后**：只消解移进/归约冲突——移进 Token 的优先级来自
  `%left/%right/%nonassoc`；归约产生式的优先级取 `%prec` 指定的 Token，没写就取
  右部最右终结符。优先级高者胜；同级时 `%left` 归约、`%right` 移进、
  `%nonassoc` 视为错误（生成期报错）。
- **归约/归约冲突**不受优先级影响，始终报错。
- **`%expect N`**：未消解的移进/归约冲突数量与 N 完全一致时放行（输出警告，并按
  移进优先写表），否则报错。
- 消解结果与未消解冲突都会写进分析报告的 `[冲突消解]` / `[未消解冲突]` 小节，
  分文件布局还会同步写进 `RESULT.md`。
- **LL(1) 不受影响**：新指令能解析但被忽略，产物与旧版一致。

```gram
%mode lalr
%left  PLUS MINUS
%left  STAR SLASH
%right UMINUS
%right POW

expr := expr PLUS expr  { $$ = $1 + $3; }
expr := expr MINUS expr { $$ = $1 - $3; }
expr := expr STAR expr  { $$ = $1 * $3; }
expr := expr SLASH expr { $$ = $1 / $3; }
expr := expr POW expr   { $$ = pow($1, $3); }
expr := MINUS expr %prec UMINUS { $$ = -$2; }
expr := NUMBER { $$ = $1; }
```

## 词法：固定字面量 / 正则 Token / 标识符识别

生成式 Lexer 的匹配顺序（长匹配优先，同长按下面的先后）：

1. 双引号字符串 `"..."`（词法层反转义，值为 `STRING`）
2. 若文法写了 `%regex`：`%regex` Token 与所有固定字面量一起做最长匹配，
   **同长度时字面量优先**（所以关键字不会被正则吃掉）；命中就返回
3. 内建数字规则（整数 / `1.2` 浮点 / `1e3` / 后缀 `u`、`d`），产出数值
4. 固定字面量（`%define` / `%token`，长的优先 + 关键字边界检查）；
   没写 `%regex` 时，所有字面量都在这一步匹配
5. 标识符：声明了 `%identifier` 就用它的 DFA 扫描，再查关键字表；否则用内建
   C 标识符规则（字母/下划线开头，字母数字下划线），映射到 `VARIABLE`

没写 `%regex` / `%identifier` 时，第 2 步不存在、第 5 步走内建规则，
产物与旧版本完全一致。

### 正则 Token

```gram
%regex FLOAT "[0-9]+[.][0-9]+"
%regex DATE  "[0-9]{4}-[0-9]{2}-[0-9]{2}"
%identifier "[A-Za-z_][A-Za-z0-9_]*"
```

支持的语法：字面量、转义、`.`、`[abc]`、`[a-z]`、`[^...]`、`(` `)`、`|`、`*`、`+`、
`?`、`{m}` `{m,}` `{m,n}`（≤32）。转义 `\* \. \[ \] \( \) \{ \} \| \^ \\ \/`
还原成字面量（写 `"\*"` 或 `"\\*"` 都可以），`\d \w \s`（大写取反）是字符集简写，
`\n \r \t \f \v \0` 是控制字符。能匹配空串的正则在生成期报错（Lexer 无法前进）。

`%regex` Token 只携带文本（`Parser_TreeGetText` / `pName`），不产生数值——要数值请
继续用内建 `NUMBER`。

### 标识符识别接口

声明 `%identifier` 后，生成的 `parser_token.h/.c` 里会有：

| 接口 | 说明 |
| --- | --- |
| `Parser_TokenIdentifierLength(text)` | 最长标识符前缀长度（走生成的 DFA） |
| `Parser_TokenIsIdentifier(text)` | 整串是否标识符 |
| `Parser_TokenKeyword(text)` | 关键字查表，非关键字返回 `TOKEN_INVALID` |
| `Parser_TokenName(type)` / `Parser_TokenText(token, buf, size)` | Token 名与文本 |
| `Parser_LexDfaMatch(dfa, text)` | 通用 DFA 最长匹配（Lexer 与上面两个函数共用） |

## 生成产物

### 单文件布局（默认）

`-o parser.c`：一个文件里包含 Token 枚举、Lexer、分析表、语义动作、运行时与演示
`main`，直接 `gcc parser.c -lm` 即可运行。

### 分文件布局（`--out-dir out`）

| 文件 | 内容 |
| --- | --- |
| `parser_config.h` | 分配器配置（`PARSER_MALLOC` / `PARSER_CALLOC` / `PARSER_REALLOC` / `PARSER_FREE`） |
| `parser.h` | 公开 API：`Parser_Run` / `Parser_RunText` / 树 / AST / 错误 |
| `parser.c` | 词法器、`ParserCtx_T`、语义动作、LL/LR 主循环、树 API、AST 元数据 |
| `parser_token.h` | Token 枚举、`ParserToken_T`、Token 名称接口、正则 DFA 与标识符接口声明 |
| `parser_token.c` | Token 名称表、文本辅助、`%regex`/`%identifier` 的 DFA 表与函数 |
| `variable_runtime.h` / `.c` | 动态变量运行时（原 `variable_runtime`） |
| `parser_table.c` | 预测分析表 / ACTION / GOTO / 产生式表 / 树元数据表 |
| `main.c` | 演示 `main`，调用 `Parser_RunText` |
| `RESULT.md` | 生成信息、产物清单、编译运行说明、冲突消解记录、文法分析报告 |

## 生成代码的 API

```c
/* 解析入口 */
VarValue_T *Parser_RunText(const char *pText);
VarValue_T *Parser_Run(const ParserToken_T *pTokens, uint32_t count);
const ParserError_T *Parser_GetLastError(void);

/* %tree 时生成 */
VarValue_T *Parser_ParseTreeText(const char *pText);
const char *Parser_TreeGetName(const VarValue_T *pNode);
const char *Parser_TreeGetText(const VarValue_T *pNode);
VarValue_T *Parser_TreeGetChildren(const VarValue_T *pNode);
void Parser_TreePrint(const VarValue_T *pRoot);
void Parser_TreeFree(VarValue_T *pRoot);

/* %ast_* 时生成 */
uint8_t Parser_AstShouldSkip(const char *pName);
uint8_t Parser_AstShouldFold(const char *pName);
uint8_t Parser_AstShouldLabel(const char *pName);
```

错误码：`PARSER_OK 0`、`PARSER_ERR_STACK -20`、`PARSER_ERR_SYNTAX -21`、
`PARSER_ERR_LEXER -22`、`PARSER_ERR_DUPLICATE -23`、`PARSER_ERR_UNDEFINED -24`、
`PARSER_ERR_SYSTEM -25`。解析失败时 `Parser_GetLastError()` 给出错误码、行列与说明。

`%tree` 的 `Parser_ParseTreeText` 只建 CST、不执行语义动作，因此可以和语义动作共存：
`Parser_RunText` 按动作求值，`Parser_ParseTreeText` 出语法树。

## 自定义分配器

运行时与解析器的所有动态内存都走 `parser_config.h` 里的四个宏（单文件布局内嵌同
一套宏），默认是 C 运行库：

```c
#define PARSER_MALLOC(size)         malloc(size)
#define PARSER_CALLOC(count, size)  calloc((count), (size))
#define PARSER_REALLOC(pointer, size) realloc((pointer), (size))
#define PARSER_FREE(pointer)        free((pointer))
```

换成静态池 / RTOS 堆 / 带统计的分配器时，推荐把实现放自己的 `.c` 里、头里只写声明：

```c
void *MyPool_Alloc(size_t size);
void  MyPool_Free(void *pointer);
#define PARSER_MALLOC(size) MyPool_Alloc((size))
#define PARSER_FREE(pointer) MyPool_Free((pointer))
```

本头文件会被多个 `.c` 包含，直接写在头里的辅助函数请用 `static inline`。

## 示例

| 文法 | 说明 | 产物目录 |
| --- | --- | --- |
| `exmaple/pl0_ll1.gram` | PL/0 的 LL(1) 写法（尾递归 + 显式消除左递归） | `exmaple/pl0_ll1_out/` |
| `exmaple/pl0.gram` | PL/0 左递归写法，`%mode lalr` + `%tree` + `%ast_*`，只出 CST | `exmaple/pl0_out/` |
| `exmaple/calc.gram` | 计算器：`+ - * / %`、括号、一元正负号、`sqrt/abs/pow`、变量 `:=` 赋值 | `exmaple/calc_out/` |
| `exmaple/calc_prec.gram` | 二义表达式文法 + `%left/%right/%prec` 优先级消解 | `exmaple/calc_prec_out/` |
| `exmaple/lex_demo.gram` | `%regex` 正则 Token + `%identifier` 标识符识别 + 关键字 | `exmaple/lex_demo_out/` |

产物目录不在版本库里（`.gitignore` 忽略），随时可以重新生成。生成任意示例：

```sh
python3 parser.py --grammar-file exmaple/calc_prec.gram --out-dir exmaple/calc_prec_out
cd exmaple/calc_prec_out && gcc -std=c99 -Wall -Wextra -Werror *.c -lm -o parser_demo
```

实跑结果（先按上面的方式在对应目录里编译出 `parser_demo`）：

```sh
cd exmaple/calc_out      && ./parser_demo "X:=3*4; X/2"           # 6
cd exmaple/calc_prec_out && ./parser_demo "-2+3"                  # 1
                            ./parser_demo "2^3^2"                 # 512（幂右结合）
cd exmaple/lex_demo_out  && ./parser_demo "begin x := 1.5 end"    # 99.5（FLOAT 正则命中）
                            ./parser_demo "begin 2024-09-10 end"  # 20240101（DATE 正则命中）
cd exmaple/pl0_out       && ./parser_demo "x := 1 + 2 * 3 ."      # program 开头的 CST
```

## 命令行选项

```
usage: parser.py [-h] [-o OUTPUT] [--out-dir OUT_DIR] [--split]
                 [--grammar-file GRAMMAR_FILE] [--report REPORT]
                 [--mode {ll1,slr,lalr}] [--dump-lr-automaton]
                 [--dump-slr-tables]

  -o, --output          单文件输出名（默认 parser.c）
  --out-dir DIR         按分文件布局输出到 DIR
  --split               等价于 --out-dir out
  --grammar-file FILE   输入 .gram 文法（默认使用内置示例文法）
  --report FILE         把文法分析报告另存为 UTF-8 文本
  --mode MODE           生成模式 ll1 / slr / lalr（与 %mode 冲突时报错）
  --dump-lr-automaton   打印 LR(0)/LALR 状态、项集与转移
  --dump-slr-tables     打印 ACTION / GOTO 表
```

## 目录结构

```
新建文件夹/
├── parser.py        单文件生成器（项目里唯一的 Python 代码）
├── pack.py          Python打包程序
├── README.md        本文件
├── LICENSE          MIT 许可证
├── .gitignore       忽略生成产物与临时文件
└── exmaple/         示例文法；生成的 <名字>_out/ 目录默认被忽略
    ├── pl0_ll1.gram
    ├── pl0.gram
    ├── calc.gram
    ├── calc_prec.gram
    └── lex_demo.gram
```

## 打包为独立可执行文件

`parser.py` 可以把自身打包成一个自包含的 C 启动器，方便在不方便带 `.py` 文件的
环境里分发。启动器内嵌 `base85(zlib(parser.py))`，运行时写临时启动器脚本并调用
系统 `python3`；参数与退出码与直接运行 `python3 parser.py ...` 一致。

```sh
# 1. 生成 C 启动器源码（不加载文法，只读自身）
python3 parser.py --emit-pack -o parser_pack.c

# 2. 编译
gcc -std=c99 -O2 -Wall -Wextra parser_pack.c -o parser_pack

# 3. 使用：等价于 python3 parser.py ...
./parser_pack --mode slr  --grammar-file exmaple/calc.gram --out-dir exmaple/calc_out
./parser_pack --mode lalr --grammar-file exmaple/pl0.gram  --out-dir exmaple/pl0_out
./parser_pack --help

## 依赖与构建

- 生成器：Python 3.8+，只用标准库。
- 产物：C99 + `libm`。建议 `gcc -std=c99 -Wall -Wextra -Werror`；
  Windows / MinGW、Linux、macOS 均可编译。

## 许可证

本项目使用 [MIT 许可证](LICENSE)。
