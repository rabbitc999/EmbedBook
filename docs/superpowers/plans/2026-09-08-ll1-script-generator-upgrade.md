# LL(1) 脚本生成器升级 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把现有生成器升级为单文件、带默认 Lexer、非递归预测分析、动态数字、固定类型变量和文法分析报告的 MCU 友好型 LL(1) 工具。

**Architecture:** `parser_gen.py` 统一负责 `.gram`/JSON 加载、直接左递归转换、集合与预测表计算、报告和 C 源码生成。生成的 C 代码以显式分析栈执行预测分析，Lexer 逐字符产生动态 `VarValue_T`；`variable_runtime.c/h` 提供严格 Getter、固定类型转换和变量注册表接口。

**Tech Stack:** Python 3 标准库、C99、GCC、LaTeX 文档。

**Spec:** `docs/superpowers/specs/2026-09-08-ll1-script-generator-upgrade-design.md`

## Global Constraints

- 所有新增 Python 文档字符串、C 文档注释、段落注释和 Markdown 备注均使用中文。
- C 函数遵循“模块名_功能名”，全局变量前缀和类型后缀遵循仓库 `AGENTS.md`。
- 所有指针判断显式写为 `!= NULL` 或 `== NULL`。
- 不引入第三方 Python 包或 C 依赖。
- 正式测试通过 `parser_gen.py --self-test` 与现有 `variable_runtime_test.c` 承载，不新增遗留临时测试文件。
- 测试产生的 C 文件、报告、日志与可执行文件必须位于临时目录，并在进程结束时清理。

---

### Task 1: 合并文法加载器并扩展元语法

**Files:**
- Modify: `Chapter-script-engine/parser_gen.py`
- Delete: `Chapter-script-engine/grammar_loader.py`

**Interfaces:**
- Produces: `load_grammar_text(text)`、`load_grammar_file(path)`，返回含 `tokens`、`token_defs`、`grammar`、`variables` 的字典。
- Produces: 默认 Token 顺序 `STRING, NUMBER, VARIABLE`。

- [ ] **Step 1: 在 `--self-test` 中加入失败断言**

```python
cfg = load_grammar_text('%define EQ "==|="\n%start root\nroot := NUMBER { $$ = $1; }')
assert cfg["tokens"][:3] == ["STRING", "NUMBER", "VARIABLE"]
assert cfg["token_defs"]["EQ"] == ["==", "="]
```

- [ ] **Step 2: 运行 RED 测试**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: FAIL，提示 `--self-test` 或 `load_grammar_text` 尚未集成。

- [ ] **Step 3: 把 `_GramTokenizer`、`_GramParser` 和加载函数移入 `parser_gen.py`**

实现 `%define TOKEN "分支|分支"`，正确处理 `\|`，拒绝空分支、重复字面量和跨 Token 冲突；默认注入三个 Token。

- [ ] **Step 4: 运行单文件加载器测试并删除旧文件**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: 元语法用例 PASS；直接执行生成命令不再 import `grammar_loader`。

### Task 2: 最终文法、FIRST/FOLLOW 与预测表报告

**Files:**
- Modify: `Chapter-script-engine/parser_gen.py`

**Interfaces:**
- Produces: `prepare_grammar(grammar, tokens)` 返回最终规则、终结符、FIRST、FOLLOW 和预测表。
- Produces: `format_grammar_report(analysis)` 返回稳定顺序 UTF-8 文本。

- [ ] **Step 1: 添加报告自测**

```python
report = format_grammar_report(prepare_grammar(cfg["grammar"], cfg["tokens"]))
assert "[最终产生式]" in report
assert "FIRST(root) = { NUMBER }" in report
assert "FOLLOW(root) = { END }" in report
assert "M[root, NUMBER]" in report
```

- [ ] **Step 2: 运行测试确认报告接口缺失**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: FAIL 于报告接口。

- [ ] **Step 3: 实现直接左递归转换及稳定报告**

最终文法使用合成尾非终结符，保留原产生式动作元数据；间接左递归和预测表冲突给出涉及的两条产生式。CLI 默认打印报告，`--report PATH` 写相同文本。

- [ ] **Step 4: 验证报告内容与文件一致**

Run: `python3 Chapter-script-engine/parser_gen.py --grammar-file Chapter-script-engine/grammar_calc.gram -o /tmp/parser_calc.c --report /tmp/parser_calc.report`
Expected: 终端包含最终产生式、FIRST、FOLLOW 和预测表，报告文件内容一致。验证后删除两个 `/tmp/parser_calc.*` 文件。

### Task 3: 严格 Getter 与固定类型转换

**Files:**
- Modify: `Chapter-script-engine/variable_runtime.h`
- Modify: `Chapter-script-engine/variable_runtime.c`
- Modify: `Chapter-script-engine/variable_runtime_test.c`

**Interfaces:**
- Produces: `Var_GetInt8/Uint8/Int16/Uint16/Int32/Uint32/Int64/Uint64/Float/Double/Char/String`。
- Produces: `Var_ConvertValue(const VarValue_T *pSource, const VarValue_T *pTypeTemplate, VarValue_T **ppResult)`。
- Produces: `VAR_RUNTIME_ERR_RANGE`。

- [ ] **Step 1: 扩展现有 C 测试**

加入 `300 -> uint8` 返回范围错误且输出参数保持原值、`1.9f -> int32` 得到 `1`、字符串转数字返回类型错误、数组转换失败不改变目标数组等断言。

- [ ] **Step 2: 编译运行确认 RED**

Run: `gcc -std=c99 -Wall -Wextra -Werror Chapter-script-engine/variable_runtime.c Chapter-script-engine/variable_runtime_test.c -IChapter-script-engine -o /tmp/variable_runtime_test && /tmp/variable_runtime_test`
Expected: 编译失败，提示新 Getter 尚未声明。

- [ ] **Step 3: 实现公共转换内核与 Getter**

使用 `<limits.h>`、`<float.h>`、`<math.h>` 做范围和有限值检查。所有转换先写局部临时变量，成功后才写输出参数；数组转换先构造完整副本再替换。

- [ ] **Step 4: 编译运行确认通过并清理产物**

Run: `gcc -std=c99 -Wall -Wextra -Werror Chapter-script-engine/variable_runtime.c Chapter-script-engine/variable_runtime_test.c -IChapter-script-engine -lm -o /tmp/variable_runtime_test && /tmp/variable_runtime_test`
Expected: 全部 `[PASS]`，退出码 0；随后删除 `/tmp/variable_runtime_test`。

### Task 4: 变量注册表的定义与固定类型赋值

**Files:**
- Modify: `Chapter-script-engine/variable_runtime.h`
- Modify: `Chapter-script-engine/variable_runtime.c`
- Modify: `Chapter-script-engine/variable_runtime_test.c`

**Interfaces:**
- Produces: `Var_DefineVariable(const char *pName, const VarValue_T *pValue, const VarValue_T *pTypeTemplate)`。
- Produces: `Var_AssignVariable(const char *pName, const VarValue_T *pValue)`。
- Consumes: `Var_FindVariable()` 和 Task 3 的 `Var_ConvertValue()`。

- [ ] **Step 1: 添加定义、重复定义、未定义赋值和类型固定测试**
- [ ] **Step 2: 运行现有 C 测试确认 RED**
- [ ] **Step 3: 实现事务式定义与赋值**

`pTypeTemplate == NULL` 时定义类型来自值；否则仅允许简单类型或数组模板。赋值先转换成目标模板再调用 `Var_SetValue()`，失败保持目标不变。

- [ ] **Step 4: 运行 C 测试并清理可执行文件**

Run 与 Expected 同 Task 3 Step 4。

### Task 5: 生成默认 Lexer 与动态 Token

**Files:**
- Modify: `Chapter-script-engine/parser_gen.py`

**Interfaces:**
- Produces: 生成代码中的 `Parser_LexNext()`、`Parser_RunText()` 和 `Parser_GetLastError()`。
- Changes: `ParserToken_T.value` 从 `int32_t` 改为 `VarValue_T *`，`VARIABLE` 的名称单独保存。

- [ ] **Step 1: 添加生成代码字符串与 GCC 集成自测**

脚本覆盖 `1`、`1u`、`1.2`、`1e3`、`1.2d`、字符串、变量和 `==|=` 最长匹配；检查各值的 `VarType_T`。

- [ ] **Step 2: 运行 `--self-test` 确认 RED**
- [ ] **Step 3: 生成逐字符 Lexer**

Lexer 不分配完整 Token 数组，而是按需产生当前 Token；数字用 `strtol`、`strtoul`、`strtof`、`strtod` 并检查 `errno` 和尾部，字符串反转义到受控缓冲区，运算符表按长度降序生成。

- [ ] **Step 4: 运行自测确认 Lexer 与错误位置通过**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: Python 与生成 C 的 Lexer 用例全部 PASS，临时目录自动清理。

### Task 6: 非递归预测分析核心与语义动作

**Files:**
- Modify: `Chapter-script-engine/parser_gen.py`

**Interfaces:**
- Produces: 生成代码中的 `Parser_ParseLoop()`，只使用显式分析栈和语义帧栈。
- Consumes: Task 2 的预测表、Task 4 的变量接口和 Task 5 的 Token 流。

- [ ] **Step 1: 添加深层输入和“无 Parse_xxx 递归函数”测试**

```python
assert "static ParseResult_T Parse_" not in generated_code
assert "Parser_ParseLoop" in generated_code
```

集成程序解析数千个同层操作符，验证结果正确且不依赖 C 调用栈。

- [ ] **Step 2: 运行自测确认旧递归生成结构失败**
- [ ] **Step 3: 生成符号栈、语义帧、动作标记与预测表**

终结符匹配后把 Token 值和名称写入所属动作帧槽位；非终结符按预测表展开；动作标记执行翻译后的动作并把 `$$` 回填父帧。直接左递归转换产生的尾规则使用累计值槽保持左结合。

- [ ] **Step 4: 扩展动作翻译器支持变量定义与赋值**

把 `def[$1] = $3`、`def[$1, int32[8]] = $3`、`def[$1, $2] = $3` 和 `[$1] = $3` 分别映射到运行时定义、模板构建与赋值接口；运行错误写入最近错误结构。

- [ ] **Step 5: 运行全量自测**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: 默认 Lexer、报告、非递归、左递归兼容和变量动作全部 PASS。

### Task 7: 示例文法、教材与最终验证

**Files:**
- Modify: `Chapter-script-engine/grammar_calc.gram`
- Modify: `Book/chapters/chapter-script-engine.tex`
- Modify: `Book/chapters/appendix-parser-generator-runtime.tex`
- Modify: `Chapter-script-engine/parser_gen.py`

**Interfaces:**
- Produces: 可直接演示 `%define`、默认 Token、`def` 和赋值的示例。

- [ ] **Step 1: 更新示例文法**

删除默认 Token 的冗余声明，增加 `%define` 运算符和变量定义/赋值产生式，保留直接左递归案例。

- [ ] **Step 2: 更新正文与附录**

把“两文件递归下降生成器”改为“单文件非递归预测分析生成器”，补充数字类型表、`%define` 限制、变量动作、Getter、报告命令和错误行为。

- [ ] **Step 3: 运行最终自动化验证**

Run: `python3 Chapter-script-engine/parser_gen.py --self-test`
Expected: 所有自测和生成 C 集成测试通过。

Run: `gcc -std=c99 -Wall -Wextra -Werror Chapter-script-engine/variable_runtime.c Chapter-script-engine/variable_runtime_test.c -IChapter-script-engine -lm -o /tmp/variable_runtime_test && /tmp/variable_runtime_test`
Expected: 全部运行时测试通过。

Run: `git diff --check`
Expected: 无空白错误。

- [ ] **Step 4: 清理检查**

Run: `find Chapter-script-engine -maxdepth 2 -type f \( -name '*.tmp' -o -name '*.log' -o -name '*.exe' -o -name '*.pyc' \) -print`
Expected: 不存在本次生成的临时文件；删除本次运行产生的 `/tmp/variable_runtime_test`。
