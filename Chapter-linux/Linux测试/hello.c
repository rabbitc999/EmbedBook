/**
 * @file    hello.c
 * @brief   第13章 GCC 测试示例
 * @details 编译后输出 hello linux
 *
 * 编译命令：
 *   gcc -Wall -Wextra -g hello.c -o hello
 *   ./hello
 */

#include <stdio.h>

/**
 * @brief  程序入口
 * @retval 0 表示正常退出
 */
int main(void)
{
    printf("hello linux\n");
    return 0;
}
