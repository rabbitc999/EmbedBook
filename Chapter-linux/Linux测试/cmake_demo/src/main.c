/**
 * @file    main.c
 * @brief   第13章 CMake 测试示例
 * @details 由 CMakeLists.txt 构建后运行
 *
 * 构建命令：
 *   cmake -S . -B build
 *   cmake --build build
 *   ./build/app
 */

#include <stdio.h>

/**
 * @brief  程序入口
 * @retval 0 表示正常退出
 */
int main(void)
{
    printf("cmake demo run ok\n");
    return 0;
}
