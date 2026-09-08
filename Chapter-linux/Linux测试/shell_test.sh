#!/bin/bash
#
# 中文注释：第13章 Shell 测试脚本
# 功能：创建测试目录、演示常用文件命令与文本命令
#
# 使用方法：
#   bash shell_test.sh
#

set -e

# 创建测试目录结构
mkdir -p "$HOME/linux_test/src"
cd "$HOME/linux_test"

# 创建空文件
touch README.md main.c

# 列出目录内容（含隐藏文件，人类可读）
ls -la

# 搜索包含 main 的内容
grep -rn "main" .

# 打包测试目录
cd "$HOME"
tar -czvf linux_test.tar.gz linux_test

echo "Shell 测试完成"
