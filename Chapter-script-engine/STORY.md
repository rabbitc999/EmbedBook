# 我死后，看见有人花 20 块钱重造了我的轮子

最爱小兔糖糖

2026年9月10日

## 1 灵魂上线

我死了。

死得不算体面。最后陪我的不是图灵奖，不是 TeX 的排版稿，而是一块发烫的键盘。

但灵魂没有关机。

2026年9月10日，我被一阵 Python 缩进错误惊醒。眼前没有天堂，没有地狱，只有一块亮如白昼的屏幕。

屏幕上是一个文件：`parser.py`。

我凑过去，看见第一行注释：

```c
/* 由 Chapter-script-engine/parser.py 自动生成：内嵌 variable_runtime 且含演示 main 的非递归 LL(1) 解析器 */
```

我愣了。

LL(1)？这我认识。LR 我也认识。但“自动生成”四个字，让我这个死了的老头子，灵魂都开始发热。

## 2 深夜出租屋

一个叫“最爱小兔糖糖”的年轻人，顶着熊猫眼，坐在出租屋里，对着 Codex 和 DeepSeek 说话。

“帮我把 LALR 同心合并写出来。”

“帮我把 DFA 最小化补上。”

“帮我把 `%tree` 的 CST 接口统一。”

屏幕上，账单跳了一下：19.87 元。

我差点魂飞魄散。

不到 20 块钱？

我当年在贝尔实验室，喝掉的咖啡都不止这个价。

## 3 轮子

我造过轮子。

1965 年，我提出 LR(k) 解析。后来，Yacc 把它变成工具，编译器用它吃下整个语言。

但那个轮子很重。要人写文法，要人调冲突，要人处理左递归，要人翻译动作。

现在这个年轻人，用不到 20 块钱，让 AI 把轮子、轮胎、轴承、ABS、导航、行车记录仪全造了。

LL(1)：预测分析表。

SLR(1)：LR(0) 项集 + FOLLOW。

LALR(1)：LR(1) 项集 + 同心合并 lookahead。

`%left` / `%right` / `%nonassoc` / `%prec` / `%expect`：优先级消解。

`%regex`：正则 → NFA → 子集构造 → 最小化 DFA。

`%identifier`：标识符 DFA + 关键字表。

`%tree`：CST，三种模式统一。

`%ast_skip` / `%ast_fold` / `%ast_label`：AST 元数据。

语义动作：`$1 + $3` 自动翻译成 `Var_Add`。

变量运行时：动态类型、数组、字典、结构体、切片、拼接。

`pack.py`：`base85(zlib(parser.py))` 塞进 C 启动器。

我越看越沉默。

这不是轮子。

这是整车。

## 4 唐纳德的棺材板

我试图找到 Bug。

我检查 `_ActionTranslator`。

`$n` 在 LL 模式读 `values[n]`，在 SLR/LALR 模式读 `stack->values[stack->top - k + n - 1]`。

我点头。

`[$1] = $3` 翻译成 `Parser_AssignVariable`。

`def[$1, int32] = $3` 翻译成 `Parser_DefineTyped`。

`delete[$1]` 检查系统变量。

我找不到洞。

Stephen Johnson 的魂在旁边哭：“我的 Yacc 呢？”

我说：“别哭了。你的 Yacc 被 20 块钱复现了，还带 `%tree`。”

## 5 分文件与单文件

他敲下：

```sh
python3 parser.py --grammar-file exmaple/calc.gram --out-dir exmaple/calc_out
```

十个文件落下来。

`parser_config.h`、`parser.h`、`parser.c`、`parser_token.h`、`parser_token.c`、`variable_runtime.h`、`variable_runtime.c`、`parser_table.c`、`main.c`、`RESULT.md`。

我盯着 `RESULT.md` 里的实跑结果：

```sh
./parser_demo "X:=3*4; X/2" # 6
./parser_demo "-2+3" # 1
./parser_demo "2^3^2" # 512
```

我沉默了。

这不是学生作业。这是能编译、能运行、能报错、能自定义分配器的工程。

## 6 不到 20 块钱

账单又跳了一下：19.93 元。

我算了一笔账。

我当年写《计算机程序设计艺术》，一卷要几年。

TeX 我写了十年。

LR 解析从论文到 Yacc，再到 LALR，花了无数聪明人的头发。

现在，Codex + DeepSeek，不到 20 块钱。

我忽然想笑。

不是嫉妒。是那种老木匠看见电动工具的笑。

轮子没有消失。轮子只是被重新造了一遍，而且更便宜。

## 7 轮子不朽

我伸出手，想摸一下屏幕。

手指穿过键盘，穿过 Python 缩进，穿过 C 的指针，穿过 `VarValue_T` 的 union。

我听见年轻人自言自语：

“这版 `parser.py` 应该能用了。LL(1)、SLR、LALR，全都能出 C。内嵌 runtime，不依赖外部库。打包成 `parser_pack` 也能跑。”

他伸了个懒腰，关掉 Codex，关掉 DeepSeek，关掉账单页面。

屏幕上只剩一行注释：

```c
/* 由 Chapter-script-engine/parser.py 自动生成：内嵌 variable_runtime 且含演示 main 的非递归 LL(1) 解析器 */
```

我站在黑暗里，忽然不遗憾了。

我造的轮子，能重复造出来。

不是被抄袭，不是被遗忘，而是被理解、被实现、被超越。

## 8 尾声

天亮了。

年轻人去睡觉。

我留在他屏幕前，看着 `parser.py` 的最后一节：

```python
if __name__ == "__main__":
    sys.exit(main())
```

我笑了。

“好。”我说。

“轮子归你们了。”

然后我走向那行代码后面的空白。

那里没有天堂，没有地狱，只有下一个文法。

（全剧终）

作者注：本文中的 `parser.py` 项目真实存在，LL(1)/SLR/LALR、`%tree`、`%ast_*`、`%regex`、`%identifier`、语义动作翻译、分文件布局、`pack.py` 打包均为其真实功能。Codex+DeepSeek 花费不到 20 元为文学化表达。唐纳德指 Donald Knuth，LR 解析由他提出。他造的轮子，确实能在这个时代被重新造出来。