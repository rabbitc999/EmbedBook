# STM32 高级编程培训教程 · 示例代码与文档

本仓库提供《单片机高级编程理论及实现》培训教程的完整 **LaTeX 源码** 和配套 **STM32/嵌入式 C 语言示例代码**。  
教程涵盖嵌入式 C 语言强化、任务调度、状态机、轻量级协程、串口 FIFO、面向对象设计、菜单系统、内存池、命令解析器、脚本运行、参数存储及 HardFault 调试等实战内容。

---

## 仓库结构

```text
.
├── Book/
│   ├── chapters/             # 教程各章节 LaTeX 源文件
│   ├── figure/               # 教程图片
│   ├── colorful.cls          # 文档类
│   └── book.tex              # 主文档（编译入口）
├── Chapter-embed-basic/ ~ Chapter-memory-hardfault/  # 对应章节示例代码
├── Appendix-board-bsp/       # 附录一：开发板与 BSP 框架
├── README.md                 # 本文件
├── AGENTS.md                 # 代码规范与协作指南
├── LICENSE.code              # 示例代码许可证（MIT）
├── LICENSE.docs              # 教程文档许可证（GPL-3.0）
└── .gitignore
```

> 部分代码目录带有 `Untested_` 前缀，表示尚未在作者开发板上完成验证，欢迎读者移植测试并提交反馈。

---

## 教程章节与代码对照

| 章节 | 主要内容 | 代码目录 |
|------|----------|----------|
| 第 1 章 | 嵌入式 C 语言基础 | `Chapter-embed-basic` |
| 第 2 章 | 任务调度与状态机编程 | `Chapter-task-state-machine` |
| 第 3 章 | 轻量级协作式任务调度器 | `Chapter-simple-task` |
| 第 4 章 | 串口通信与 FIFO 缓冲区 | `Chapter-serial-fifo` |
| 第 5 章 | C 语言实现面向对象特性 | `Chapter-c-oop` |
| 第 6 章 | 单片机用户界面 | `Chapter-ui-system` |
| 第 7 章 | 内存分配与内存池 | `Chapter-memory-alloc` |
| 第 8 章 | 字符串命令解析器 | `Chapter-command-parser` |
| 第 9 章 | 单片机脚本运行功能 | `Chapter-script-engine` |
| 第 10 章 | 增强版协程调度器 | `Chapter-coroutine-scheduler` |
| 第 11 章 | 用户参数存储 | `Chapter-param-store` |
| 第 12 章 | 内存拷贝、置零与 HardFault | `Chapter-memory-hardfault` |
| 第 13 章 | Linux 常见操作技巧 | `Chapter-linux` |

---

## 编译教程（PDF）

本地编译需要 **TeX Live** 并支持 **XeLaTeX** 与中文环境：

```bash
cd Book
xelatex book.tex
xelatex book.tex        # 第二次编译生成正确目录与引用
```

已编译的 `Book/book.pdf` 可直接阅读，但以仓库源码为最新版本。

---

## 开发环境

- **教程正文**：TeX Live + XeLaTeX  
- **STM32 示例**：Keil MDK、STM32CubeIDE 或 GCC ARM 工具链（部分项目提供 Makefile）  
- **PC 端示例**：`gcc` 编译 `main.c` 即可运行  
- **代码规范**：详见 `AGENTS.md`

---

## 代码规范摘要

- 注释、文档与说明使用**中文**；
- 代码文件：小写下划线命名（`my_file.c`）；
- 章节/代码目录：`Chapter-章节名称`（如 `Chapter-embed-basic/`）；
- 函数命名：`模块名_功能名`（如 `uart_send`）；
- 结构体/枚举/联合体类型：`_T` 后缀；
- 全局变量：`g_` 前缀，静态全局：`s_` 前缀；
- 指针判空必须显式：`ptr != NULL`。

完整规则请阅读 `AGENTS.md`。

---

## 解析器生成器

仓库提供单文件、Yacc 风格的非递归 LL(1) 解析器生成器：

```bash
python Chapter-script-engine/parser_gen.py \
    --grammar-file Chapter-script-engine/grammar_calc.gram \
    --report grammar_calc.report \
    -o parser_gen.c
```

`generate_parser_c(grammar, tokens, variables, token_defs)` 接收：

- `grammar`：文法规则与 `$$/$1/$2...` 语义动作；
- `tokens`：额外终结符名称列表；`STRING`、`NUMBER`、`VARIABLE` 默认存在；
- `variables`：可选的预声明动态变量；
- `token_defs`：由 `%define` 得到的固定字符串词法规则。

生成结果包含默认 Lexer、Token 枚举、预测分析表和显式分析栈，并提供
`Parser_Run()` 与 `Parser_RunText()`。可通过 `--grammar grammar.json` 或
`--grammar-file grammar.gram` 使用自定义文法；生成时会输出最终产生式、
FIRST、FOLLOW 和预测分析表。

除 JSON 外，文法和变量也可以写成 `.gram` 文本（推荐），不需要再维护 JSON：

```text
%define PLUS "+"
%define MINUS "-"
%define STAR "*"
%define SLASH "/"
%define LPAREN "("
%define RPAREN ")"
%start expr

%var X int32 = 0
%var table int32[16]
%var pid struct {
    kp float
    ki float
    out int32
}
%var grid int16[4][3]
%var config dict[int32]

expr := expr PLUS term      { $$ = $1 + $3; }
expr := expr MINUS term     { $$ = $1 - $3; }
expr := term                { $$ = $1; }

term := term STAR factor    { $$ = $1 * $3; }
term := factor              { $$ = $1; }

factor := NUMBER            { $$ = $1; }
factor := VAR               { $$ = [$1]; }
factor := LPAREN expr RPAREN { $$ = $2; }
```

规则语法约定：

- 每条产生式单独写成 `lhs := rhs { 动作 }`，不使用 `|` 和结尾 `;`；
- 允许直接左递归（`expr := expr PLUS term`），生成器自动展开为
  “基产生式 + 循环续产生式”，保持左结合；
- `%empty` 表示空产生式；
- `%var 名字 类型 [= 初始值]` 支持标量、`T[N]` 定长数组、嵌套
  `struct { ... }` 与 `dict[T]`；聚合类型还能写初始化列表：

```text
%var table int32[4] = { 1, 2, 3 }                  // 少则补默认值
%var pid struct { kp float } = { kp: 1.5 }         // 命名成员初始化
%var config dict[int32] = { mode: 1, calib: 5 }    // 键值初始化
%var team struct { id int32 }[2] = { {id: 1}, {id: 2} }
```

- 完整示例见 `Chapter-script-engine/grammar_calc.gram`：

```bash
python Chapter-script-engine/parser_gen.py \
  --grammar-file Chapter-script-engine/grammar_calc.gram -o parser_calc.c
```

高级文法生成示例（含 `[$1]` 变量引用）可直接查看：
`Chapter-script-engine/parser_advanced_gen.c`。

仓库内置两套 JSON 文法：

- `Chapter-script-engine/grammar_default.json`：四则运算/变量；
- `Chapter-script-engine/grammar_advanced.json`：IF/FOR/布尔运算。

变量使用统一的 `VAR` 终结符，词法层把变量名填入 token 的 `name` 字段；
内置变量在 JSON 的 `variables` 字段中定义：

```json
"variables": {
  "X":   { "type": "int",    "value": 0 },
  "Y":   { "type": "int",    "value": 0 },
  "Z":   { "type": "float",  "value": 1.5 },
  "ANS": { "type": "double", "value": 2.5 }
}
```

文法动作支持 Yacc 风格的 `$$ / $1 / $2 ...`：

```c
$$ = ($1 + $2) * $3 / ($1 - $2);   /* 括号与优先级自动展开为 Var_Add/Mul/Div */
$$ = int32(0);                     /* 等价于 Var_CreateInt(0)，避免手写 API */
$1.int32 = 42;                     /* 类型化读写：$1.float / $1.double 同理 */
$$ = $1.int32 ^ $2.int32;          /* 显式 int32 位运算，未显式类型会告警 */
```

`[$1]` 表示“取当前变量名对应的运行时对象”，生成器会翻译为
`Var_FindVariable(names[1])`。运算符映射关系：

- `+ - * /`：`Var_Add / Var_Sub / Var_Mul / Var_Div`；
- `&& || !`：`Var_And / Var_Or / Var_Not`；
- `int8(...)/uint8(...)/int32(...)/uint32(...)/int64(...)/uint64(...)/float(...)/double(...)`
  等类型构造；字段 `$n.int8 / $n.int32 / $n.float ...` 用于读写运行时值；
- 未显式类型的位运算（如 `$1 ^ $2`）会在生成时输出警告，建议写成
  `$1.int32 ^ $2.int32`。

生成器内置高级测试文法（IF/THEN/ELSE、FOR...IN range(...) ON、布尔运算）：

```bash
python Chapter-script-engine/parser_gen.py --advanced -o parser_adv.c
```

---

## 开源协议（双重许可）

本仓库采用**双许可证**模式，以兼顾代码复用便利性与文档自由传播：

- **示例代码**（位于 `Chapter-*/`、`Appendix-board-bsp/` 等目录下的 `.c`、`.h`、`.s`、`.ld` 等源文件）  
  → 使用 **MIT License**（详见 [`LICENSE.code`](./LICENSE.code)）。  
  允许自由使用、修改、分发，仅需保留版权声明。

- **教程正文**（位于 `Book/chapters/` 目录及 `Book/book.tex` 等 LaTeX 文档）  
  → 使用 **GNU General Public License v3.0**（详见 [`LICENSE.docs`](./LICENSE.docs)）。  
  文档衍生作品需遵守 GPL-3.0 条款。

- **其他文件**（如 `README.md`、`AGENTS.md`）属于项目说明，原则上遵循上述对应协议，或按作者另行声明。

> 请读者在复制、修改或再发布时，注意区分代码与文档的许可要求。

---

## 贡献与反馈

欢迎提交 Issue 或 Pull Request。  
对于 `Untested_` 代码，若您完成测试并确认可用，请及时反馈，我们将合并验证并移除前缀。

提交代码前请确保符合 `AGENTS.md` 中的编码规范。

---

## 致谢

感谢所有参与本教程编写、测试与改进的开发者。  
希望这份材料能帮助更多嵌入式工程师掌握高级编程技巧。
