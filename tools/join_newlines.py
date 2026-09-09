#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
join_newlines.py —— LaTeX/Markdown 换行规范化工具

规则：
  1. 双换行（段落分隔）原样保留；
  2. 单换行被合并成同一段落：
       - 换行前后都是中文（含中文标点）时不加空格；
       - 其他情况加一个英文空格；
  3. 代码块（lstlisting / verbatim / Markdown 围栏 / tikzpicture）
     内部换行保持原样，不改变代码逻辑；
  4. 注释行、\\begin/\\end、表格行分隔符等结构行独立保留，
     避免合并后破坏 LaTeX 结构。

用法：
  python tools/join_newlines.py 文件路径
  python tools/join_newlines.py 文件路径 --in-place
  python tools/join_newlines.py 文件路径 -o 输出路径
"""

import argparse
import io
import re
import sys


def is_chinese_char(ch):
    """判断是否为中文字符或中文标点。"""
    point = ord(ch)
    return (
        0x4E00 <= point <= 0x9FFF or
        0x3400 <= point <= 0x4DBF or
        0xF900 <= point <= 0xFAFF or
        0x3000 <= point <= 0x303F or
        0xFF00 <= point <= 0xFFEF
    )


def join_lines(lines):
    """合并组内连续行；中文相接不加空格，其他情况加一个空格。"""
    result = ""
    for line in lines:
        text = line.strip()
        if not text:
            continue
        if not result:
            result = text
            continue
        if is_chinese_char(result[-1]) and is_chinese_char(text[0]):
            result += text
        else:
            result += " " + text
    return result


def is_hard_boundary(line):
    """结构行：单独保留，避免破坏环境/注释。"""
    text = line.strip()
    return (
        text.startswith("%") or
        text.startswith("\\begin{") or
        text.startswith("\\end{") or
        text.startswith("\\toprule") or
        text.startswith("\\midrule") or
        text.startswith("\\bottomrule") or
        text.startswith("\\hline")
    )


def is_row_terminator(line):
    """表格/矩阵行结束标记。"""
    return line.rstrip().endswith("\\\\")


def process_text(text):
    """对整段文本执行换行规范化，返回处理后的文本。"""
    lines = text.splitlines()
    output = []
    buffer_lines = []
    item_lines = None
    code_kind = None
    md_fence = None
    tikz_depth = 0

    def flush_prose():
        """把普通正文组输出。"""
        if buffer_lines:
            output.append(join_lines(buffer_lines))
            buffer_lines.clear()

    def flush_item():
        """把列表项（含续行）合并输出。"""
        nonlocal item_lines
        if item_lines:
            output.append(join_lines(item_lines))
            item_lines = None

    for line in lines:
        stripped = line.strip()
        stripped_lower = stripped.lower()

        # LaTeX 代码块与 verbatim：原样保留内部换行
        if code_kind is None and re.match(
                r"\\begin\{(lstlisting|verbatim)\}", stripped_lower):
            flush_prose()
            flush_item()
            code_kind = re.match(
                r"\\begin\{(lstlisting|verbatim)\}", stripped_lower).group(1)
            output.append(line)
            continue
        if code_kind is not None:
            output.append(line)
            if re.match(r"\\end\{" + code_kind + r"\}", stripped_lower):
                code_kind = None
            continue

        # Markdown 代码围栏：原样保留
        if md_fence is None and stripped.startswith("```"):
            flush_prose()
            flush_item()
            md_fence = "```"
            output.append(line)
            continue
        if md_fence is not None:
            output.append(line)
            if stripped.startswith("```"):
                md_fence = None
            continue

        # TikZ 绘图代码：保持分行，方便日后编辑
        if tikz_depth == 0 and stripped.startswith("\\begin{tikzpicture}"):
            flush_prose()
            flush_item()
            tikz_depth = 1
            output.append(line)
            continue
        if tikz_depth > 0:
            output.append(line)
            if stripped.startswith("\\begin{tikzpicture}"):
                tikz_depth += 1
            elif stripped.startswith("\\end{tikzpicture}"):
                tikz_depth -= 1
            continue

        # 空行：段落边界
        if not stripped:
            flush_prose()
            flush_item()
            output.append("")
            continue

        # 列表项：与后续续行合并成同一条内容
        if stripped.startswith("\\item"):
            flush_prose()
            flush_item()
            item_lines = [line]
            continue

        # 结构行：独立保留
        if is_hard_boundary(line):
            flush_prose()
            flush_item()
            output.append(line.rstrip())
            continue

        # 表格行结束符：先收当前组，再单独保留该行
        if is_row_terminator(line):
            if buffer_lines or item_lines is not None:
                if item_lines is not None:
                    item_lines.append(line)
                    flush_item()
                else:
                    buffer_lines.append(line)
                    flush_prose()
            else:
                output.append(line.rstrip())
            continue

        if item_lines is not None:
            item_lines.append(line)
        else:
            buffer_lines.append(line)

    flush_prose()
    flush_item()
    return "\n".join(output)


def process_file(path):
    """读取文件并返回规范化文本。"""
    with io.open(path, "r", encoding="utf-8") as file_handle:
        return process_text(file_handle.read())


def main():
    parser = argparse.ArgumentParser(
        description="LaTeX/Markdown 换行规范化工具"
    )
    parser.add_argument("file", help="待处理文件路径")
    parser.add_argument("-o", "--output", default=None,
                        help="输出文件；缺省时打印到终端")
    parser.add_argument("--in-place", action="store_true",
                        help="直接覆盖原文件")
    args = parser.parse_args()

    result = process_file(args.file)
    if args.in_place:
        with io.open(args.file, "w", encoding="utf-8",
                     newline="") as file_handle:
            file_handle.write(result)
        print("已规范化: %s" % args.file)
    elif args.output:
        with io.open(args.output, "w", encoding="utf-8",
                     newline="") as file_handle:
            file_handle.write(result)
        print("已写入: %s" % args.output)
    else:
        sys.stdout.write(result)
        if not result.endswith("\n"):
            sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
