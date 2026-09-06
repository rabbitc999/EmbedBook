#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
param_migrate.py —— C 参数迁移代码生成器

功能：根据“旧版本字段表”和“新版本字段表”自动生成

    void Param_MigrateV1ToV2(const ParamDataV1_T *pOld,
                             ParamData_T *pNew);

生成规则：
1. 旧、新版本共有的字段：原样拷贝（标量用赋值，char 数组用 memcpy）；
2. 新版本新增的字段：按 schema 中的 default 赋值；
3. 若新旧 schema 中都存在 DataVersion，自动更新为目标版本。

注意：本工具生成的是“C 源码”，不负责把 .bin 文件转换成另一个 .bin。
若固件只是在旧结构体末尾追加字段，也可以只生成“默认值填充”函数，
把参数文件结构差异交给 C 代码处理。
"""

import argparse
import json
import sys


def load_schema(path):
    """读取新旧版本字段 schema JSON。"""
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data


def c_default(type_name, default):
    """把 JSON default 转成 C 字面量。"""
    if default is None:
        if type_name.startswith("float"):
            return "0.0f"
        if type_name.startswith(("uint", "int")):
            return "0"
        return '""'

    if isinstance(default, (int, float)):
        if type_name.startswith("float") or type_name.startswith("double"):
            return f"{float(default)}f"
        return str(default)

    if isinstance(default, str):
        return json.dumps(default)

    raise TypeError(f"不支持默认值类型: {type(default)}")


def is_string_field(field):
    """判断是否为 char 数组字段。"""
    ctype = field.get("type", "")
    return ctype.startswith("char") or field.get("string", False)


def emit_common_assign(field, old_ptr, new_ptr, lines):
    name = field["name"]
    if is_string_field(field):
        lines.append(
            f'    memcpy({new_ptr}->{name}, {old_ptr}->{name}, '
            f'sizeof({new_ptr}->{name}));'
        )
    else:
        lines.append(f"    {new_ptr}->{name} = {old_ptr}->{name};")


def emit_new_default(field, new_ptr, lines):
    name = field["name"]
    default = field.get("default")
    if is_string_field(field):
        if default:
            lines.append(
                f'    strncpy({new_ptr}->{name}, {c_default("char", default)}, '
                f'sizeof({new_ptr}->{name}) - 1);'
            )
            lines.append(f"    {new_ptr}->{name}[sizeof({new_ptr}->{name}) - 1] = '\\0';")
        else:
            lines.append(f"    memset({new_ptr}->{name}, 0, sizeof({new_ptr}->{name}));")
    else:
        lines.append(
            f"    {new_ptr}->{name} = {c_default(field['type'], default)};"
        )


def generate(old_schema, new_schema):
    """生成 C 迁移函数源码。"""
    old_fields = old_schema["fields"]
    new_fields = new_schema["fields"]
    old_struct = old_schema.get("struct", "ParamDataV1_T")
    new_struct = new_schema.get("struct", "ParamData_T")
    from_version = old_schema.get("version", 1)
    to_version = new_schema.get("version", 2)

    old_names = {f["name"] for f in old_fields}
    new_by_name = {f["name"]: f for f in new_fields}
    common = [f for f in old_fields if f["name"] in new_by_name]
    added = [f for f in new_fields if f["name"] not in old_names]

    lines = []
    lines.append("/* 本文件由 param_migrate.py 自动生成，请勿手改 */")
    lines.append(f"#define PARAM_DATA_VERSION_V{from_version} {from_version}u")
    lines.append(f"#define PARAM_DATA_VERSION_V{to_version} {to_version}u")
    lines.append("")
    lines.append("#include <string.h>")
    lines.append("")
    lines.append("/**")
    lines.append(" * @brief  参数版本迁移")
    lines.append(f" * @param  pOld 旧版本参数（{old_struct}）")
    lines.append(f" * @param  pNew 新版本参数（{new_struct}）")
    lines.append(" */")
    lines.append(f"void Param_MigrateV{from_version}ToV{to_version}(")
    lines.append(f"    const {old_struct} *pOld,")
    lines.append(f"    {new_struct} *pNew)")
    lines.append("{")
    lines.append("    if ((pOld == NULL) || (pNew == NULL)) {")
    lines.append("        return;")
    lines.append("    }")
    lines.append("")

    lines.append("    /* ---------- 公共字段：原样拷贝 ---------- */")
    if not common:
        lines.append("    (void)pOld;")
    for field in common:
        emit_common_assign(field, "pOld", "pNew", lines)

    lines.append("")
    lines.append("    /* ---------- 新增字段：填充默认值 ---------- */")
    for field in added:
        emit_new_default(field, "pNew", lines)

    # 统一更新版本号
    if "DataVersion" in new_by_name:
        lines.append("")
        lines.append(f"    pNew->DataVersion = PARAM_DATA_VERSION_V{to_version};")

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="生成 C 参数迁移代码（不是 bin 迁移工具）"
    )
    parser.add_argument("old_schema", help="旧版本字段 schema JSON")
    parser.add_argument("new_schema", help="新版本字段 schema JSON")
    parser.add_argument("-o", "--output", default="param_migrate.c",
                        help="输出 C 文件名")
    args = parser.parse_args()

    old_schema = load_schema(args.old_schema)
    new_schema = load_schema(args.new_schema)
    code = generate(old_schema, new_schema)

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(code)

    print(f"生成成功: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
