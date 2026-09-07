#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
grammar_loader.py —— .gram 文本文法加载器

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
        self.token_names = []
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
