cmake_minimum_required(VERSION 3.16)
project(linux_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 生成可执行文件 app
add_executable(app src/main.c)

# 打开常用警告
target_compile_options(app PRIVATE -Wall -Wextra)
