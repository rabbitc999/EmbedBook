#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
parser_gen.py —— 单文件 Yacc 风格非递归 LL(1) 解析器生成器

generate_parser_c(grammar, tokens)
----------------------------------
输入：
  grammar: {
      "start": "expr",
      "rules": {
          "expr": [
              {"rhs": ["term", "expr_tail"], "action": "$$ = $1 + $2;"}
          ],
          ...
      }
  }
  tokens: 终结符名字列表，TOKEN_END 自动加入并作为输入结束符

语义动作写法与 Yacc 类似：
  $$ : 本规则返回值
  $1, $2 ... : 第 n 个右部符号的返回值
  非终结符调用后返回 ParseResult_T.value；
  NUMBER 终结符返回词法层填入的 TOKEN.value；
  VAR/IDENT 终结符返回 Var_FindVariable() 查到的变量对象。

动作里的算术/逻辑运算符（$1 + $2、$2 && $3、-$2 等）在生成 C 代码时会被自动
改写为 Var_* 运行时调用；[$1] 表示按变量名解析（等价 Var_FindVariable）。

动作表达式还支持：
  - 任意括号与优先级，例如 ($1 + $2) * $3 / ($1 - $2)；
  - 类型字段 $1.int32 / $1.float 读写数值（配合 $1.int32 = 表达式）；
  - 类型构造 int32(0)、float(1.5) 等，替代手写 Var_CreateInt(...)；
  - 未对运行时值定义的运算符（如 $1 ^ $2）会在生成时输出警告，
    建议显式写成 $1.int32 ^ $2.int32 再参与 C 层运算。

生成内容：
  - Token 枚举
  - ParserToken_T（携带 value / line / column）
  - 默认 Lexer、预测分析表与显式分析栈
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import warnings

"""
.gram 文本文法加载模块

把 Yacc 风格但去掉 '|' 分隔符的文法描述转换成 parser_gen.py 需要的字典结构：

    %token NUMBER PLUS MINUS STAR SLASH LPAREN RPAREN VAR
    %start expr

    %var X int32 = 0
    %var table int32[16]
    %var pid struct {
        kp float
        ki float
    }

    expr := expr PLUS term      { $$ = $1 + $3; }
    expr := term                { $$ = $1; }

规则特点：
  - 每条产生式独立写成  lhs := rhs { action }，不使用 '|'，也不需要结尾 ';'；
  - %empty 表示空产生式；
  - 允许直接左递归，后续由 parser_gen.py 转换成“基产生式 + 循环续产生式”。
"""

import io
import re

# 与 parser_gen.py 保持一致的标量类型
GRAMMAR_SCALAR_TYPES = {
    "int8", "uint8", "char", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "float", "double", "string", "int",
}

# 文本类型名 -> 内部规范名
GRAMMAR_TYPE_ALIAS = {
    "int": "int32",
    "uint": "uint32",
    "int8_t": "int8",
    "uint8_t": "uint8",
    "int16_t": "int16",
    "uint16_t": "uint16",
    "int32_t": "int32",
    "uint32_t": "uint32",
    "int64_t": "int64",
    "uint64_t": "uint64",
}

# 文本词法器始终提供的基础终结符
DEFAULT_TOKEN_NAMES = ["STRING", "NUMBER", "VARIABLE"]


class GrammarLoadError(ValueError):
    """.gram 语法错误。"""


def _decode_string_literal(text):
    """把 C 风格字符串/字符字面量转成 Python 字符串。"""
    body = text[1:-1]
    result = []
    index = 0
    escape_map = {
        "n": "\n", "r": "\r", "t": "\t", "0": "\0",
        "\\": "\\", '"': '"', "'": "'",
    }
    while index < len(body):
        ch = body[index]
        if ch == "\\" and index + 1 < len(body):
            nxt = body[index + 1]
            result.append(escape_map.get(nxt, nxt))
            index += 2
        else:
            result.append(ch)
            index += 1
    return "".join(result)


class _GramTokenizer:
    """把 .gram 文本切成 token；注释被跳过，但保留换行以便指令按行结束。"""

    _RE = re.compile(
        r"(?P<num>\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?)|"
        r"(?P<str>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')|"
        r"(?P<id>[A-Za-z_%][A-Za-z0-9_]*)|"
        r"(?P<dollar>\$\$|\$\d+|\$)|"
        r"(?P<op>\:=|&&|\|\||<<|>>|<=|>=|==|!=|"
        r"[+\-*/%^&|!~<>=?:(.)\[\]{},;:])|"
        r"(?P<nl>\n)",
        re.VERBOSE
    )

    def __init__(self, text):
        self.text = text
        self.tokens = []
        self.pos = 0
        self._skip_comments_and_collect()

    def _skip_comments_and_collect(self):
        """逐字符扫描：// 与 /* */ 注释跳过，字符串整体成 token。"""
        text = self.text
        i = 0
        while i < len(text):
            ch = text[i]
            # 行注释
            if ch == "/" and i + 1 < len(text) and text[i + 1] == "/":
                while i < len(text) and text[i] != "\n":
                    i += 1
                continue
            # 块注释
            if ch == "/" and i + 1 < len(text) and text[i + 1] == "*":
                i += 2
                while i + 1 < len(text) and not (
                        text[i] == "*" and text[i + 1] == "/"):
                    i += 1
                i = min(i + 2, len(text))
                continue
            # 空白与换行
            if ch in " \t\r":
                i += 1
                continue
            if ch == "\n":
                self.tokens.append(("nl", "\n", i, i + 1))
                i += 1
                continue
            match = self._RE.match(text, i)
            if match is None:
                raise GrammarLoadError(
                    "无法识别的字符: %r (位置 %d)" % (ch, i)
                )
            kind = match.lastgroup
            self.tokens.append(
                (kind, match.group(), match.start(), match.end())
            )
            i = match.end()


class _GramParser:
    """把 token 流解析成 tokens/rules/variables 字典。"""

    def __init__(self, text):
        self.text = text
        self.tokens = _GramTokenizer(text).tokens
        self.pos = 0
        self.token_names = list(DEFAULT_TOKEN_NAMES)
        self.token_defs = {}
        self.literal_owners = {}
        self.start = None
        self.rules = {}
        self.variables = {}
        self.first_rule = None

    # ------------------------------------------------------------------
    def _peek(self, offset=0):
        index = self.pos + offset
        if index < len(self.tokens):
            return self.tokens[index][1]
        return None

    def _peek_kind(self, offset=0):
        index = self.pos + offset
        if index < len(self.tokens):
            return self.tokens[index][0]
        return None

    def _skip_nl(self):
        while self._peek() == "\n":
            self.pos += 1

    def _accept(self, text):
        if self._peek() == text:
            self.pos += 1
            return True
        return False

    def _expect(self, text):
        if not self._accept(text):
            raise GrammarLoadError(
                "期望 '%s'，实际得到 '%s'" % (text, self._peek())
            )

    # ------------------------------------------------------------------
    def parse(self):
        """主入口：解析完整文件。"""
        while self.pos < len(self.tokens):
            self._skip_nl()
            if self.pos >= len(self.tokens):
                break
            token = self._peek()
            if token == "%token":
                self._parse_token_directive()
            elif token == "%define":
                self._parse_define_directive()
            elif token == "%start":
                self._parse_start_directive()
            elif token == "%var":
                self._parse_var_directive()
            elif (self._peek_kind() == "id" and
                  self._peek(1) == ":=" and
                  not token.startswith("%")):
                self._parse_production()
            else:
                raise GrammarLoadError(
                    "无法解析的语句起点: '%s'（第 %d 个 token）"
                    % (token, self.pos)
                )

        if self.start is None:
            if self.first_rule is None:
                raise GrammarLoadError("没有找到任何文法规则")
            self.start = self.first_rule

        # 校验：RHS 符号必须已声明为终结符或非终结符
        known = set(self.token_names) | set(self.rules.keys())
        for lhs, prods in self.rules.items():
            if lhs in self.token_names:
                raise GrammarLoadError(
                    "非终结符 %s 与终结符同名" % lhs
                )
            for prod in prods:
                for symbol in prod["rhs"]:
                    if symbol not in known:
                        raise GrammarLoadError(
                            "规则 %s 使用了未定义的符号 %s"
                            % (lhs, symbol)
                        )
        return {
            "tokens": self.token_names,
            "token_defs": self.token_defs,
            "grammar": {"start": self.start, "rules": self.rules},
            "variables": self.variables,
        }

    # ------------------------------------------------------------------
    def _parse_token_directive(self):
        """%token A B C ...（一行一条指令，换行结束）。"""
        self.pos += 1
        while True:
            if self.pos >= len(self.tokens):
                break
            token = self._peek()
            if token == "\n":
                self.pos += 1
                break
            if token == ",":
                self.pos += 1
                continue
            if self._peek_kind() != "id" or token.startswith("%"):
                raise GrammarLoadError(
                    "%%token 只能包含标识符，实际得到 '%s'" % token
                )
            if token not in self.token_names:
                self.token_names.append(token)
            self.pos += 1

    def _parse_define_directive(self):
        """解析固定字符串 Token；仅允许使用竖线表达多个候选。"""
        self.pos += 1
        if self._peek_kind() != "id" or self._peek().startswith("%"):
            raise GrammarLoadError("%define 后应跟 Token 名")
        token_name = self._peek()
        self.pos += 1
        if self._peek_kind() != "str" or not self._peek().startswith('\"'):
            raise GrammarLoadError("%define 规则必须使用双引号")
        literal_text = self._peek()
        self.pos += 1
        branches = self._split_define_literal(literal_text)
        if token_name in self.token_defs:
            raise GrammarLoadError("Token %s 重复定义" % token_name)
        for literal in branches:
            owner = self.literal_owners.get(literal)
            if owner is not None:
                raise GrammarLoadError(
                    "字面量 %r 已由 Token %s 定义" % (literal, owner)
                )
            self.literal_owners[literal] = token_name
        self.token_defs[token_name] = branches
        if token_name not in self.token_names:
            self.token_names.append(token_name)
        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%define %s 后有多余内容 '%s'"
                % (token_name, self._peek())
            )

    @staticmethod
    def _split_define_literal(text):
        """拆分 `%define` 字面量，并把 `\\|` 保留为普通竖线。"""
        escape_map = {
            "n": "\n", "r": "\r", "t": "\t", "0": "\0",
            "\\": "\\", '\"': '\"', "|": "|",
        }
        branches = []
        current = []
        body = text[1:-1]
        index = 0
        while index < len(body):
            ch = body[index]
            if ch == "\\":
                if index + 1 >= len(body):
                    raise GrammarLoadError("%define 字面量末尾存在孤立转义符")
                nxt = body[index + 1]
                current.append(escape_map.get(nxt, nxt))
                index += 2
                continue
            if ch == "|":
                literal = "".join(current)
                if literal == "":
                    raise GrammarLoadError("%define 不允许空分支")
                branches.append(literal)
                current = []
            else:
                current.append(ch)
            index += 1
        literal = "".join(current)
        if literal == "":
            raise GrammarLoadError("%define 不允许空分支")
        branches.append(literal)
        if len(set(branches)) != len(branches):
            raise GrammarLoadError("%define 内存在重复字面量")
        return branches

    def _parse_start_directive(self):
        self.pos += 1
        if self._peek_kind() != "id":
            raise GrammarLoadError("%start 后应跟起始非终结符名")
        self.start = self._peek()
        self.pos += 1
        self._skip_nl()
        if self._peek() == "\n":
            self.pos += 1

    # ------------------------------------------------------------------
    def _parse_var_directive(self):
        """%var 名字 类型 [= 初始值]（一条声明，可跨行写 struct/dict）。"""
        self.pos += 1
        if self._peek_kind() != "id":
            raise GrammarLoadError("%var 后应跟变量名")
        name = self._peek()
        self.pos += 1
        spec = self._parse_type()

        if self._accept("="):
            initialized = self._parse_typed_initializer(spec)
            spec = initialized

        if self._peek() == "\n":
            self.pos += 1
        elif self._peek() is not None:
            raise GrammarLoadError(
                "%%var %s 声明后有多余内容 '%s'"
                % (name, self._peek())
            )
        self.variables[name] = spec

    def _parse_type(self):
        """类型表达式：标量 / struct {...} / dict[...]，后可跟数组后缀。"""
        token = self._peek()
        if self._peek_kind() != "id":
            raise GrammarLoadError("期望类型名，实际得到 '%s'" % token)

        if token == "struct":
            self.pos += 1
            spec = self._parse_struct()
        elif token == "dict":
            self.pos += 1
            self._expect("[")
            value_spec = self._parse_type()
            self._expect("]")
            spec = {"type": "dict", "value": value_spec}
        else:
            if token not in GRAMMAR_SCALAR_TYPES:
                raise GrammarLoadError(
                    "未知类型 '%s'，支持的标量：%s"
                    % (token, ", ".join(sorted(GRAMMAR_SCALAR_TYPES)))
                )
            canonical = GRAMMAR_TYPE_ALIAS.get(token, token)
            self.pos += 1
            spec = {"type": canonical}

        # 数组后缀：int32[4]、float[3][2]；先写的维度是外层
        dimensions = []
        while self._peek() == "[":
            self.pos += 1
            if self._peek_kind() != "num":
                raise GrammarLoadError("数组长度必须是正整数")
            length = int(float(self._peek()))
            if length <= 0:
                raise GrammarLoadError("数组长度必须是正整数")
            self.pos += 1
            self._expect("]")
            dimensions.append(length)
        for length in reversed(dimensions):
            spec = {"type": "array", "element": spec, "length": length}
        return spec

    def _parse_struct(self):
        """struct { 字段名 类型; ... }，字段以换行或 ; 分隔。"""
        fields = {}
        self._expect("{")
        while True:
            self._skip_nl()
            if self._accept("}"):
                break
            if self._peek_kind() != "id":
                raise GrammarLoadError(
                    "结构体字段名必须是标识符，实际得到 '%s'" % self._peek()
                )
            field_name = self._peek()
            self.pos += 1
            if field_name in fields:
                raise GrammarLoadError(
                    "结构体字段重复: %s" % field_name
                )
            fields[field_name] = self._parse_type()
            # 可选的 ; / , 分隔符
            self._accept(";")
            self._accept(",")
        return {"type": "struct", "fields": fields}

    def _parse_scalar_value(self, type_name):
        """解析 = 后面的初始值：数值、字符串或字符。"""
        self._skip_nl()
        negative = False
        if self._peek() == "-" and self._peek_kind(1) == "num":
            negative = True
            self.pos += 1
        kind = self._peek_kind()
        token = self._peek()
        if token is None:
            raise GrammarLoadError("= 后面缺少初始值")

        if kind == "num":
            text = token
            self.pos += 1
            if "." in text or "e" in text or "E" in text:
                value = float(text)
            else:
                value = int(text)
            if negative:
                value = -value
            return value

        if kind == "str":
            self.pos += 1
            if type_name not in ("char", "string"):
                raise GrammarLoadError(
                    "类型 %s 不能用字符串/字符字面量初始化" % type_name
                )
            if type_name == "char":
                text = _decode_string_literal(token)
                if len(text) != 1:
                    raise GrammarLoadError(
                        "char 初始化必须是单个字符: %s" % token
                    )
                return text
            return _decode_string_literal(token)

        raise GrammarLoadError("无法解析的初始值: '%s'" % token)

    def _parse_typed_initializer(self, target_spec):
        """解析一个与 target_spec 类型一致的初始化值。

        返回新规格（不改动模板），标量填 'value'，聚合填 'init'/'init_items'。
        """
        type_name = target_spec.get("type", "int32")
        if type_name in GRAMMAR_SCALAR_TYPES or type_name in (
                "int32", "uint8", "int8", "uint16", "int16", "uint32",
                "uint64", "int64"):
            value = self._parse_scalar_value(type_name)
            return {"type": type_name, "value": value}

        if type_name == "array":
            element_spec = target_spec["element"]
            length = int(target_spec.get("length", 0))
            self._expect("{")
            init_items = []
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                init_items.append(
                    self._parse_typed_initializer(element_spec)
                )
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            if len(init_items) > length:
                raise GrammarLoadError(
                    "数组初始化项 %d 超过声明长度 %d"
                    % (len(init_items), length)
                )
            return {
                "type": "array",
                "element": element_spec,
                "length": length,
                "init": init_items,
            }

        if type_name == "struct":
            fields = target_spec["fields"]
            init_map = {}
            self._expect("{")
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                if self._peek_kind() != "id":
                    raise GrammarLoadError(
                        "结构体初始化应写 字段名: 值，实际得到 '%s'"
                        % self._peek()
                    )
                field_name = self._peek()
                if field_name not in fields:
                    raise GrammarLoadError(
                        "结构体没有字段 %s" % field_name
                    )
                self.pos += 1
                self._expect(":")
                init_map[field_name] = self._parse_typed_initializer(
                    fields[field_name]
                )
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            return {
                "type": "struct",
                "fields": fields,
                "init": init_map,
            }

        if type_name == "dict":
            value_spec = target_spec.get("value")
            if value_spec is None:
                raise GrammarLoadError("dict 类型必须声明值类型")
            self._expect("{")
            init_items = []
            while True:
                self._skip_nl()
                if self._accept("}"):
                    break
                key_kind = self._peek_kind()
                key_text = self._peek()
                if key_kind == "id":
                    self.pos += 1
                elif key_kind == "str":
                    key_text = _decode_string_literal(key_text)
                    self.pos += 1
                else:
                    raise GrammarLoadError(
                        "字典键必须是字符串字面量或标识符，实际得到 '%s'"
                        % key_text
                    )
                self._expect(":")
                value_init = self._parse_typed_initializer(value_spec)
                init_items.append((key_text, value_init))
                self._skip_nl()
                if not self._accept(","):
                    self._skip_nl()
                    self._expect("}")
                    break
            return {
                "type": "dict",
                "value": value_spec,
                "init_items": init_items,
            }

        raise GrammarLoadError("不支持的初始化类型: %s" % type_name)

    # ------------------------------------------------------------------
    def _parse_production(self):
        """lhs := rhs { action }，禁止 '|'，动作必须存在。"""
        lhs = self._peek()
        if lhs.startswith("%"):
            raise GrammarLoadError("规则名不能以 %% 开头: %s" % lhs)
        self.pos += 1
        self._expect(":=")
        if self.first_rule is None:
            self.first_rule = lhs

        rhs = []
        while True:
            if self._peek() is None:
                raise GrammarLoadError(
                    "规则 %s := ... 缺少 { 动作 }" % lhs
                )
            if self._peek() == "{":
                break
            if self._peek_kind() == "id":
                symbol = self._peek()
                if symbol == "%empty":
                    self.pos += 1
                    rhs = []
                    continue
                rhs.append(symbol)
                self.pos += 1
                continue
            if self._peek() == "\n":
                self.pos += 1
                continue
            raise GrammarLoadError(
                "规则 %s 的 RHS 不能包含 '%s'"
                % (lhs, self._peek())
            )

        # 动作块：括号配对称取整段文本，允许跨行与嵌套
        action = self._read_action_block()
        self.rules.setdefault(lhs, []).append({
            "rhs": rhs,
            "action": action,
        })

    def _read_action_block(self):
        """读取 { ... } 动作，返回去掉外层花括号的原始文本。"""
        start_token = self.tokens[self.pos]
        if start_token[1] != "{":
            raise GrammarLoadError("期望动作 { ... }")
        depth = 0
        index = self.pos
        while index < len(self.tokens):
            kind, text = self.tokens[index][0], self.tokens[index][1]
            if kind == "op":
                if text == "{":
                    depth += 1
                elif text == "}":
                    depth -= 1
                    if depth == 0:
                        break
            index += 1
        if index >= len(self.tokens):
            raise GrammarLoadError("动作缺少右花括号 }")

        body = self.text[start_token[2] + 1:self.tokens[index][2]]
        self.pos = index + 1
        return body.strip()


def load_grammar_text(text):
    """解析 .gram 文本，返回 parser_gen.py 的输入字典。"""
    return _GramParser(text).parse()


def load_grammar_file(path):
    """读取并解析 .gram 文件。"""
    with io.open(path, "r", encoding="utf-8") as file_handle:
        return load_grammar_text(file_handle.read())

EPS = ""


def compute_first(rules, terminals, nonterms):
    """计算每个非终结符的 First 集合（含 EPS 表示可空）。"""
    first = {n: set() for n in nonterms}
    changed = True
    while changed:
        changed = False
        for lhs, prods in rules.items():
            for prod in prods:
                rhs = prod["rhs"]
                index = 0
                while index < len(rhs):
                    sym = rhs[index]
                    if sym in terminals:
                        if sym not in first[lhs]:
                            first[lhs].add(sym)
                            changed = True
                        break
                    # 非终结符
                    before = len(first[lhs])
                    first[lhs] |= (first[sym] - {EPS})
                    if len(first[lhs]) != before:
                        changed = True
                    if EPS not in first[sym]:
                        break
                    index += 1
                else:
                    if EPS not in first[lhs]:
                        first[lhs].add(EPS)
                        changed = True
    return first


def compute_follow(start, rules, terminals, nonterms, first):
    """计算每个非终结符的 Follow 集合。"""
    follow = {n: set() for n in nonterms}
    follow[start].add("END")
    changed = True
    while changed:
        changed = False
        for lhs, prods in rules.items():
            for prod in prods:
                rhs = prod["rhs"]
                for i, sym in enumerate(rhs):
                    if sym not in nonterms:
                        continue
                    suffix = rhs[i + 1:]
                    suffix_first = set()
                    nullable = True
                    for s in suffix:
                        if s in terminals:
                            suffix_first.add(s)
                            nullable = False
                            break
                        suffix_first |= (first[s] - {EPS})
                        if EPS not in first[s]:
                            nullable = False
                            break
                    before = len(follow[sym])
                    follow[sym] |= suffix_first
                    if nullable:
                        follow[sym] |= follow[lhs]
                    if len(follow[sym]) != before:
                        changed = True
    return follow


def select_set(prod, lhs, rules, terminals, nonterms, first, follow):
    """LL(1) 选择集。"""
    rhs = prod["rhs"]
    result = set()
    nullable = True
    for sym in rhs:
        if sym in terminals:
            result.add(sym)
            nullable = False
            break
        result |= (first[sym] - {EPS})
        if EPS not in first[sym]:
            nullable = False
            break
    if nullable:
        result |= follow[lhs]
    return result


def _eliminate_direct_left_recursion(grammar):
    """消除直接左递归，并保留后续生成语义折叠所需的元数据。"""
    source_rules = grammar["rules"]
    final_rules = {}
    for lhs, productions in source_rules.items():
        recursive = [
            prod for prod in productions
            if prod.get("rhs") and prod["rhs"][0] == lhs
        ]
        base = [prod for prod in productions if prod not in recursive]
        if not recursive:
            final_rules[lhs] = [dict(prod) for prod in productions]
            continue
        if not base:
            raise ValueError(
                "文法错误：%s 只有直接左递归产生式，缺少基础产生式" % lhs
            )
        tail = lhs + "__tail"
        while tail in source_rules or tail in final_rules:
            tail += "_"
        final_rules[lhs] = []
        for prod in base:
            transformed = dict(prod)
            transformed["rhs"] = list(prod.get("rhs", [])) + [tail]
            transformed["transform"] = "left_base"
            transformed["base_rhs_length"] = len(prod.get("rhs", []))
            final_rules[lhs].append(transformed)
        final_rules[tail] = []
        for prod in recursive:
            transformed = dict(prod)
            transformed["rhs"] = list(prod["rhs"][1:]) + [tail]
            transformed["transform"] = "left_continue"
            transformed["owner"] = lhs
            final_rules[tail].append(transformed)
        final_rules[tail].append({
            "rhs": [],
            "action": "",
            "transform": "left_empty",
            "owner": lhs,
        })
    return {"start": grammar["start"], "rules": final_rules}


def _check_indirect_left_recursion(rules):
    """检查含可空前缀的左角图，拒绝间接左递归。"""
    nonterms = set(rules)
    nullable = set()
    changed = True

    # 先求可空非终结符闭包，避免漏掉 b 可空时 a := b c 的左角 c。
    while changed:
        changed = False
        for lhs, productions in rules.items():
            for prod in productions:
                rhs = prod.get("rhs", [])
                if all(symbol in nullable for symbol in rhs):
                    if lhs not in nullable:
                        nullable.add(lhs)
                        changed = True
                    break

    # 每条产生式沿可空前缀收集所有可能成为最左符号的非终结符。
    graph = {name: [] for name in rules}
    for lhs, productions in rules.items():
        for prod in productions:
            rhs = prod.get("rhs", [])
            for symbol in rhs:
                if symbol not in nonterms:
                    break
                graph[lhs].append(symbol)
                if symbol not in nullable:
                    break

    def visit(name, path):
        """深度优先检查当前非终结符是否回到访问路径。"""
        if name in path:
            cycle = path[path.index(name):] + [name]
            raise ValueError(
                "文法包含间接左递归：%s" % " -> ".join(cycle)
            )
        for target in graph[name]:
            visit(target, path + [name])

    for nonterm in rules:
        visit(nonterm, [])


def prepare_grammar(grammar, tokens):
    """生成最终文法、FIRST/FOLLOW 集合与非递归预测分析表。"""
    final_grammar = _eliminate_direct_left_recursion(grammar)
    rules = final_grammar["rules"]
    _check_indirect_left_recursion(rules)
    terminals = list(dict.fromkeys(
        list(DEFAULT_TOKEN_NAMES) + list(tokens) + ["END"]
    ))
    nonterms = list(rules.keys())
    first = compute_first(rules, terminals, nonterms)
    follow = compute_follow(
        final_grammar["start"], rules, terminals, nonterms, first
    )
    productions = []
    table = {}
    for lhs, alternatives in rules.items():
        for prod in alternatives:
            prod_id = len(productions)
            entry = dict(prod)
            entry["id"] = prod_id
            entry["lhs"] = lhs
            productions.append(entry)
            selected = select_set(
                entry, lhs, rules, terminals, nonterms, first, follow
            )
            for token in selected:
                key = (lhs, token)
                if key in table:
                    previous = productions[table[key]]
                    raise ValueError(
                        "文法不是 LL(1)：M[%s, %s] 同时对应 %s -> %s 和 %s -> %s"
                        % (lhs, token, previous["lhs"],
                           " ".join(previous["rhs"]) or "%empty",
                           lhs, " ".join(entry["rhs"]) or "%empty")
                    )
                table[key] = prod_id
    return {
        "grammar": final_grammar,
        "terminals": terminals,
        "nonterms": nonterms,
        "first": first,
        "follow": follow,
        "productions": productions,
        "table": table,
    }


def format_grammar_report(analysis):
    """把最终产生式、集合和预测表格式化为稳定的教学报告。"""
    terminals = analysis["terminals"]
    order = {token: index for index, token in enumerate(terminals)}
    order[EPS] = len(order)

    def ordered(values):
        """按 Token 声明顺序排列集合元素。"""
        return sorted(values, key=lambda item: (order.get(item, 999999), item))

    lines = ["[最终产生式]"]
    for prod in analysis["productions"]:
        rhs = " ".join(prod["rhs"]) if prod["rhs"] else "%empty"
        lines.append("P%d: %s -> %s" % (prod["id"], prod["lhs"], rhs))
    lines.extend(["", "[FIRST]"])
    for name in analysis["nonterms"]:
        values = ["%empty" if item == EPS else item
                  for item in ordered(analysis["first"][name])]
        lines.append("FIRST(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[FOLLOW]"])
    for name in analysis["nonterms"]:
        values = ordered(analysis["follow"][name])
        lines.append("FOLLOW(%s) = { %s }" % (name, ", ".join(values)))
    lines.extend(["", "[预测分析表]"])
    for name in analysis["nonterms"]:
        for token in terminals:
            prod_id = analysis["table"].get((name, token))
            if prod_id is not None:
                lines.append("M[%s, %s] = P%d" % (name, token, prod_id))
    return "\n".join(lines) + "\n"


def _compile_and_run_generated_test(generated_code, harness_code,
                                    sanitize=False):
    """在临时目录编译运行生成的 C，可选启用地址与泄漏检查。"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory(prefix="ll1_generator_") as temp_dir:
        parser_path = os.path.join(temp_dir, "generated_parser.c")
        harness_path = os.path.join(temp_dir, "generated_harness.c")
        executable_path = os.path.join(temp_dir, "generated_harness")
        with open(parser_path, "w", encoding="utf-8") as file_handle:
            file_handle.write(generated_code)
        with open(harness_path, "w", encoding="utf-8") as file_handle:
            file_handle.write(harness_code)
        compile_command = [
            "gcc", "-std=c99", "-Wall", "-Wextra", "-Werror",
            "-I", script_dir, harness_path,
            os.path.join(script_dir, "variable_runtime.c"),
            "-lm", "-o", executable_path,
        ]
        if sanitize:
            compile_command[1:1] = [
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer"
            ]
        subprocess.run(compile_command, check=True)
        environment = dict(os.environ)
        if sanitize:
            environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        subprocess.run([executable_path], check=True, env=environment)


# 变量标量类型 -> VarValue 构造函数
VAR_SCALAR_CREATE = {
    "int8":   "Var_CreateInt8",
    "uint8":  "Var_CreateUint8",
    "char":   "Var_CreateChar",
    "int16":  "Var_CreateInt16",
    "uint16": "Var_CreateUint16",
    "int32":  "Var_CreateInt",
    "int":    "Var_CreateInt",
    "uint32": "Var_CreateUint32",
    "int64":  "Var_CreateInt64",
    "uint64": "Var_CreateUint64",
    "float":  "Var_CreateFloat",
    "double": "Var_CreateDouble",
    "string": "Var_CreateString",
}

# 变量标量类型的默认值（C 字面量）
VAR_SCALAR_DEFAULT = {
    "int8":   "0",
    "uint8":  "0",
    "char":   "'\\0'",
    "int16":  "0",
    "uint16": "0",
    "int32":  "0",
    "int":    "0",
    "uint32": "0u",
    "int64":  "0",
    "uint64": "0u",
    "float":  "0.0f",
    "double": "0.0",
    "string": '""',
}


def _var_c_string(value):
    """把 Python 字符串转成安全的 C 字符串字面量。"""
    return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'


def _var_scalar_code(spec):
    """生成标量变量的一条创建表达式。"""
    type_name = spec.get("type", "int")
    create = VAR_SCALAR_CREATE.get(type_name, "Var_CreateInt")
    value = spec.get("value")
    if value is None:
        return f"{create}({VAR_SCALAR_DEFAULT.get(type_name, '0')})"
    if type_name == "string":
        return f"{create}({_var_c_string(value)})"
    if type_name == "char":
        ch = str(value)
        if len(ch) == 1:
            esc = {"\\": "\\\\", "'": "\\'"}.get(ch, ch)
            return f"{create}('{esc}')"
        return f"{create}('\\0')"
    if type_name in ("float", "double"):
        number = float(value)
        text = repr(number)
        if type_name == "float":
            text += "f"
        return f"{create}({text})"
    return f"{create}({str(value)})"


def _var_array_code(array_fn, element_fn, init_fns, length):
    """数组构建函数：显式初始化项在前，剩余位置用默认元素补齐。"""
    lines = [
        "static VarValue_T *%s(void)" % array_fn,
        "{",
        "    VarValue_T *pArray = NULL;",
        "    size_t index = 0u;",
        "",
        "    pArray = Var_CreateArray();",
        "    if (pArray == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for init_fn in init_fns:
        lines.append("    Var_ArrayPush(pArray, %s());" % init_fn)
    if len(init_fns) < length:
        lines.append(
            "    for (index = 0u; index < %du; index++) {"
            % (length - len(init_fns))
        )
        lines.append("        Var_ArrayPush(pArray, %s());" % element_fn)
        lines.append("    }")
    lines.append("    return pArray;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def _var_dict_code(dict_fn, entries):
    """字典构建函数：按初始化列表逐项 Var_DictSet。"""
    lines = [
        "static VarValue_T *%s(void)" % dict_fn,
        "{",
        "    VarValue_T *pDict = NULL;",
        "",
        "    pDict = Var_CreateDict();",
        "    if (pDict == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for key_text, value_fn in entries:
        lines.append(
            "    Var_DictSet(pDict, %s, %s());"
            % (_var_c_string(key_text), value_fn)
        )
    lines.append("    return pDict;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def _var_struct_code(struct_fn, fields):
    """结构体构建函数：先建结构体，再逐字段注册子函数创建的成员。"""
    lines = [
        "static VarValue_T *%s(void)" % struct_fn,
        "{",
        "    VarValue_T *pStruct = NULL;",
        "",
        "    pStruct = Var_CreateStruct();",
        "    if (pStruct == NULL) {",
        "        return NULL;",
        "    }",
    ]
    for index, (field_name, child_fn) in enumerate(fields):
        lines.append(
            "    Var_StructSetField(pStruct, %s, %s());"
            % (_var_c_string(field_name), child_fn)
        )
    lines.append("    return pStruct;")
    lines.append("}")
    return "\n".join(lines)


def _var_emit_node(spec, fn_name):
    """递归生成一个类型节点的构建函数 C 代码（子函数在前）。"""
    type_name = spec.get("type", "int")
    if type_name == "array":
        element_fn = fn_name + "_elem"
        length = int(spec.get("length", 0))
        init_items = spec.get("init", [])
        element_code = ""
        if len(init_items) < length:
            element_code = _var_emit_node(spec["element"], element_fn)
        init_fns = []
        for index, item_spec in enumerate(init_items):
            init_fn = fn_name + "_init%d" % index
            element_code += "\n\n" + _var_emit_node(item_spec, init_fn)
            init_fns.append(init_fn)
        return element_code + "\n\n" + _var_array_code(
            fn_name, element_fn, init_fns, length)
    if type_name == "struct":
        children = []
        child_codes = []
        init_map = spec.get("init", {})
        for index, (field_name, field_spec) in enumerate(
                spec.get("fields", {}).items()):
            child_fn = fn_name + "_f%d_%s" % (index, field_name)
            children.append((field_name, child_fn))
            emit_spec = init_map.get(field_name, field_spec)
            child_codes.append(_var_emit_node(emit_spec, child_fn))
        return "\n\n".join(child_codes + [_var_struct_code(fn_name, children)])
    if type_name == "dict":
        child_codes = []
        entries = []
        for index, (key_text, value_spec) in enumerate(
                spec.get("init_items", [])):
            value_fn = fn_name + "_item%d" % index
            entries.append((key_text, value_fn))
            child_codes.append(_var_emit_node(value_spec, value_fn))
        return "\n\n".join(
            child_codes + [_var_dict_code(fn_name, entries)])
    return "static VarValue_T *%s(void)\n{\n    return %s;\n}\n" % (
        fn_name, _var_scalar_code(spec))


# ---------------------------------------------------------------------------
# 动作表达式翻译器
# ---------------------------------------------------------------------------
# DSL 类型表：名字 -> (C 类型, VarValue 构造函数)
ACTION_TYPE_TABLE = {
    "char":   ("char", "Var_CreateChar"),
    "int8":   ("int8_t", "Var_CreateInt8"),
    "uint8":  ("uint8_t", "Var_CreateUint8"),
    "int16":  ("int16_t", "Var_CreateInt16"),
    "uint16": ("uint16_t", "Var_CreateUint16"),
    "int32":  ("int32_t", "Var_CreateInt"),
    "uint32": ("uint32_t", "Var_CreateUint32"),
    "int64":  ("int64_t", "Var_CreateInt64"),
    "uint64": ("uint64_t", "Var_CreateUint64"),
    "float":  ("float", "Var_CreateFloat"),
    "double": ("double", "Var_CreateDouble"),
}

ACTION_FLOAT_TYPES = {"float", "double"}

# 类型化赋值使用的运行时函数
ACTION_SETTER_TABLE = {
    "char":   "Var_SetChar",
    "int8":   "Var_SetInt8",
    "uint8":  "Var_SetUint8",
    "int16":  "Var_SetInt16",
    "uint16": "Var_SetUint16",
    "int32":  "Var_SetInt",
    "uint32": "Var_SetUint32",
    "int64":  "Var_SetInt64",
    "uint64": "Var_SetUint64",
    "float":  "Var_SetFloat",
    "double": "Var_SetDouble",
}


class _ExprNode:
    """动作表达式节点。

    kind  : 'ptr'（VarValue_T * 表达式）或 'raw'（C 标量表达式）
    code  : 生成的 C 表达式文本
    dtype : kind == 'raw' 时的 DSL 类型名
    lvalue: None，或 ('result',)，或 ('field', 指针表达式, 类型名)
    """

    __slots__ = ("kind", "code", "dtype", "lvalue")

    def __init__(self, kind, code, dtype=None, lvalue=None):
        self.kind = kind
        self.code = code
        self.dtype = dtype
        self.lvalue = lvalue


class _ActionTranslator:
    """把 Yacc 风格动作表达式翻译为运行时 API 调用的 C 语句。"""

    # 二元运算符优先级（数字越大越先结合）
    _PRECEDENCE = {
        "||": 1,
        "&&": 2,
        "|": 3,
        "^": 4,
        "&": 5,
        "==": 6, "!=": 6,
        "<": 7, ">": 7, "<=": 7, ">=": 7,
        "<<": 8, ">>": 8,
        "+": 9, "-": 9,
        "*": 10, "/": 10, "%": 10,
    }

    # 运行时值直接支持的二元运算
    _RUNTIME_ARITH = {
        "+": "Var_Add",
        "-": "Var_Sub",
        "*": "Var_Mul",
        "/": "Var_Div",
    }
    _RUNTIME_LOGIC = {
        "&&": "Var_And",
        "||": "Var_Or",
    }

    _TOKEN_RE = re.compile(
        r"(?P<space>\s+)|"
        r"(?P<num>\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|"
        r"\.\d+(?:[eE][+-]?\d+)?)|"
        r'(?P<str>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|'
        r"(?P<id>[A-Za-z_]\w*)|"
        r"(?P<dollar>\$\$|\$\d+)|"
        r"(?P<op>&&|\|\||<<|>>|<=|>=|==|!=|"
        r"[+\-*/%^&|!~<>=?:(.)\[\];,])",
        re.VERBOSE
    )

    def __init__(self, action):
        """对单条动作文本做词法分析。"""
        self.action = action
        self.tokens = []
        for match in self._TOKEN_RE.finditer(action):
            if match.group("space"):
                continue
            if match.group("num") is not None:
                kind = "num"
            elif match.group("str") is not None:
                kind = "str"
            elif match.group("id") is not None:
                kind = "id"
            elif match.group("dollar") is not None:
                kind = "dollar"
            else:
                kind = "op"
            self.tokens.append(
                (kind, match.group(), match.start(), match.end())
            )
        self.pos = 0
        self._warned = set()

    # ------------------------------------------------------------------
    # 词法辅助
    # ------------------------------------------------------------------
    def _peek(self):
        """返回当前 token 文本；越界返回 None。"""
        if self.pos < len(self.tokens):
            return self.tokens[self.pos][1]
        return None

    def _accept(self, text):
        """若当前位置是指定文本则前进并返回 True。"""
        if self._peek() == text:
            self.pos += 1
            return True
        return False

    def _expect(self, text):
        """要求当前位置是指定文本。"""
        if not self._accept(text):
            raise ValueError(
                f"动作 '{self.action}' 期望 '{text}'，"
                f"实际遇到 '{self._peek()}'"
            )

    def _warn(self, message):
        """去重输出生成期警告。"""
        if message not in self._warned:
            self._warned.add(message)
            warnings.warn(message, RuntimeWarning, stacklevel=2)

    # ------------------------------------------------------------------
    # 类型转换辅助
    # ------------------------------------------------------------------
    def _ptr_to_raw(self, ptr_code, dtype):
        """把 VarValue 表达式读取为指定类型的 C 标量。"""
        ctype = ACTION_TYPE_TABLE[dtype][0]
        if dtype in ACTION_FLOAT_TYPES:
            return f"(({ctype})Var_ToDouble({ptr_code}))"
        return f"(({ctype})Var_ToInt({ptr_code}))"

    def _wrap_raw(self, node):
        """把 C 标量包装成 VarValue_T * 表达式。"""
        ctype, create = ACTION_TYPE_TABLE[node.dtype]
        return _ExprNode(
            "ptr",
            f"{create}(({ctype})({node.code}))"
        )

    def _common_dtype(self, left_dtype, right_dtype):
        """求两个 DSL 标量类型运算后的公共类型。"""
        if left_dtype == right_dtype:
            return left_dtype
        left_float = left_dtype in ACTION_FLOAT_TYPES
        right_float = right_dtype in ACTION_FLOAT_TYPES
        if left_float or right_float:
            if (left_dtype == "double") or (right_dtype == "double"):
                return "double"
            return "float"
        # 不同整数类型统一提升到 int64，避免中间溢出告警
        return "int64"

    # ------------------------------------------------------------------
    # 语法分析（词法 -> 表达式树）
    # ------------------------------------------------------------------
    def translate(self):
        """翻译整条动作，返回可直接写入生成的 C 文件的语句。"""
        statements = []
        while self.pos < len(self.tokens):
            lhs = self._parse_ternary()
            if self._accept("="):
                rhs = self._parse_ternary()
                statement = self._build_assignment(lhs, rhs)
            else:
                statement = lhs.code + ";"
            self._expect(";")
            statements.append(statement)
        if self.pos != len(self.tokens):
            raise ValueError(
                f"动作 '{self.action}' 存在无法识别的尾部内容："
                f"'{self.tokens[self.pos][1]}'"
            )
        return "\n        ".join(statements)

    def _parse_ternary(self):
        """ternary := binary ('?' ternary ':' ternary)?  —— 自顶向下入口"""
        condition = self._parse_binary(1)
        if self._accept("?"):
            then_node = self._parse_ternary()
            self._expect(":")
            else_node = self._parse_ternary()
            return self._make_ternary(condition, then_node, else_node)
        return condition

    def _parse_binary(self, min_prec):
        """Pratt 表达式解析，天然支持括号与任意运算符组合。"""
        left = self._parse_unary()
        while True:
            op = self._peek()
            prec = self._PRECEDENCE.get(op)
            if (prec is None) or (prec < min_prec):
                break
            self.pos += 1
            right = self._parse_binary(prec + 1)
            left = self._make_binary(op, left, right)
        return left

    def _parse_unary(self):
        """一元前缀：! - + ~"""
        op = self._peek()
        if op in ("!", "-", "+", "~"):
            self.pos += 1
            return self._make_unary(op, self._parse_unary())
        return self._parse_postfix()

    def _parse_postfix(self):
        """后缀字段访问：$1.int32 / [$1].float。"""
        node = self._parse_primary()
        while True:
            token = self._peek()
            if token == ".":
                # 必须紧跟类型名，例如 int32/float/double
                nxt = self.tokens[self.pos + 1][1] \
                    if self.pos + 1 < len(self.tokens) else None
                if (nxt is None) or (nxt not in ACTION_TYPE_TABLE):
                    break
                self.pos += 2
                if node.kind != "ptr":
                    raise ValueError(
                        f"动作 '{self.action}'：只有运行时值能带类型字段"
                    )
                raw = self._ptr_to_raw(node.code, nxt)
                node = _ExprNode("raw", raw, nxt,
                                 lvalue=("field", node.code, nxt))
                continue
            break
        return node

    def _parse_primary(self):
        """原子：数字、$$、$n、[$n]、类型构造、函数调用、括号。"""
        token = self._peek()
        if token is None:
            raise ValueError(f"动作 '{self.action}' 意外结束")

        if self.tokens[self.pos][0] == "num":
            text = self.tokens[self.pos][1]
            self.pos += 1
            dtype = "double" if (("." in text) or
                                 ("e" in text) or ("E" in text)) \
                else "int32"
            return _ExprNode("raw", text, dtype)

        if token == "(":
            self.pos += 1
            node = self._parse_ternary()
            self._expect(")")
            return node

        if token == "$$":
            self.pos += 1
            return _ExprNode("ptr", "result.value", lvalue=("result",))

        if (token == "$") or (token.startswith("$") and
                              token[1:].isdigit()):
            # $1 / $2 ...
            self.pos += 1
            index = int(token[1:])
            return _ExprNode("ptr", f"values[{index}]",
                             lvalue=("value", f"values[{index}]"))

        if token == "[":
            self.pos += 1
            dollar = self._peek()
            if not (dollar is not None and dollar.startswith("$") and
                    dollar[1:].isdigit()):
                raise ValueError(
                    f"动作 '{self.action}'：'[' 后应为 [$n] 变量引用"
                )
            index = int(dollar[1:])
            self.pos += 1
            self._expect("]")
            code = (
                f"Var_CloneValue(Parser_FindVariable(names[{index}]))"
            )
            return _ExprNode("ptr", code,
                             lvalue=("variable", index, code))

        if self.tokens[self.pos][0] == "id":
            ident = token
            self.pos += 1
            if ident == "def" and self._peek() == "[":
                return self._parse_definition_target()
            if self._peek() == "(":
                # 类型构造 int32(...)/float(...)
                if ident in ACTION_TYPE_TABLE:
                    self.pos += 1
                    arg = self._parse_ternary()
                    self._expect(")")
                    return self._make_constructor(ident, arg)
                # 其余函数调用保持原样（例如 Var_CloneValue(...)）
                code = self._consume_call(token)
                return _ExprNode("ptr", code)
            raise ValueError(
                f"动作 '{self.action}'：未定义标识符 '{ident}'，"
                f"它后面没有 '('"
            )

        raise ValueError(
            f"动作 '{self.action}'：无法解析 '{token}'"
        )

    def _parse_definition_target(self):
        """解析 def[$n]、def[$n, type] 或 def[$n, $m] 左值。"""
        self._expect("[")
        name_token = self._peek()
        if not (name_token is not None and name_token.startswith("$") and
                name_token[1:].isdigit()):
            raise ValueError(
                f"动作 '{self.action}'：def 的第一个参数必须是 $n"
            )
        name_index = int(name_token[1:])
        self.pos += 1
        type_spec = None
        if self._accept(","):
            token = self._peek()
            if token is not None and token.startswith("$") and token[1:].isdigit():
                type_spec = ("value", int(token[1:]))
                self.pos += 1
            elif token in ACTION_TYPE_TABLE or token == "string":
                base_type = token
                self.pos += 1
                dimensions = []
                while self._accept("["):
                    length_token = self._peek()
                    if (length_token is None or
                            self.tokens[self.pos][0] != "num" or
                            not length_token.isdigit() or
                            int(length_token) <= 0):
                        raise ValueError(
                            f"动作 '{self.action}'：数组长度必须是正整数"
                        )
                    dimensions.append(int(length_token))
                    self.pos += 1
                    self._expect("]")
                type_spec = ("explicit", base_type, dimensions)
            else:
                raise ValueError(
                    f"动作 '{self.action}'：def 类型只支持简单类型、数组或 $n"
                )
        self._expect("]")
        return _ExprNode(
            "ptr", f"Parser_FindVariable(names[{name_index}])",
            lvalue=("define", name_index, type_spec)
        )

    def _consume_call(self, ident):
        """从 '(' 开始按括号配对称取整段调用文本。"""
        paren_start = self.tokens[self.pos][2]
        depth = 0
        index = paren_start
        quote = None
        while index < len(self.action):
            ch = self.action[index]
            if quote is not None:
                if ch == "\\":
                    index += 1
                elif ch == quote:
                    quote = None
            else:
                if ch in "\"'":
                    quote = ch
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        index += 1
                        break
            index += 1
        end = index
        ident_start = self.tokens[self.pos - 1][2]
        while (self.pos < len(self.tokens) and
               self.tokens[self.pos][3] <= end):
            self.pos += 1
        return self.action[ident_start:end]

    # ------------------------------------------------------------------
    # 表达式树 -> C
    # ------------------------------------------------------------------
    def _make_constructor(self, dtype, arg):
        """int32(0) / float($1) 等类型构造。"""
        ctype, create = ACTION_TYPE_TABLE[dtype]
        if arg.kind == "ptr":
            code = f"{create}({self._ptr_to_raw(arg.code, dtype)})"
        else:
            code = f"{create}(({ctype})({arg.code}))"
        return _ExprNode("ptr", code)

    def _make_ternary(self, condition, then_node, else_node):
        """三元：运行时值按 Var_IsTrue 判断，选中的分支做一次深拷贝。"""
        if condition.kind == "ptr":
            if then_node.kind == "raw":
                then_node = self._wrap_raw(then_node)
            if else_node.kind == "raw":
                else_node = self._wrap_raw(else_node)
            return _ExprNode(
                "ptr",
                f"Var_IsTrue({condition.code}) ? "
                f"Var_CloneValue({then_node.code}) : "
                f"Var_CloneValue({else_node.code})"
            )
        if (then_node.kind == "ptr") or (else_node.kind == "ptr"):
            if then_node.kind == "raw":
                then_node = self._wrap_raw(then_node)
            if else_node.kind == "raw":
                else_node = self._wrap_raw(else_node)
            return _ExprNode(
                "ptr",
                f"(({condition.code}) != 0) ? "
                f"Var_CloneValue({then_node.code}) : "
                f"Var_CloneValue({else_node.code})"
            )
        dtype = self._common_dtype(then_node.dtype, else_node.dtype)
        return _ExprNode(
            "raw",
            f"(({condition.code}) ? ({then_node.code}) : "
            f"({else_node.code}))",
            dtype
        )

    def _make_unary(self, op, node):
        """一元运算符。"""
        if op == "!":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_Not({node.code})")
            return _ExprNode("raw", f"(!({node.code}))", "int32")
        if op == "-":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_Neg({node.code})")
            return _ExprNode("raw", f"(-({node.code}))", node.dtype)
        if op == "+":
            if node.kind == "ptr":
                return _ExprNode("ptr", f"Var_CloneValue({node.code})")
            return _ExprNode("raw", f"(+({node.code}))", node.dtype)
        if op == "~":
            if node.kind == "ptr":
                self._warn(
                    f"动作 '{self.action}'：运算符 ~ 未对运行时值定义，"
                    "已按 int32 隐式换算；建议写 $n.int32 后参与位运算"
                )
                return _ExprNode(
                    "raw",
                    f"(~(int32_t)Var_ToInt({node.code}))",
                    "int32"
                )
            return _ExprNode("raw", f"(~({node.code}))", node.dtype)
        raise ValueError(f"动作 '{self.action}'：未知一元运算符 {op}")

    def _make_binary(self, op, left, right):
        """二元运算符，自动区分运行时值与 C 标量。"""
        # 两侧都是运行时值：优先映射到 Var_* 动态运算
        if (left.kind == "ptr") and (right.kind == "ptr"):
            fn = self._RUNTIME_ARITH.get(op) or \
                self._RUNTIME_LOGIC.get(op)
            if fn is not None:
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            # 未定义的运行时运算符：警告并按 int32 隐式换算
            self._warn(
                f"动作 '{self.action}'：运算符 {op} 未对运行时值定义，"
                f"已按 int32 隐式换算；建议写 $n.int32 {op} $n.int32"
            )
            left_raw = self._ptr_to_raw(left.code, "int32")
            right_raw = self._ptr_to_raw(right.code, "int32")
            return self._make_binary(op,
                                     _ExprNode("raw", left_raw, "int32"),
                                     _ExprNode("raw", right_raw, "int32"))

        # 一侧是运行时值、一侧是标量：标量先包成 VarValue 再走动态运算
        if (left.kind == "ptr") or (right.kind == "ptr"):
            if op in self._RUNTIME_ARITH:
                if left.kind == "raw":
                    left = self._wrap_raw(left)
                if right.kind == "raw":
                    right = self._wrap_raw(right)
                fn = self._RUNTIME_ARITH[op]
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            if op in self._RUNTIME_LOGIC:
                if left.kind == "raw":
                    left = self._wrap_raw(left)
                if right.kind == "raw":
                    right = self._wrap_raw(right)
                fn = self._RUNTIME_LOGIC[op]
                return _ExprNode("ptr", f"{fn}({left.code}, {right.code})")
            # 位运算 / 移位 / 比较：把运行时值按 int32 读取
            if left.kind == "ptr":
                left = _ExprNode(
                    "raw", self._ptr_to_raw(left.code, "int32"), "int32")
            if right.kind == "ptr":
                right = _ExprNode(
                    "raw", self._ptr_to_raw(right.code, "int32"), "int32")
            return self._make_binary(op, left, right)

        # 两侧都是 C 标量：按 C 类型提升直接生成运算
        return self._make_raw_binary(op, left, right)

    def _make_raw_binary(self, op, left, right):
        """C 标量运算：返回 'raw' 节点，顶层赋值时再包成 VarValue。"""
        comparison = op in ("==", "!=", "<", ">", "<=", ">=")
        int_only = op in ("%", "&", "|", "^", "<<", ">>")
        dtype = self._common_dtype(left.dtype, right.dtype)

        # 比较结果直接是 0/1
        if comparison:
            return _ExprNode(
                "raw",
                f"(({left.code}) {op} ({right.code}))",
                "int32"
            )
        # 位运算只允许整数参与：若出现浮点则统一按 int64 读取
        if int_only and (dtype in ACTION_FLOAT_TYPES):
            self._warn(
                f"动作 '{self.action}'：运算符 {op} 在浮点上无定义，"
                "已把两侧按 int64 截断"
            )
            dtype = "int64"
        ctype = ACTION_TYPE_TABLE[dtype][0]
        return _ExprNode(
            "raw",
            f"(({ctype})({left.code}) {op} ({ctype})({right.code}))",
            dtype
        )

    def _build_assignment(self, lhs, rhs):
        """根据左值的种类生成赋值语句。"""
        if lhs.lvalue is None:
            raise ValueError(
                f"动作 '{self.action}'：'=' 左侧不是可赋值目标"
            )
        lvalue_kind = lhs.lvalue[0]

        # $$ = 表达式：把 C 标量自动包成 VarValue
        if lvalue_kind == "result":
            if rhs.kind == "ptr":
                return f"{lhs.code} = {rhs.code};"
            return f"{lhs.code} = {self._wrap_raw(rhs).code};"

        # $n.type / [$n].type = 表达式：直接覆盖数值内容
        if lvalue_kind == "field":
            _, ptr_code, dtype = lhs.lvalue
            setter = ACTION_SETTER_TABLE[dtype]
            if rhs.kind == "ptr":
                raw = self._ptr_to_raw(rhs.code, dtype)
            else:
                ctype = ACTION_TYPE_TABLE[dtype][0]
                raw = f"(({ctype})({rhs.code}))"
            return f"{setter}({ptr_code}, {raw});"

        # def[$n, ...] = 表达式：定义新变量并固定其类型
        if lvalue_kind == "define":
            _, name_index, type_spec = lhs.lvalue
            rhs_code = rhs.code if rhs.kind == "ptr" else self._wrap_raw(rhs).code
            if type_spec is None:
                return (
                    "if (actionStatus == PARSER_OK) { "
                    f"actionStatus = Parser_DefineVariable(names[{name_index}], "
                    f"{rhs_code}, NULL); }}"
                )
            if type_spec[0] == "value":
                return (
                    "if (actionStatus == PARSER_OK) { "
                    f"actionStatus = Parser_DefineVariable(names[{name_index}], "
                    f"{rhs_code}, values[{type_spec[1]}]); }}"
                )
            _, base_type, dimensions = type_spec
            enum_name = {
                "int32": "VAR_TYPE_INT", "int": "VAR_TYPE_INT",
                "uint8": "VAR_TYPE_UINT8", "int8": "VAR_TYPE_INT8",
                "uint16": "VAR_TYPE_UINT16", "int16": "VAR_TYPE_INT16",
                "uint32": "VAR_TYPE_UINT32", "uint64": "VAR_TYPE_UINT64",
                "int64": "VAR_TYPE_INT64", "float": "VAR_TYPE_FLOAT",
                "double": "VAR_TYPE_DOUBLE", "char": "VAR_TYPE_CHAR",
                "string": "VAR_TYPE_STRING",
            }[base_type]
            if dimensions:
                dims = ", ".join(str(item) + "u" for item in dimensions)
                return (
                    f"{{ const uint32_t dimensions[] = {{ {dims} }}; "
                    "if (actionStatus == PARSER_OK) { "
                    f"actionStatus = Parser_DefineTyped(names[{name_index}], "
                    f"{rhs_code}, {enum_name}, dimensions, "
                    f"{len(dimensions)}u); }} }}"
                )
            return (
                "if (actionStatus == PARSER_OK) { "
                f"actionStatus = Parser_DefineTyped(names[{name_index}], "
                f"{rhs_code}, {enum_name}, NULL, 0u); }}"
            )

        # [$n] = 表达式：只允许给已经存在的固定类型变量赋值
        if lvalue_kind == "variable":
            _, name_index, _ = lhs.lvalue
            rhs_code = rhs.code if rhs.kind == "ptr" else self._wrap_raw(rhs).code
            return (
                "if (actionStatus == PARSER_OK) { "
                f"actionStatus = Parser_AssignVariable(names[{name_index}], "
                f"{rhs_code}); }}"
            )

        raise ValueError(
            f"动作 '{self.action}'：直接给 $n 赋值会把子结果指针指向新对象，"
            "会破坏归属关系；请用 $n.int32 = 表达式 或 $$ = 表达式"
        )


def _translate_action(action):
    """对外入口：把一条动作文本翻译成可生成的 C 语句。"""
    return _ActionTranslator(action).translate()


def c_token_name(token):
    return "TOKEN_" + token


def _generator_registry_block(variables):
    """生成固定容量变量表，以及定义和固定类型赋值接口。"""
    builders = []
    initializers = []
    for index, (name, spec) in enumerate(variables.items()):
        if isinstance(spec, str):
            spec = {"type": spec}
        elif not isinstance(spec, dict):
            spec = {"type": "int32", "value": spec}
        builder = "Parser_CreateBuiltin_%d" % index
        builders.append(_var_emit_node(spec, builder))
        initializers.append(
            "    Parser_StoreBuiltin(%s, %s());"
            % (_var_c_string(name), builder)
        )
    builder_text = "\n\n".join(builders)
    init_text = ("\n".join(initializers) if initializers
                 else "    /* 当前文法没有预声明变量。 */")
    code = r'''
/* 固定容量变量表避免在 MCU 上动态扩展注册表。 */
#ifndef PARSER_MAX_VARIABLES
#define PARSER_MAX_VARIABLES 32u
#endif

typedef struct __ParserVariable_T
{
    uint8_t used;
    char name[PARSER_MAX_NAME_LENGTH];
    VarValue_T *pValue;
} ParserVariable_T;

static ParserVariable_T s_ParserVariables[PARSER_MAX_VARIABLES];
static uint8_t s_ParserVariablesReady = 0u;

/** @brief 查找变量表项。 @param pName 名称。 @retval 表项或 NULL。 */
static ParserVariable_T *Parser_FindVariableEntry(const char *pName)
{
    uint32_t index = 0u;
    if (pName == NULL) { return NULL; }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if ((s_ParserVariables[index].used != 0u) &&
            (strcmp(s_ParserVariables[index].name, pName) == 0)) {
            return &s_ParserVariables[index];
        }
    }
    return NULL;
}

/** @brief 保存预声明变量。 @param pName 名称。 @param pValue 值。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_StoreBuiltin(const char *pName,
                                                 VarValue_T *pValue)
{
    uint32_t index = 0u;
    if ((pName == NULL) || (pValue == NULL)) { return VAR_RUNTIME_ERR_TYPE; }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if (s_ParserVariables[index].used == 0u) {
            s_ParserVariables[index].used = 1u;
            snprintf(s_ParserVariables[index].name,
                     sizeof(s_ParserVariables[index].name), "%s", pName);
            s_ParserVariables[index].pValue = pValue;
            return VAR_RUNTIME_OK;
        }
    }
    Var_Free(pValue);
    return VAR_RUNTIME_ERR_MEMORY;
}

/** @brief 初始化预声明变量。 @retval 状态码。 */
static int32_t Parser_InitVariables(void)
{
    if (s_ParserVariablesReady != 0u) { return VAR_RUNTIME_OK; }
    s_ParserVariablesReady = 1u;
__INITIALIZERS__
    return VAR_RUNTIME_OK;
}

/** @brief 查找变量值。 @param pName 名称。 @retval 值或 NULL。 */
static PARSER_UNUSED VarValue_T *Parser_FindVariable(const char *pName)
{
    ParserVariable_T *pEntry = NULL;
    Parser_InitVariables();
    pEntry = Parser_FindVariableEntry(pName);
    return (pEntry != NULL) ? pEntry->pValue : NULL;
}

/** @brief 定义变量。 @param pName 名称。 @param pValue 初值。 @param pTemplate 类型模板。 @retval 状态码。 */
static int32_t Parser_DefineVariable(const char *pName,
                                     const VarValue_T *pValue,
                                     const VarValue_T *pTemplate)
{
    VarValue_T *pNewValue = NULL;
    uint32_t index = 0u;
    int32_t ret = VAR_RUNTIME_OK;
    Parser_InitVariables();
    if ((pName == NULL) || (pValue == NULL)) { return VAR_RUNTIME_ERR_TYPE; }
    if (Parser_FindVariableEntry(pName) != NULL) { return PARSER_ERR_DUPLICATE; }
    if (pTemplate == NULL) {
        pNewValue = Var_CloneValue(pValue);
    } else {
        ret = Var_ConvertValue(pValue, pTemplate, &pNewValue);
        if (ret != VAR_RUNTIME_OK) { return ret; }
    }
    if (pNewValue == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
    for (index = 0u; index < PARSER_MAX_VARIABLES; index++) {
        if (s_ParserVariables[index].used == 0u) {
            s_ParserVariables[index].used = 1u;
            snprintf(s_ParserVariables[index].name,
                     sizeof(s_ParserVariables[index].name), "%s", pName);
            s_ParserVariables[index].pValue = pNewValue;
            return VAR_RUNTIME_OK;
        }
    }
    Var_Free(pNewValue);
    return VAR_RUNTIME_ERR_MEMORY;
}

/** @brief 固定类型赋值。 @param pName 名称。 @param pValue 新值。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_AssignVariable(
    const char *pName, const VarValue_T *pValue)
{
    ParserVariable_T *pEntry = NULL;
    VarValue_T *pConverted = NULL;
    int32_t ret = VAR_RUNTIME_OK;
    Parser_InitVariables();
    pEntry = Parser_FindVariableEntry(pName);
    if (pEntry == NULL) { return PARSER_ERR_UNDEFINED; }
    ret = Var_ConvertValue(pValue, pEntry->pValue, &pConverted);
    if (ret != VAR_RUNTIME_OK) { return ret; }
    ret = Var_SetValue(pEntry->pValue, pConverted);
    Var_Free(pConverted);
    return ret;
}
'''
    return builder_text + "\n\n" + code.replace("__INITIALIZERS__", init_text)


def _generator_lex_rules(token_defs):
    """生成按长度降序排列的固定字符串 Lexer 表。"""
    rules = []
    definition_order = 0
    for token_name, literals in token_defs.items():
        for literal in literals:
            rules.append((literal, token_name, definition_order))
            definition_order += 1
    rules.sort(key=lambda item: (-len(item[0]), item[2]))
    if not rules:
        return "    { NULL, 0u, TOKEN_END }"
    return ",\n".join(
        "    { %s, %du, TOKEN_%s }"
        % (_var_c_string(literal), len(literal.encode("utf-8")), token_name)
        for literal, token_name, _ in rules
    )


def _generator_action_cases(analysis):
    """生成每条最终产生式对应的语义动作分支。"""
    cases = []
    for prod in analysis["productions"]:
        if prod.get("transform") == "left_empty":
            continue
        action = prod.get("action", "").strip()
        translated = (_translate_action(action) if action
                      else "result.value = NULL;")
        cases.append(
            "        case %du:\n        {\n            %s\n"
            "            break;\n        }"
            % (prod["id"], translated.replace("\n", "\n            "))
        )
    return "\n".join(cases)


def _generator_expand_cases(analysis, terminals, nonterm_ids):
    """生成产生式展开分支，所有调用只操作显式分析栈。"""
    terminal_set = set(terminals)
    cases = []

    def push_symbol(symbol, slot, seed="PARSER_INVALID_INDEX"):
        """生成一个终结符或非终结符压栈语句。"""
        if symbol in terminal_set:
            kind = "PARSER_ITEM_TERMINAL"
            symbol_id = "TOKEN_" + symbol
        else:
            kind = "PARSER_ITEM_NONTERM"
            symbol_id = str(nonterm_ids[symbol]) + "u"
        return (
            "            if (Parser_Push(pCtx, %s, %s, frameIndex, "
            "%du, %s) != PARSER_OK) { return PARSER_ERR_STACK; }"
            % (kind, symbol_id, slot, seed)
        )

    for prod in analysis["productions"]:
        rhs = prod["rhs"]
        transform = prod.get("transform")
        lines = ["        case %du:" % prod["id"], "        {"]
        if transform == "left_base":
            base_length = prod["base_rhs_length"]
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_LEFT_LOOP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % nonterm_ids[rhs[-1]]
            )
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION_KEEP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
            symbols = rhs[:base_length]
            start_slot = 1
        elif transform == "left_continue":
            symbols = rhs[:-1]
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_LEFT_LOOP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % nonterm_ids[rhs[-1]]
            )
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION_KEEP, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
            start_slot = 2
        elif transform == "left_empty":
            symbols = []
            start_slot = 1
            lines.append("            /* 空尾规则由 LEFT_LOOP 直接完成。 */")
        else:
            symbols = rhs
            start_slot = 1
            lines.append(
                "            if (Parser_Push(pCtx, PARSER_ITEM_ACTION, "
                "%du, frameIndex, 0u, PARSER_INVALID_INDEX) != PARSER_OK) "
                "{ return PARSER_ERR_STACK; }" % prod["id"]
            )
        for index in range(len(symbols) - 1, -1, -1):
            lines.append(push_symbol(symbols[index], start_slot + index))
        lines.extend(["            break;", "        }"])
        cases.append("\n".join(lines))
    return "\n".join(cases)


def _generator_lexer_c():
    """返回生成式 Lexer 与 Token 前移函数的 C 模板。"""
    return r'''
/** @brief 从脚本文本产生 Token。 @param pCtx 上下文。 @param pToken 输出 Token。 @retval 状态码。 */
static int32_t Parser_LexNext(ParserCtx_T *pCtx, ParserToken_T *pToken)
{
    const char *pStart = NULL;
    char *pEnd = NULL;
    uint32_t length = 0u;
    uint32_t index = 0u;
    uint32_t outputIndex = 0u;
    uint8_t hasDot = 0u;
    uint8_t hasExponent = 0u;
    char suffix = '\0';

    while (isspace((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
        Parser_ReadChar(pCtx);
    }
    memset(pToken, 0, sizeof(*pToken));
    pToken->line = pCtx->line;
    pToken->column = pCtx->column;
    pStart = &pCtx->pText[pCtx->textPosition];
    if (*pStart == '\0') { pToken->type = TOKEN_END; return PARSER_OK; }

    /* 双引号字符串在词法阶段完成反转义。 */
    if (*pStart == '"') {
        Parser_ReadChar(pCtx);
        while ((pCtx->pText[pCtx->textPosition] != '\0') &&
               (pCtx->pText[pCtx->textPosition] != '"')) {
            char ch = Parser_ReadChar(pCtx);
            if (ch == '\\') {
                char escaped = Parser_ReadChar(pCtx);
                switch (escaped) {
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    case '0': return PARSER_ERR_LEXER;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    default: return PARSER_ERR_LEXER;
                }
            }
            if (outputIndex + 1u >= sizeof(pCtx->lexeme)) {
                return PARSER_ERR_LEXER;
            }
            pCtx->lexeme[outputIndex++] = ch;
        }
        if (pCtx->pText[pCtx->textPosition] != '"') {
            return PARSER_ERR_LEXER;
        }
        Parser_ReadChar(pCtx);
        pCtx->lexeme[outputIndex] = '\0';
        pToken->type = TOKEN_STRING;
        pToken->pValue = Var_CreateString(pCtx->lexeme);
        return (pToken->pValue != NULL) ? PARSER_OK : VAR_RUNTIME_ERR_MEMORY;
    }

    /* 小数点和指数决定 float，后缀 d 决定 double。 */
    if ((isdigit((unsigned char)*pStart) != 0) ||
        ((*pStart == '.') && (isdigit((unsigned char)pStart[1]) != 0))) {
        while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            Parser_ReadChar(pCtx);
        }
        if (pCtx->pText[pCtx->textPosition] == '.') {
            hasDot = 1u;
            Parser_ReadChar(pCtx);
            while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
                Parser_ReadChar(pCtx);
            }
        }
        if ((pCtx->pText[pCtx->textPosition] == 'e') ||
            (pCtx->pText[pCtx->textPosition] == 'E')) {
            hasExponent = 1u;
            Parser_ReadChar(pCtx);
            if ((pCtx->pText[pCtx->textPosition] == '+') ||
                (pCtx->pText[pCtx->textPosition] == '-')) {
                Parser_ReadChar(pCtx);
            }
            if (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) == 0) {
                return PARSER_ERR_LEXER;
            }
            while (isdigit((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
                Parser_ReadChar(pCtx);
            }
        }
        if (isalpha((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            suffix = Parser_ReadChar(pCtx);
        }
        if (isalpha((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) {
            return PARSER_ERR_LEXER;
        }
        length = (uint32_t)(&pCtx->pText[pCtx->textPosition] - pStart);
        if (length >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
        memcpy(pCtx->lexeme, pStart, length);
        pCtx->lexeme[length] = '\0';
        if (suffix != '\0') { pCtx->lexeme[length - 1u] = '\0'; }
        errno = 0;
        if ((suffix == 'u') || (suffix == 'U')) {
            unsigned long value = 0ul;
            if ((hasDot != 0u) || (hasExponent != 0u)) {
                return PARSER_ERR_LEXER;
            }
            value = strtoul(pCtx->lexeme, &pEnd, 10);
            if ((errno == ERANGE) || (*pEnd != '\0') || (value > UINT32_MAX)) {
                return VAR_RUNTIME_ERR_RANGE;
            }
            pToken->pValue = Var_CreateUint32((uint32_t)value);
        } else if ((suffix == 'd') || (suffix == 'D')) {
            double value = 0.0;
            if ((hasDot == 0u) && (hasExponent == 0u)) {
                return PARSER_ERR_LEXER;
            }
            value = strtod(pCtx->lexeme, &pEnd);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (isfinite(value) == 0)) { return VAR_RUNTIME_ERR_RANGE; }
            pToken->pValue = Var_CreateDouble(value);
        } else if (suffix != '\0') {
            return PARSER_ERR_LEXER;
        } else if ((hasDot != 0u) || (hasExponent != 0u)) {
            float value = strtof(pCtx->lexeme, &pEnd);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (isfinite(value) == 0)) { return VAR_RUNTIME_ERR_RANGE; }
            pToken->pValue = Var_CreateFloat(value);
        } else {
            long value = strtol(pCtx->lexeme, &pEnd, 10);
            if ((errno == ERANGE) || (*pEnd != '\0') ||
                (value < INT32_MIN) || (value > INT32_MAX)) {
                return VAR_RUNTIME_ERR_RANGE;
            }
            pToken->pValue = Var_CreateInt((int32_t)value);
        }
        pToken->type = TOKEN_NUMBER;
        return (pToken->pValue != NULL) ? PARSER_OK : VAR_RUNTIME_ERR_MEMORY;
    }

    /* 固定规则按最长候选优先，并在关键字末尾检查标识符边界。 */
    for (index = 0u; index < (uint32_t)(sizeof(s_ParserLexRules) /
                                        sizeof(s_ParserLexRules[0])); index++) {
        const ParserLexRule_T *pRule = &s_ParserLexRules[index];
        if ((pRule->pText != NULL) &&
            (strncmp(pStart, pRule->pText, pRule->length) == 0)) {
            unsigned char last = (unsigned char)pRule->pText[pRule->length - 1u];
            unsigned char next = (unsigned char)pStart[pRule->length];
            if (((isalnum(last) != 0) || (last == '_')) &&
                ((isalnum(next) != 0) || (next == '_'))) {
                continue;
            }
            for (length = 0u; length < pRule->length; length++) {
                Parser_ReadChar(pCtx);
            }
            pToken->type = pRule->type;
            return PARSER_OK;
        }
    }

    /* C 风格标识符默认映射到 VARIABLE。 */
    if ((isalpha((unsigned char)*pStart) != 0) || (*pStart == '_')) {
        while ((isalnum((unsigned char)pCtx->pText[pCtx->textPosition]) != 0) ||
               (pCtx->pText[pCtx->textPosition] == '_')) {
            if (length + 1u >= sizeof(pCtx->lexeme)) { return PARSER_ERR_LEXER; }
            pCtx->lexeme[length++] = Parser_ReadChar(pCtx);
        }
        pCtx->lexeme[length] = '\0';
        pToken->type = TOKEN_VARIABLE;
        pToken->pName = pCtx->lexeme;
        return PARSER_OK;
    }

    return PARSER_ERR_LEXER;
}

/** @brief 前移 Token。 @param pCtx 上下文。 @retval 状态码。 */
static int32_t Parser_Advance(ParserCtx_T *pCtx)
{
    int32_t ret = PARSER_OK;
    if ((pCtx->ownsCurrentValue != 0u) && (pCtx->current.pValue != NULL)) {
        Var_Free(pCtx->current.pValue);
    }
    pCtx->ownsCurrentValue = 0u;
    if (pCtx->textMode != 0u) {
        ret = Parser_LexNext(pCtx, &pCtx->current);
        if (ret != PARSER_OK) {
            Parser_SetError(ret, pCtx->current.line, pCtx->current.column,
                            "词法解析失败");
            pCtx->current.type = TOKEN_INVALID;
            return ret;
        }
        pCtx->ownsCurrentValue = (pCtx->current.pValue != NULL) ? 1u : 0u;
    } else if (pCtx->tokenPosition < pCtx->tokenCount) {
        pCtx->current = pCtx->pTokens[pCtx->tokenPosition++];
    } else {
        memset(&pCtx->current, 0, sizeof(pCtx->current));
        pCtx->current.type = TOKEN_END;
    }
    return PARSER_OK;
}
'''


def _generator_parse_loop_c():
    """返回显式分析栈主循环和两个公开入口。"""
    return r'''
/** @brief 执行非递归预测分析。 @param pCtx 上下文。 @retval 结果或 NULL。 */
static VarValue_T *Parser_ParseLoop(ParserCtx_T *pCtx)
{
    if (Parser_Push(pCtx, PARSER_ITEM_NONTERM, __START_ID__u,
                    PARSER_INVALID_INDEX, 0u,
                    PARSER_INVALID_INDEX) != PARSER_OK) {
        return NULL;
    }
    while (pCtx->stackTop > 0u) {
        ParserStackItem_T item = pCtx->stack[--pCtx->stackTop];
        if (item.kind == PARSER_ITEM_TERMINAL) {
            if ((uint16_t)pCtx->current.type != item.symbol) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "终结符不匹配");
                return NULL;
            }
            if (Parser_CaptureTerminal(pCtx, item.frameIndex,
                                       item.valueIndex) != PARSER_OK) {
                return NULL;
            }
            if (Parser_Advance(pCtx) != PARSER_OK) { return NULL; }
        } else if (item.kind == PARSER_ITEM_NONTERM) {
            int16_t production = -1;
            uint16_t frameIndex = 0u;
            ParserSemanticFrame_T *pFrame = NULL;
            if (((uint16_t)pCtx->current.type >= __TERMINAL_COUNT__u) ||
                (item.symbol >= __NONTERM_COUNT__u)) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "没有可用产生式");
                return NULL;
            }
            production = s_ParserTable[item.symbol][pCtx->current.type];
            if (production < 0) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "预测分析表没有匹配项");
                return NULL;
            }
            if (pCtx->frameTop >= PARSER_MAX_STACK_DEPTH) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "语义帧栈已满");
                return NULL;
            }
            frameIndex = pCtx->frameTop++;
            pFrame = &pCtx->frames[frameIndex];
            memset(pFrame, 0, sizeof(*pFrame));
            pFrame->parentFrameIndex = item.frameIndex;
            pFrame->parentValueIndex = item.valueIndex;
            if (item.seedFrameIndex != PARSER_INVALID_INDEX) {
                pFrame->values[1] =
                    pCtx->frames[item.seedFrameIndex].pResult;
            }
            if (Parser_ExpandProduction(pCtx, (uint16_t)production,
                                        frameIndex) != PARSER_OK) {
                Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                pCtx->current.column, "分析栈已满");
                return NULL;
            }
        } else if (item.kind == PARSER_ITEM_LEFT_LOOP) {
            int16_t production = -1;
            ParserSemanticFrame_T *pFrame = &pCtx->frames[item.frameIndex];
            if (((uint16_t)pCtx->current.type >= __TERMINAL_COUNT__u) ||
                (item.symbol >= __NONTERM_COUNT__u)) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column,
                                "左递归尾部没有可用产生式");
                return NULL;
            }
            production = s_ParserTable[item.symbol][pCtx->current.type];
            if (production < 0) {
                Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                                pCtx->current.column, "左递归尾部预测失败");
                return NULL;
            }
            if (s_ParserProductionIsLeftEmpty[production] != 0u) {
                if (Parser_CompleteFrame(pCtx, item.frameIndex) != PARSER_OK) {
                    return NULL;
                }
            } else {
                if (pFrame->pResult != NULL) {
                    pFrame->values[1] = Var_CloneValue(pFrame->pResult);
                    if (pFrame->values[1] == NULL) { return NULL; }
                    pFrame->valueOwned[1] = 1u;
                }
                if (Parser_ExpandProduction(pCtx, (uint16_t)production,
                                            item.frameIndex) != PARSER_OK) {
                    Parser_SetError(PARSER_ERR_STACK, pCtx->current.line,
                                    pCtx->current.column, "分析栈已满");
                    return NULL;
                }
            }
        } else {
            ParserSemanticFrame_T *pFrame = &pCtx->frames[item.frameIndex];
            if (item.kind == PARSER_ITEM_FORWARD) {
                if (Parser_ForwardFrame(pFrame, item.valueIndex) != PARSER_OK) {
                    return NULL;
                }
            } else if (Parser_ExecuteAction(pCtx, item.symbol,
                                            item.frameIndex) != PARSER_OK) {
                return NULL;
            }
            if ((item.kind != PARSER_ITEM_ACTION_KEEP) &&
                (Parser_CompleteFrame(pCtx, item.frameIndex) != PARSER_OK)) {
                return NULL;
            }
        }
    }
    if (pCtx->current.type != TOKEN_END) {
        Parser_SetError(PARSER_ERR_SYNTAX, pCtx->current.line,
                        pCtx->current.column, "输入尚未完全消费");
        return NULL;
    }
    return pCtx->pRootResult;
}

/** @brief 解析 Token 数组。 @param pTokens Token 数组。 @param count 数量。 @retval 结果或 NULL。 */
VarValue_T *Parser_Run(const ParserToken_T *pTokens, uint32_t count)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pTokens = pTokens;
    pContext->tokenCount = count;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoop(pContext);
    if (pResult == NULL) { Parser_ReleaseContext(pContext); }
    return pResult;
}

/** @brief 解析脚本文本。 @param pText 输入文本。 @retval 结果或 NULL。 */
VarValue_T *Parser_RunText(const char *pText)
{
    ParserCtx_T *pContext = &s_ParserContext;
    VarValue_T *pResult = NULL;
    if (pText == NULL) { return NULL; }
    memset(pContext, 0, sizeof(*pContext));
    memset(&s_ParserLastError, 0, sizeof(s_ParserLastError));
    pContext->pText = pText;
    pContext->textMode = 1u;
    pContext->line = 1u;
    pContext->column = 1u;
    if (Parser_Advance(pContext) != PARSER_OK) { return NULL; }
    pResult = Parser_ParseLoop(pContext);
    if ((pContext->ownsCurrentValue != 0u) &&
        (pContext->current.pValue != NULL)) {
        Var_Free(pContext->current.pValue);
    }
    if (pResult == NULL) { Parser_ReleaseContext(pContext); }
    return pResult;
}
'''


def generate_parser_c(grammar, tokens, variables=None, token_defs=None):
    """生成带默认 Lexer 的表驱动非递归 LL(1) C 解析器。"""
    variables = variables or {}
    token_defs = token_defs or {}
    analysis = prepare_grammar(grammar, tokens)
    terminals = analysis["terminals"]
    nonterms = analysis["nonterms"]
    nonterm_ids = {name: index for index, name in enumerate(nonterms)}
    max_slots = max(
        [len(prod["rhs"]) + 3 for prod in analysis["productions"]] + [4]
    )
    table_rows = []
    for nonterm in nonterms:
        row = [str(analysis["table"].get((nonterm, token), -1))
               for token in terminals]
        table_rows.append("    { %s }" % ", ".join(row))
    token_enum = ", ".join("TOKEN_" + token for token in terminals)
    token_enum += ", TOKEN_INVALID"
    replacements = {
        "__TOKEN_ENUM__": token_enum,
        "__MAX_SLOTS__": str(max_slots),
        "__NONTERM_COUNT__": str(len(nonterms)),
        "__TERMINAL_COUNT__": str(len(terminals)),
        "__TABLE_ROWS__": ",\n".join(table_rows),
        "__PRODUCTION_LEFT_EMPTY__": ", ".join(
            "1u" if prod.get("transform") == "left_empty" else "0u"
            for prod in analysis["productions"]
        ),
        "__LEX_RULES__": _generator_lex_rules(token_defs),
        "__REGISTRY__": _generator_registry_block(variables),
        "__EXPAND_CASES__": _generator_expand_cases(
            analysis, terminals, nonterm_ids
        ),
        "__ACTION_CASES__": _generator_action_cases(analysis),
        "__START_ID__": str(nonterm_ids[analysis["grammar"]["start"]]),
    }
    code = r'''/* 由 Chapter-script-engine/parser_gen.py 自动生成：非递归 LL(1) 解析器 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "variable_runtime.h"

#ifndef PARSER_MAX_STACK_DEPTH
#define PARSER_MAX_STACK_DEPTH 64u
#endif
#ifndef PARSER_MAX_NAME_LENGTH
#define PARSER_MAX_NAME_LENGTH 64u
#endif
#ifndef PARSER_MAX_STRING_LENGTH
#define PARSER_MAX_STRING_LENGTH 128u
#endif

#define PARSER_OK 0
#define PARSER_ERR_STACK -20
#define PARSER_ERR_SYNTAX -21
#define PARSER_ERR_LEXER -22
#define PARSER_ERR_DUPLICATE -23
#define PARSER_ERR_UNDEFINED -24
#define PARSER_INVALID_INDEX UINT16_MAX
#define PARSER_VALUE_SLOTS __MAX_SLOTS__u
#if defined(__GNUC__)
#define PARSER_UNUSED __attribute__((unused))
#else
#define PARSER_UNUSED
#endif

typedef enum __ParserTokenType_T { __TOKEN_ENUM__ } ParserTokenType_T;

typedef struct __ParserToken_T
{
    ParserTokenType_T type;
    int32_t value;
    const char *name;
    VarValue_T *pValue;
    const char *pName;
    uint32_t line;
    uint32_t column;
} ParserToken_T;

typedef struct __ParserError_T
{
    int32_t code;
    uint32_t line;
    uint32_t column;
    char message[96];
} ParserError_T;

typedef struct __ParseResult_T
{
    VarValue_T *value;
} ParseResult_T;

typedef enum __ParserItemKind_T
{
    PARSER_ITEM_TERMINAL = 0,
    PARSER_ITEM_NONTERM,
    PARSER_ITEM_ACTION,
    PARSER_ITEM_ACTION_KEEP,
    PARSER_ITEM_FORWARD,
    PARSER_ITEM_LEFT_LOOP
} ParserItemKind_T;

typedef struct __ParserStackItem_T
{
    ParserItemKind_T kind;
    uint16_t symbol;
    uint16_t frameIndex;
    uint16_t valueIndex;
    uint16_t seedFrameIndex;
} ParserStackItem_T;

typedef struct __ParserSemanticFrame_T
{
    uint16_t parentFrameIndex;
    uint16_t parentValueIndex;
    VarValue_T *values[PARSER_VALUE_SLOTS];
    uint8_t valueOwned[PARSER_VALUE_SLOTS];
    char names[PARSER_VALUE_SLOTS][PARSER_MAX_NAME_LENGTH];
    VarValue_T *pResult;
    uint8_t resultOwned;
} ParserSemanticFrame_T;

typedef struct __ParserCtx_T
{
    const ParserToken_T *pTokens;
    uint32_t tokenCount;
    uint32_t tokenPosition;
    const char *pText;
    uint32_t textPosition;
    uint32_t line;
    uint32_t column;
    uint8_t textMode;
    uint8_t ownsCurrentValue;
    char lexeme[PARSER_MAX_STRING_LENGTH];
    ParserToken_T current;
    ParserStackItem_T stack[PARSER_MAX_STACK_DEPTH];
    uint16_t stackTop;
    ParserSemanticFrame_T frames[PARSER_MAX_STACK_DEPTH];
    uint16_t frameTop;
    VarValue_T *pRootResult;
} ParserCtx_T;

typedef struct __ParserLexRule_T
{
    const char *pText;
    uint32_t length;
    ParserTokenType_T type;
} ParserLexRule_T;

static ParserError_T s_ParserLastError;
static ParserCtx_T s_ParserContext;
static const int16_t s_ParserTable[__NONTERM_COUNT__][__TERMINAL_COUNT__] =
{
__TABLE_ROWS__
};
static const uint8_t s_ParserProductionIsLeftEmpty[] =
{
    __PRODUCTION_LEFT_EMPTY__
};
static const ParserLexRule_T s_ParserLexRules[] =
{
__LEX_RULES__
};

__REGISTRY__

/** @brief 设置最近错误。 @param code 错误码。 @param line 行。 @param column 列。 @param pMessage 说明。 @retval 无。 */
static void Parser_SetError(int32_t code, uint32_t line, uint32_t column,
                            const char *pMessage)
{
    s_ParserLastError.code = code;
    s_ParserLastError.line = line;
    s_ParserLastError.column = column;
    snprintf(s_ParserLastError.message, sizeof(s_ParserLastError.message),
             "%s", (pMessage != NULL) ? pMessage : "解析错误");
}

/** @brief 获取最近错误。 @retval 只读错误结构。 */
const ParserError_T *Parser_GetLastError(void)
{
    return &s_ParserLastError;
}

/** @brief 创建简单类型模板。 @param type 类型。 @retval 新模板或 NULL。 */
static VarValue_T *Parser_CreateScalarTemplate(VarType_T type)
{
    switch (type) {
        case VAR_TYPE_INT: return Var_CreateInt(0);
        case VAR_TYPE_UINT8: return Var_CreateUint8(0u);
        case VAR_TYPE_INT8: return Var_CreateInt8(0);
        case VAR_TYPE_UINT16: return Var_CreateUint16(0u);
        case VAR_TYPE_INT16: return Var_CreateInt16(0);
        case VAR_TYPE_UINT32: return Var_CreateUint32(0u);
        case VAR_TYPE_UINT64: return Var_CreateUint64(0u);
        case VAR_TYPE_INT64: return Var_CreateInt64(0);
        case VAR_TYPE_FLOAT: return Var_CreateFloat(0.0f);
        case VAR_TYPE_DOUBLE: return Var_CreateDouble(0.0);
        case VAR_TYPE_CHAR: return Var_CreateChar('\0');
        case VAR_TYPE_STRING: return Var_CreateString("");
        default: return NULL;
    }
}

/** @brief 创建数组类型模板。 @param type 元素类型。 @param pDimensions 维度。 @param count 维数。 @param level 当前层。 @retval 新模板或 NULL。 */
static VarValue_T *Parser_CreateArrayTemplate(VarType_T type,
                                              const uint32_t *pDimensions,
                                              uint32_t count,
                                              uint32_t level)
{
    VarValue_T *pArray = NULL;
    uint32_t index = 0u;
    if (level >= count) { return Parser_CreateScalarTemplate(type); }
    pArray = Var_CreateArray();
    if (pArray == NULL) { return NULL; }
    for (index = 0u; index < pDimensions[level]; index++) {
        VarValue_T *pItem = Parser_CreateArrayTemplate(
            type, pDimensions, count, level + 1u
        );
        if ((pItem == NULL) ||
            (Var_ArrayPush(pArray, pItem) != VAR_RUNTIME_OK)) {
            Var_Free(pItem);
            Var_Free(pArray);
            return NULL;
        }
    }
    return pArray;
}

/** @brief 按显式类型定义变量。 @param pName 名称。 @param pValue 初值。 @param type 类型。 @param pDimensions 维度。 @param count 维数。 @retval 状态码。 */
static PARSER_UNUSED int32_t Parser_DefineTyped(
    const char *pName, const VarValue_T *pValue, VarType_T type,
    const uint32_t *pDimensions, uint32_t count)
{
    VarValue_T *pTemplate = NULL;
    int32_t ret = VAR_RUNTIME_OK;
    if (count == 0u) {
        pTemplate = Parser_CreateScalarTemplate(type);
    } else if (pDimensions != NULL) {
        pTemplate = Parser_CreateArrayTemplate(type, pDimensions, count, 0u);
    }
    if (pTemplate == NULL) { return VAR_RUNTIME_ERR_TYPE; }
    ret = Parser_DefineVariable(pName, pValue, pTemplate);
    Var_Free(pTemplate);
    return ret;
}

/** @brief 读取字符并维护行列号。 @param pCtx 上下文。 @retval 当前字符。 */
static char Parser_ReadChar(ParserCtx_T *pCtx)
{
    char ch = pCtx->pText[pCtx->textPosition++];
    if (ch == '\n') { pCtx->line++; pCtx->column = 1u; }
    else { pCtx->column++; }
    return ch;
}
'''
    code += _generator_lexer_c()
    code += r'''
/** @brief 压入分析项目。 @retval 状态码。 */
static int32_t Parser_Push(ParserCtx_T *pCtx, ParserItemKind_T kind,
                           uint16_t symbol, uint16_t frameIndex,
                           uint16_t valueIndex, uint16_t seedFrameIndex)
{
    ParserStackItem_T *pItem = NULL;
    if (pCtx->stackTop >= PARSER_MAX_STACK_DEPTH) { return PARSER_ERR_STACK; }
    pItem = &pCtx->stack[pCtx->stackTop++];
    pItem->kind = kind;
    pItem->symbol = symbol;
    pItem->frameIndex = frameIndex;
    pItem->valueIndex = valueIndex;
    pItem->seedFrameIndex = seedFrameIndex;
    return PARSER_OK;
}

/** @brief 展开产生式。 @retval 状态码。 */
static int32_t Parser_ExpandProduction(ParserCtx_T *pCtx,
                                       uint16_t production,
                                       uint16_t frameIndex)
{
    switch (production) {
__EXPAND_CASES__
        default: return PARSER_ERR_SYNTAX;
    }
    return PARSER_OK;
}

/** @brief 执行语义动作。 @retval 状态码。 */
static int32_t Parser_ExecuteAction(ParserCtx_T *pCtx,
                                    uint16_t production,
                                    uint16_t frameIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    VarValue_T **values = pFrame->values;
    const char *names[PARSER_VALUE_SLOTS] = { 0 };
    ParseResult_T result = { 0 };
    int32_t actionStatus = PARSER_OK;
    uint32_t index = 0u;
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        names[index] = pFrame->names[index];
    }
    (void)values;
    (void)names;
    result.value = pFrame->pResult;
    switch (production) {
__ACTION_CASES__
        default: return PARSER_ERR_SYNTAX;
    }
    if (actionStatus != PARSER_OK) {
        if ((result.value != NULL) && (result.value != pFrame->pResult)) {
            Var_Free(result.value);
        }
        Parser_SetError(actionStatus, pCtx->current.line,
                        pCtx->current.column, "变量定义或赋值失败");
        return actionStatus;
    }
    if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL) &&
        (pFrame->pResult != result.value)) {
        Var_Free(pFrame->pResult);
    }
    pFrame->resultOwned = (result.value != NULL) ? 1u : 0u;
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        if (pFrame->values[index] == result.value) {
            if (pFrame->valueOwned[index] != 0u) {
                pFrame->valueOwned[index] = 0u;
            } else if (result.value != NULL) {
                result.value = Var_CloneValue(result.value);
                if (result.value == NULL) { return VAR_RUNTIME_ERR_MEMORY; }
            }
            break;
        }
    }
    for (index = 0u; index < PARSER_VALUE_SLOTS; index++) {
        if ((pFrame->valueOwned[index] != 0u) &&
            (pFrame->values[index] != NULL)) {
            Var_Free(pFrame->values[index]);
        }
        pFrame->values[index] = NULL;
        pFrame->valueOwned[index] = 0u;
    }
    pFrame->pResult = result.value;
    return PARSER_OK;
}

/** @brief 转发语义槽并处理所有权。 @retval 状态码。 */
static int32_t Parser_ForwardFrame(ParserSemanticFrame_T *pFrame,
                                   uint16_t valueIndex)
{
    VarValue_T *pValue = pFrame->values[valueIndex];
    uint8_t owned = pFrame->valueOwned[valueIndex];
    if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL) &&
        (pFrame->pResult != pValue)) {
        Var_Free(pFrame->pResult);
    }
    pFrame->pResult = pValue;
    pFrame->resultOwned = owned;
    pFrame->valueOwned[valueIndex] = 0u;
    return PARSER_OK;
}

/** @brief 完成语义帧。 @retval 状态码。 */
static int32_t Parser_CompleteFrame(ParserCtx_T *pCtx, uint16_t frameIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    if ((pCtx->frameTop == 0u) ||
        (frameIndex != (uint16_t)(pCtx->frameTop - 1u))) {
        return PARSER_ERR_STACK;
    }
    if (pFrame->parentFrameIndex == PARSER_INVALID_INDEX) {
        pCtx->pRootResult = pFrame->pResult;
        pFrame->resultOwned = 0u;
    } else {
        pCtx->frames[pFrame->parentFrameIndex]
            .values[pFrame->parentValueIndex] = pFrame->pResult;
        pCtx->frames[pFrame->parentFrameIndex]
            .valueOwned[pFrame->parentValueIndex] = pFrame->resultOwned;
        pFrame->resultOwned = 0u;
    }
    memset(pFrame, 0, sizeof(*pFrame));
    pCtx->frameTop--;
    return PARSER_OK;
}

/** @brief 释放失败路径仍由语义帧持有的值。 @retval 无。 */
static void Parser_ReleaseContext(ParserCtx_T *pCtx)
{
    uint16_t frameIndex = 0u;
    uint16_t valueIndex = 0u;
    for (frameIndex = 0u; frameIndex < pCtx->frameTop; frameIndex++) {
        ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
        for (valueIndex = 0u; valueIndex < PARSER_VALUE_SLOTS; valueIndex++) {
            if ((pFrame->valueOwned[valueIndex] != 0u) &&
                (pFrame->values[valueIndex] != NULL)) {
                Var_Free(pFrame->values[valueIndex]);
            }
        }
        if ((pFrame->resultOwned != 0u) && (pFrame->pResult != NULL)) {
            Var_Free(pFrame->pResult);
        }
        memset(pFrame, 0, sizeof(*pFrame));
    }
    pCtx->frameTop = 0u;
    pCtx->stackTop = 0u;
}

/** @brief 捕获终结符语义。 @retval 状态码。 */
static int32_t Parser_CaptureTerminal(ParserCtx_T *pCtx,
                                      uint16_t frameIndex,
                                      uint16_t valueIndex)
{
    ParserSemanticFrame_T *pFrame = &pCtx->frames[frameIndex];
    if (valueIndex >= PARSER_VALUE_SLOTS) { return PARSER_ERR_STACK; }
    if ((pCtx->current.pName != NULL) || (pCtx->current.name != NULL)) {
        snprintf(pFrame->names[valueIndex],
                 sizeof(pFrame->names[valueIndex]), "%s",
                 (pCtx->current.pName != NULL) ? pCtx->current.pName :
                                                pCtx->current.name);
    }
    if (pCtx->current.pValue != NULL) {
        pFrame->values[valueIndex] = Var_CloneValue(pCtx->current.pValue);
        if (pFrame->values[valueIndex] == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        pFrame->valueOwned[valueIndex] = 1u;
    } else if (pCtx->current.type == TOKEN_NUMBER) {
        pFrame->values[valueIndex] = Var_CreateInt(pCtx->current.value);
        if (pFrame->values[valueIndex] == NULL) {
            return VAR_RUNTIME_ERR_MEMORY;
        }
        pFrame->valueOwned[valueIndex] = 1u;
    }
    return PARSER_OK;
}
'''
    code += _generator_parse_loop_c()
    for marker, value in replacements.items():
        code = code.replace(marker, value)
    return code


def main():
    parser = argparse.ArgumentParser(
        description="Yacc 风格非递归 LL(1) 解析器生成器"
    )
    parser.add_argument("-o", "--output", default="parser_gen.c",
                        help="输出 C 文件名")
    parser.add_argument("--grammar", default=None,
                        help="可选 JSON 文法文件")
    parser.add_argument("--grammar-file", default=None,
                        help="可选 .gram 文本文法文件")
    parser.add_argument("--advanced", action="store_true",
                        help="生成 IF/FOR/布尔运算符高级示例文法")
    parser.add_argument("--report", default=None,
                        help="把文法分析报告另存为 UTF-8 文本")
    parser.add_argument("--self-test", action="store_true",
                        help="运行生成器内置回归测试")
    args = parser.parse_args()

    # 内置测试不生成用户文件，便于单文件工具随时自检
    if args.self_test:
        cfg = load_grammar_text(
            '%define EQ "==|="\n'
            '%start root\n'
            'root := NUMBER { $$ = $1; }\n'
        )
        assert cfg["tokens"][:3] == [
            "STRING", "NUMBER", "VARIABLE"
        ]
        assert cfg["token_defs"]["EQ"] == ["==", "="]
        analysis = prepare_grammar(cfg["grammar"], cfg["tokens"])
        report = format_grammar_report(analysis)
        assert "[最终产生式]" in report
        assert "FIRST(root) = { NUMBER }" in report
        assert "FOLLOW(root) = { END }" in report
        assert "M[root, NUMBER]" in report
        try:
            load_grammar_text(
                '%define A "+"\n%define B "+"\n'
                '%start root\nroot := NUMBER { $$ = $1; }\n'
            )
            raise AssertionError("重复固定字面量没有被拒绝")
        except GrammarLoadError:
            pass
        nullable_cycle_cfg = load_grammar_text(
            '%start a\n'
            'a := b c { $$ = $2; }\n'
            'b := %empty { $$ = int32(0); }\n'
            'c := a { $$ = $1; }\n'
        )
        try:
            prepare_grammar(
                nullable_cycle_cfg["grammar"], nullable_cycle_cfg["tokens"]
            )
            raise AssertionError("可空前缀后的间接左递归没有被拒绝")
        except ValueError as error:
            assert "间接左递归" in str(error)
        generated_code = generate_parser_c(
            cfg["grammar"], cfg["tokens"], {}, cfg["token_defs"]
        )
        assert "Parser_ParseLoop" in generated_code
        assert "static ParseResult_T Parse_" not in generated_code
        assert "Parser_RunText" in generated_code
        assert "static ParserCtx_T s_ParserContext;" in generated_code
        assert "ParserCtx_T context;" not in generated_code
        _compile_and_run_generated_test(generated_code, r'''
/**
 * @file    generated_harness.c
 * @brief   验证生成式 Lexer 的数字动态类型。
 */
#include "generated_parser.c"

/** @brief 执行动态数字测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("1");
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_INT)) { return 1; }
    Var_Free(pValue);
    pValue = Parser_RunText("1u");
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_UINT32)) { return 2; }
    Var_Free(pValue);
    pValue = Parser_RunText("1.2");
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_FLOAT)) { return 3; }
    Var_Free(pValue);
    pValue = Parser_RunText("1e3d");
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_DOUBLE)) { return 4; }
    Var_Free(pValue);
    if (Parser_RunText("1.2u") != NULL) { return 5; }
    if ((Parser_GetLastError()->code != PARSER_ERR_LEXER) ||
        (Parser_GetLastError()->line != 1u) ||
        (Parser_GetLastError()->column != 1u)) { return 6; }
    return 0;
}
''')
        _compile_and_run_generated_test(generated_code, r'''
/** @file generated_harness.c @brief 验证旧版 Token 数组源码兼容性。 */
#include "generated_parser.c"
/** @brief 使用旧 value/name 字段解析数字。 @retval 0 表示通过。 */
int main(void)
{
    ParserToken_T tokens[1] = { 0 };
    VarValue_T *pValue = NULL;
    int32_t result = 0;
    tokens[0].type = TOKEN_NUMBER;
    tokens[0].value = 7;
    tokens[0].name = "legacy";
    pValue = Parser_Run(tokens, 1u);
    if ((pValue == NULL) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 7)) { return 1; }
    Var_Free(pValue);
    return 0;
}
''')
        expression_cfg = load_grammar_text(
            '%define PLUS "+"\n'
            '%define EQ "==|="\n'
            '%start expr\n'
            'expr := expr PLUS atom { $$ = $1 + $3; }\n'
            'expr := atom { $$ = $1; }\n'
            'atom := NUMBER { $$ = $1; }\n'
        )
        expression_code = generate_parser_c(
            expression_cfg["grammar"], expression_cfg["tokens"], {},
            expression_cfg["token_defs"]
        )
        _compile_and_run_generated_test(expression_code, r'''
/** @file generated_harness.c @brief 验证直接左递归的非递归执行。 */
#include "generated_parser.c"

/** @brief 执行表达式测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1");
    int32_t result = 0;
    if ((pValue == NULL) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 200)) { return 1; }
    Var_Free(pValue);
    return 0;
}
''', sanitize=True)
        string_cfg = load_grammar_text(
            '%start root\nroot := STRING { $$ = $1; }\n'
        )
        string_code = generate_parser_c(
            string_cfg["grammar"], string_cfg["tokens"], {},
            string_cfg["token_defs"]
        )
        _compile_and_run_generated_test(string_code, r'''
/** @file generated_harness.c @brief 验证默认字符串 Token。 */
#include <string.h>
#include "generated_parser.c"

/** @brief 执行字符串测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("\"a\\n\"");
    const char *pText = NULL;
    if ((pValue == NULL) ||
        (Var_GetString(pValue, &pText) != VAR_RUNTIME_OK) ||
        (strcmp(pText, "a\n") != 0)) { return 1; }
    Var_Free(pValue);
    if (Parser_RunText("\"a\\0b\"") != NULL) { return 2; }
    if (Parser_GetLastError()->code != PARSER_ERR_LEXER) { return 3; }
    return 0;
}
''')
        define_cfg = load_grammar_text(
            '%define ASSIGN ":="\n'
            '%start root\n'
            'root := VARIABLE ASSIGN NUMBER '
            '{ def[$1, int32] = $3; $$ = [$1]; }\n'
        )
        define_code = generate_parser_c(
            define_cfg["grammar"], define_cfg["tokens"], {},
            define_cfg["token_defs"]
        )
        _compile_and_run_generated_test(define_code, r'''
/** @file generated_harness.c @brief 验证变量定义和重复定义错误。 */
#include "generated_parser.c"

/** @brief 执行变量定义测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("x:=1.9");
    int32_t result = 0;
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_INT) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 1)) { return 1; }
    if (Parser_RunText("x:=2") != NULL) { return 2; }
    if (Parser_GetLastError()->code != PARSER_ERR_DUPLICATE) { return 3; }
    return 0;
}
''')
        assign_cfg = load_grammar_text(
            '%define ASSIGN ":="\n'
            '%start root\n'
            '%var x int32 = 0\n'
            'root := VARIABLE ASSIGN NUMBER '
            '{ [$1] = $3; $$ = [$1]; }\n'
        )
        assign_code = generate_parser_c(
            assign_cfg["grammar"], assign_cfg["tokens"],
            assign_cfg["variables"], assign_cfg["token_defs"]
        )
        _compile_and_run_generated_test(assign_code, r'''
/** @file generated_harness.c @brief 验证固定类型赋值。 */
#include "generated_parser.c"

/** @brief 执行固定类型赋值测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("x:=2.8");
    int32_t result = 0;
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_INT) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 2)) { return 1; }
    if (Parser_RunText("y:=2") != NULL) { return 2; }
    if (Parser_GetLastError()->code != PARSER_ERR_UNDEFINED) { return 3; }
    return 0;
}
''')
        action_error_cfg = load_grammar_text(
            '%start root\n%var x int32 = 0\n'
            'root := VARIABLE NUMBER '
            '{ def[$1] = $2; [$1] = $2; $$ = [$1]; }\n'
        )
        action_error_code = generate_parser_c(
            action_error_cfg["grammar"], action_error_cfg["tokens"],
            action_error_cfg["variables"], action_error_cfg["token_defs"]
        )
        _compile_and_run_generated_test(action_error_code, r'''
/** @file generated_harness.c @brief 验证语义动作保留首个错误。 */
#include "generated_parser.c"
/** @brief 执行多动作错误测试。 @retval 0 表示通过。 */
int main(void)
{
    if (Parser_RunText("x 2") != NULL) { return 1; }
    if (Parser_GetLastError()->code != PARSER_ERR_DUPLICATE) { return 2; }
    return 0;
}
''')
        longest_cfg = load_grammar_text(
            '%define EQ "==|="\n%start root\n'
            'root := NUMBER EQ NUMBER { $$ = $1; }\n'
        )
        longest_code = generate_parser_c(
            longest_cfg["grammar"], longest_cfg["tokens"], {},
            longest_cfg["token_defs"]
        )
        _compile_and_run_generated_test(longest_code, r'''
/** @file generated_harness.c @brief 验证固定运算符最长匹配。 */
#include "generated_parser.c"
/** @brief 执行最长匹配测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pFirst = Parser_RunText("1==2");
    VarValue_T *pSecond = Parser_RunText("1=2");
    if ((pFirst == NULL) || (pSecond == NULL)) { return 1; }
    Var_Free(pFirst);
    Var_Free(pSecond);
    return 0;
}
''')
        keyword_cfg = load_grammar_text(
            '%define IF "if"\n%start root\n'
            'root := IF { $$ = int32(1); }\n'
        )
        keyword_code = generate_parser_c(
            keyword_cfg["grammar"], keyword_cfg["tokens"], {},
            keyword_cfg["token_defs"]
        )
        _compile_and_run_generated_test(keyword_code, r'''
/** @file generated_harness.c @brief 验证关键字优先级与标识符边界。 */
#include "generated_parser.c"
/** @brief 执行关键字边界测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("if");
    if (pValue == NULL) { return 1; }
    Var_Free(pValue);
    if (Parser_RunText("ifx") != NULL) { return 2; }
    if (Parser_GetLastError()->code != PARSER_ERR_SYNTAX) { return 3; }
    return 0;
}
''')
        type_copy_cfg = load_grammar_text(
            '%start root\n'
            'root := VARIABLE NUMBER NUMBER '
            '{ def[$1, $2] = $3; $$ = [$1]; }\n'
        )
        type_copy_code = generate_parser_c(
            type_copy_cfg["grammar"], type_copy_cfg["tokens"], {},
            type_copy_cfg["token_defs"]
        )
        _compile_and_run_generated_test(type_copy_code, r'''
/** @file generated_harness.c @brief 验证从语义值复制类型。 */
#include "generated_parser.c"
/** @brief 执行类型复制测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("typed 0.0 2");
    float result = 0.0f;
    if ((pValue == NULL) || (pValue->type != VAR_TYPE_FLOAT) ||
        (Var_GetFloat(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 2.0f)) { return 1; }
    return 0;
}
''')
        array_cfg = load_grammar_text(
            '%token SOURCE\n%start root\n'
            'root := VARIABLE SOURCE '
            '{ def[$1, int32[2]] = $2; $$ = [$1]; }\n'
        )
        array_code = generate_parser_c(
            array_cfg["grammar"], array_cfg["tokens"], {},
            array_cfg["token_defs"]
        )
        _compile_and_run_generated_test(array_code, r'''
/** @file generated_harness.c @brief 验证显式数组类型定义。 */
#include "generated_parser.c"
/** @brief 执行数组类型测试。 @retval 0 表示通过。 */
int main(void)
{
    ParserToken_T tokens[2] = { 0 };
    VarValue_T *pSource = Var_CreateArray();
    VarValue_T *pResult = NULL;
    int32_t first = 0;
    Var_ArrayPush(pSource, Var_CreateFloat(1.8f));
    Var_ArrayPush(pSource, Var_CreateFloat(2.2f));
    tokens[0].type = TOKEN_VARIABLE;
    tokens[0].pName = "arrayValue";
    tokens[1].type = TOKEN_SOURCE;
    tokens[1].pValue = pSource;
    pResult = Parser_Run(tokens, 2u);
    if ((pResult == NULL) || (pResult->type != VAR_TYPE_ARRAY) ||
        (Var_GetInt32(Var_ArrayGet(pResult, 0u), &first) != VAR_RUNTIME_OK) ||
        (first != 1)) { return 1; }
    Var_Free(pSource);
    return 0;
}
''')
        calc_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "grammar_calc.gram"
        )
        calc_cfg = load_grammar_file(calc_path)
        calc_code = generate_parser_c(
            calc_cfg["grammar"], calc_cfg["tokens"], calc_cfg["variables"],
            calc_cfg["token_defs"]
        )
        _compile_and_run_generated_test(calc_code, r'''
/** @file generated_harness.c @brief 验证正式示例的优先级与变量读取。 */
#include "generated_parser.c"
/** @brief 执行正式示例测试。 @retval 0 表示通过。 */
int main(void)
{
    VarValue_T *pValue = Parser_RunText("X+2*3");
    int32_t result = 0;
    if ((pValue == NULL) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 6)) { return 1; }
    Var_Free(pValue);
    pValue = Parser_RunText("X");
    if ((pValue == NULL) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 0)) { return 2; }
    Var_Free(pValue);
    pValue = Parser_RunText("X");
    if ((pValue == NULL) ||
        (Var_GetInt32(pValue, &result) != VAR_RUNTIME_OK) ||
        (result != 0)) { return 3; }
    Var_Free(pValue);
    return 0;
}
''')
        print("生成器自测通过")
        return 0

    script_dir = os.path.dirname(os.path.abspath(__file__))

    if args.grammar_file:
        cfg = load_grammar_file(args.grammar_file)
        grammar = cfg["grammar"]
        tokens = cfg["tokens"]
        variables = cfg.get("variables", {})
    elif args.grammar:
        with open(args.grammar, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        grammar = cfg["grammar"]
        tokens = cfg["tokens"]
        variables = cfg.get("variables", {})
    else:
        grammar_file = ("grammar_advanced.json" if args.advanced
                        else "grammar_default.json")
        with open(os.path.join(script_dir, grammar_file),
                  "r", encoding="utf-8") as f:
            cfg = json.load(f)
        grammar = cfg["grammar"]
        tokens = cfg["tokens"]
        variables = cfg.get("variables", {})

    token_defs = cfg.get("token_defs", {})

    analysis = prepare_grammar(grammar, tokens)
    report = format_grammar_report(analysis)
    print(report, end="")
    if args.report is not None:
        with open(args.report, "w", encoding="utf-8") as file_handle:
            file_handle.write(report)
    code = generate_parser_c(grammar, tokens, variables, token_defs)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(code)
    print(f"生成成功: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
