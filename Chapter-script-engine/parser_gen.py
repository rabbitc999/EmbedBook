#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
parser_gen.py —— Yacc 风格递归下降解析器生成器

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
  - 每个非终结符对应一个 Parse_xxx() 递归函数
"""

import argparse
import json
import os
import re
import sys
import warnings

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


def _var_block(variables):
    """生成内置变量表与按名取值函数，支持标量/数组/结构体/字典。"""
    decls = []
    assigns = []
    builder_code_parts = []
    names = []

    for name, spec in variables.items():
        if isinstance(spec, str):
            spec = {"type": spec}
        if not isinstance(spec, dict):
            spec = {"type": "int", "value": spec}

        symbol = "s_" + name
        root_fn = "Vars_Create_s_%s" % name
        names.append(name)
        decls.append("static VarValue_T *%s = NULL;" % symbol)
        assigns.append("%s = %s();" % (symbol, root_fn))
        builder_code_parts.append(_var_emit_node(spec, root_fn))

    if not variables:
        return r"""
static VarValue_T *Var_FindVariable(const char *name)
{
    (void)name;
    return NULL;
}
"""
    decl_text = "\n".join(decls)
    assign_text = "\n        ".join(assigns)
    builder_code = "\n\n".join(builder_code_parts)
    conds = "\n".join(
        '        if (strcmp(name, "%s") == 0) { return s_%s; }' % (n, n)
        for n in names
    )
    return """
/* 内置变量由 grammar JSON/.gram 的 variables 定义 */
static uint8_t s_VarsReady = 0;
%s

%s

static int32_t Vars_InitAll(void)
{
    if (s_VarsReady != 0u) {
        return VAR_RUNTIME_OK;
    }
    %s
    s_VarsReady = 1u;
    return VAR_RUNTIME_OK;
}

static VarValue_T *Var_FindVariable(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    Vars_InitAll();
%s
    return NULL;
}
""" % (decl_text, builder_code, assign_text, conds)


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
            code = f"Var_FindVariable(names[{index}])"
            return _ExprNode("ptr", code, lvalue=("value", code))

        if self.tokens[self.pos][0] == "id":
            ident = token
            self.pos += 1
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

        raise ValueError(
            f"动作 '{self.action}'：直接给 $n 赋值会把子结果指针指向新对象，"
            "会破坏归属关系；请用 $n.int32 = 表达式 或 $$ = 表达式"
        )


def _translate_action(action):
    """对外入口：把一条动作文本翻译成可生成的 C 语句。"""
    return _ActionTranslator(action).translate()


def c_token_name(token):
    return "TOKEN_" + token


def generate_parser_c(grammar, tokens, variables=None):
    """根据文法和 TOKEN 流生成递归下降 C 解析器。"""
    start = grammar["start"]
    rules = grammar["rules"]
    terminals = list(tokens) + ["END"]
    nonterms = list(rules.keys())
    first = compute_first(rules, terminals, nonterms)
    follow = compute_follow(start, rules, terminals, nonterms, first)
    variables = variables or {}
    need_var = bool(variables) or any(
        "[$" in prod.get("action", "")
        for prods in rules.values() for prod in prods
    )

    # LL(1) 检查：基产生式之间、左递归续产生式之间必须互不冲突
    for lhs, prods in rules.items():
        base_prods = []
        cont_prods = []
        for prod in prods:
            if prod["rhs"] and prod["rhs"][0] == lhs:
                cont_prods.append(prod)
            else:
                base_prods.append(prod)

        if cont_prods and not base_prods:
            raise ValueError(
                "文法错误：%s 只有左递归产生式，缺少基产生式"
                % lhs
            )

        def check_group(group, is_cont=False):
            used = {}
            for prod in group:
                rhs = prod["rhs"][1:] if is_cont else prod["rhs"]
                ss = select_set(
                    dict(prod, rhs=rhs), lhs, rules,
                    terminals, nonterms, first, follow
                )
                for token in ss:
                    if token in used:
                        raise ValueError(
                            "文法不是 LL(1)：%s 在选择符 %s 上存在冲突"
                            % (lhs, token)
                        )
                    used[token] = prod

        check_group(base_prods)
        if cont_prods:
            check_group(cont_prods, is_cont=True)

    # 生成函数体前的公共动作翻译（供普通与循环产生式共用）
    def translate_prod_action(prod, name, prod_index):
        """把一条产生式的动作翻译成 C 语句文本。"""
        try:
            return _translate_action(
                prod.get("action", "$$ = 0;")
            )
        except ValueError as exc:
            raise ValueError(
                "规则 %s 第 %d 个产生式的动作错误：%s"
                % (name, prod_index, exc)
            ) from exc

    def build_case_lines(rhs, prod, name, prod_index,
                         value_start, cont_mode):
        """为一个 (token, 产生式) 生成 switch case 内部的解析语句。"""
        lines = []
        if cont_mode:
            lines.append("            values[1] = accum;")
            lines.append("            names[1] = NULL;")
        value_index = value_start
        for sym in rhs:
            if sym in terminals:
                lines.append(
                    "            if (ctx->cur.type != %s) "
                    "{ return Parser_Fail(ctx); }" % c_token_name(sym)
                )
                lines.append(
                    "            names[%d] = ctx->cur.name;" % value_index
                )
                if sym == "NUMBER":
                    lines.append(
                        "            values[%d] = "
                        "Var_CreateInt(ctx->cur.value);" % value_index
                    )
                elif sym == "TRUE":
                    lines.append(
                        "            values[%d] = Var_CreateInt(1);"
                        % value_index
                    )
                elif sym == "FALSE":
                    lines.append(
                        "            values[%d] = Var_CreateInt(0);"
                        % value_index
                    )
                elif sym in ("VAR", "IDENT"):
                    lines.append(
                        "            values[%d] = "
                        "Var_FindVariable(ctx->cur.name);" % value_index
                    )
                else:
                    lines.append(
                        "            values[%d] = NULL;" % value_index
                    )
                lines.append("            Parser_Advance(ctx);")
                value_index += 1
            else:
                lines.append(
                    "            ParseResult_T _r%d_%d = Parse_%s(ctx);"
                    % (prod_index, value_index, sym)
                )
                lines.append(
                    "            if (!_r%d_%d.ok) { "
                    "result = _r%d_%d; return result; }"
                    % (prod_index, value_index,
                       prod_index, value_index)
                )
                lines.append(
                    "            values[%d] = _r%d_%d.value;"
                    % (value_index, prod_index, value_index)
                )
                value_index += 1

        act = translate_prod_action(prod, name, prod_index)
        lines.append("            " + act)

        # 断言：GET_VAR 等语义检查，失败则带错误信息向上返回
        assert_expr = prod.get("assert")
        error_msg = prod.get("error_msg")
        if assert_expr:
            cond = assert_expr
            cond = re.sub(
                r"GET_VAR\(\$(\d+)\)",
                r"GET_VAR(names[\1])",
                cond
            )
            for n in range(value_start, value_index + 1):
                cond = cond.replace(f"${n}", f"names[{n}]")
            fmt = error_msg if error_msg else "解析错误"
            args = []
            for match in re.finditer(r"\{\{\$(\d+)\}\}", fmt):
                arg_index = int(match.group(1))
                args.append(
                    "names[%d] ? names[%d] : \"?\""
                    % (arg_index, arg_index)
                )
            fmt = re.sub(r"\{\{\$\d+\}\}", "%s", fmt)
            args_txt = ", " + ", ".join(args) if args else ""
            lines.append(
                "            if (!(%s)) {" % cond
            )
            lines.append(
                "                snprintf(result.errorMsg, "
                "sizeof(result.errorMsg), \"%s\"%s);" % (fmt, args_txt)
            )
            lines.append(
                "                result.ok = 0;"
                "                return result;"
            )
            lines.append("            }")
        return lines

    def make_switch_cases(prods, name, cont_mode):
        """生成 switch 语句所需的 case 片段。"""
        cases = []
        for prod_index, prod in enumerate(prods):
            if cont_mode:
                rhs = prod["rhs"][1:]
            else:
                rhs = prod["rhs"]
            ss = select_set(
                dict(prod, rhs=rhs), name, rules,
                terminals, nonterms, first, follow
            )
            for token in sorted(ss):
                case_body = build_case_lines(
                    rhs, prod, name, prod_index,
                    value_start=(2 if cont_mode else 1),
                    cont_mode=cont_mode
                )
                if cont_mode:
                    case_body.append("            matched = 1u;")
                    case_body.append("            accum = result.value;")
                else:
                    case_body.append("            result.ok = 1;")
                case_body.append("            break;")
                cases.append(
                    "        case " + c_token_name(token) + ":\n"
                    "        {\n"
                    + "\n".join(case_body) +
                    "\n        }"
                )
        return cases

    funcs = []
    for name in nonterms:
        prods = rules[name]
        base_prods = []
        cont_prods = []
        for prod in prods:
            if prod["rhs"] and prod["rhs"][0] == name:
                cont_prods.append(prod)
            else:
                base_prods.append(prod)

        if not cont_prods:
            # 普通 LL(1) 递归下降函数
            switch_cases = make_switch_cases(prods, name, False)
            funcs.append("""
static ParseResult_T Parse_%s(ParserCtx_T *ctx)
{
    ParseResult_T result = { 0 };
    VarValue_T *values[16] = { 0 };
    const char *names[16] = { 0 };

    switch (ctx->cur.type) {
%s
        default:
            return Parser_Fail(ctx);
    }
    return result;
}
""" % (name, "\n".join(switch_cases)))
            continue

        # 左递归：先解析基产生式，再循环匹配续产生式
        base_cases = make_switch_cases(base_prods, name, False)
        cont_cases = make_switch_cases(cont_prods, name, True)
        funcs.append("""
static ParseResult_T Parse_%s(ParserCtx_T *ctx)
{
    ParseResult_T result = { 0 };
    VarValue_T *values[16] = { 0 };
    const char *names[16] = { 0 };
    VarValue_T *accum = NULL;
    uint8_t matched = 0u;

    /* 基产生式：expr -> term 等 */
    switch (ctx->cur.type) {
%s
        default:
            return Parser_Fail(ctx);
    }
    accum = result.value;

    /* 续产生式：expr -> expr PLUS term 等，保持左结合循环 */
    while (1u) {
        matched = 0u;
        switch (ctx->cur.type) {
%s
            default:
                break;
        }
        if (matched == 0u) {
            break;
        }
    }
    return result;
}
""" % (name, "\n".join(base_cases), "\n".join(cont_cases)))


    # 入口函数
    return r'''/* 由 Chapter-script-engine/parser_gen.py 自动生成：Yacc 风格 LL(1) 递归下降解析器 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "variable_runtime.h"

''' + "\n".join(
        f"typedef enum __ParserTokenType_T {{ {', '.join('TOKEN_'+t for t in terminals)} }} ParserTokenType_T;\n"
        for _ in [0]
    ) + r'''
typedef struct __ParserToken_T
{
    ParserTokenType_T type;   /* 终结符类型 */
    int32_t value;            /* 终结符语义值（词法层填入） */
    const char *name;         /* VAR/IDENT 等变量名字符串 */
    uint32_t line;            /* 行号 */
    uint32_t column;          /* 列号 */
} ParserToken_T;

typedef struct __ParseResult_T
{
    uint8_t ok;
    VarValue_T *value;        /* 语义值改为运行时对象 */
    char errorMsg[64];        /* 失败时带错误信息向上传递 */
} ParseResult_T;

typedef struct __ParserCtx_T
{
    const ParserToken_T *tokens;
    uint32_t count;
    uint32_t pos;
    ParserToken_T cur;
} ParserCtx_T;

static void Parser_Advance(ParserCtx_T *ctx)
{
    if (ctx->pos + 1 < ctx->count) {
        ctx->pos++;
        ctx->cur = ctx->tokens[ctx->pos];
    } else {
        ctx->pos = ctx->count;
        ctx->cur.type = TOKEN_END;
    }
}

static ParseResult_T Parser_Fail(ParserCtx_T *ctx)
{
    ParseResult_T result = { 0 };
    snprintf(result.errorMsg, sizeof(result.errorMsg), "syntax error");
    (void)ctx;
    return result;
}

''' + "\n".join(
        f"static ParseResult_T Parse_{name}(ParserCtx_T *ctx);"
        for name in nonterms
    ) + "\n" + (
        _var_block(variables)
        if need_var else ""
    ) + "\n" + r'''

VarValue_T *Parser_Run(const ParserToken_T *tokens, uint32_t count)
{
    ParserCtx_T ctx;
    ParseResult_T result;

    memset(&ctx, 0, sizeof(ctx));
    ctx.tokens = tokens;
    ctx.count = count;
    if (count == 0) {
        return NULL;
    }
    ctx.cur = tokens[0];

''' + f"    result = Parse_{start}(&ctx);\n" + r'''    if ((!result.ok) || (ctx.cur.type != TOKEN_END)) {
        return NULL;
    }
    return result.value;
}
''' + "\n".join(funcs)


def main():
    parser = argparse.ArgumentParser(
        description="Yacc 风格 LL(1) 递归下降解析器生成器"
    )
    parser.add_argument("-o", "--output", default="parser_gen.c",
                        help="输出 C 文件名")
    parser.add_argument("--grammar", default=None,
                        help="可选 JSON 文法文件")
    parser.add_argument("--grammar-file", default=None,
                        help="可选 .gram 文本文法文件")
    parser.add_argument("--advanced", action="store_true",
                        help="生成 IF/FOR/布尔运算符高级示例文法")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))

    if args.grammar_file:
        import grammar_loader
        cfg = grammar_loader.load_grammar_file(args.grammar_file)
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

    code = generate_parser_c(grammar, tokens, variables)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(code)
    print(f"生成成功: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
