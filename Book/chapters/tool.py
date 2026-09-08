#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LaTeX \textbf 移除工具
功能：移除文件中所有的 \textbf{...} 命令，仅保留内部文本（支持嵌套花括号）
用法：
    python remove_textbf.py [文件路径]           # 处理指定文件（原地修改，自动备份）
    python remove_textbf.py                      # 处理当前目录下所有 .tex 文件（递归）
    cat input.tex | python remove_textbf.py -   # 从标准输入读取，输出到标准输出
"""

import os
import sys

def find_matching_brace(text, start):
    """
    从 start 位置开始（该位置应为 '{' 之后的下一个字符）
    找到与之匹配的 '}' 的索引，返回结束位置 + 1
    若未找到，返回 -1
    """
    brace_count = 1
    i = start
    while i < len(text) and brace_count > 0:
        if text[i] == '{':
            brace_count += 1
        elif text[i] == '}':
            brace_count -= 1
        i += 1
    return i if brace_count == 0 else -1

def remove_textbf(text):
    """移除文本中所有 \textbf{...}，保留内部内容"""
    result = []
    i = 0
    n = len(text)

    while i < n:
        # 检测 \textbf{ 开头
        if text.startswith('\\textbf{', i):
            start_content = i + 8  # len('\\textbf{')
            end_pos = find_matching_brace(text, start_content)
            if end_pos != -1:
                # 提取内部内容（不含外层花括号）
                content = text[start_content:end_pos - 1]
                result.append(content)
                i = end_pos
                continue
            else:
                # 未找到匹配的 }，则原样复制，防止死循环
                result.append(text[i])
                i += 1
        else:
            result.append(text[i])
            i += 1

    return ''.join(result)

def process_file(filepath):
    """处理单个文件：读取、替换、备份、写回"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            original = f.read()
    except Exception as e:
        print(f"读取文件 {filepath} 失败: {e}", file=sys.stderr)
        return False

    # 生成备份
    backup_path = filepath + '.bak'
    try:
        with open(backup_path, 'w', encoding='utf-8') as f:
            f.write(original)
    except Exception as e:
        print(f"创建备份 {backup_path} 失败: {e}", file=sys.stderr)
        return False

    # 执行替换
    converted = remove_textbf(original)

    # 写回原文件
    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(converted)
        print(f"已处理: {filepath}")
        return True
    except Exception as e:
        print(f"写入文件 {filepath} 失败: {e}", file=sys.stderr)
        return False

def process_stdin():
    """从标准输入读取，处理后输出到标准输出"""
    text = sys.stdin.read()
    result = remove_textbf(text)
    sys.stdout.write(result)

def main():
    # 判断是否从管道/标准输入读取
    if not sys.stdin.isatty() and len(sys.argv) == 1:
        # 有管道输入且没有指定文件
        process_stdin()
        return

    # 处理命令行参数
    if len(sys.argv) >= 2:
        # 处理指定文件
        filepath = sys.argv[1]
        if filepath == '-':
            process_stdin()
        else:
            if not os.path.isfile(filepath):
                print(f"错误：文件 '{filepath}' 不存在", file=sys.stderr)
                sys.exit(1)
            process_file(filepath)
    else:
        # 未指定文件，递归处理当前目录下所有 .tex 文件
        count = 0
        for root, dirs, files in os.walk('.'):
            for f in files:
                if f.endswith('.tex'):
                    fullpath = os.path.join(root, f)
                    if process_file(fullpath):
                        count += 1
        print(f"共处理 {count} 个 .tex 文件。")

if __name__ == '__main__':
    main()
