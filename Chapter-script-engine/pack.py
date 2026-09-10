#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pack_c.py —— 把 parser.py 打包成 C 固化启动器源码。

用法：
    python3 pack_c.py --parser parser.py --output parser_pack.c
    gcc -std=c99 -O2 -Wall -Wextra parser_pack.c -o parser_pack
    ./parser_pack --mode slr --grammar-file calc.gram --project-dir out/
"""

import argparse
import base64
import zlib


# 注意：外层用 r"""...""" 而不是 r'''...'''，
# 因为 C 模板里会字面出现 b''' 和 ''')，用三单引号会提前结束 Python 字符串。
C_TEMPLATE = r"""/* 由 pack_c.py 自动生成：parser.py 的 C 固化启动器。
 * 编译：gcc -std=c99 -O2 -Wall -Wextra parser_pack.c -o parser_pack
 * 运行：./parser_pack --mode slr --grammar-file calc.gram --project-dir out/
 */

#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* ================= 内嵌 base85 压缩数据 ================= */
static const char PACKED_PY_B85[] =
__PACKED_DATA__
;
/* ================= 内嵌数据结束 ================= */

/* 生成临时启动器脚本内容，返回 0 表示成功。 */
static int WriteLauncher(FILE *fp)
{
    if (fputs("import base64, zlib, sys\n", fp) < 0) { return -1; }
    if (fputs("src = zlib.decompress(base64.b85decode(b", fp) < 0) { return -1; }
    if (fputs("'''", fp) < 0) { return -1; }
    if (fputs(PACKED_PY_B85, fp) < 0) { return -1; }
    if (fputs("'''))\n", fp) < 0) { return -1; }
    if (fputs("sys.argv = ['parser.py'] + sys.argv[1:]\n", fp) < 0) { return -1; }
    if (fputs("exec(compile(src, 'parser.py', 'exec'))\n", fp) < 0) { return -1; }
    return 0;
}

#if !defined(_WIN32)
/* ================= POSIX ================= */

int main(int argc, char **argv)
{
    char tmpl[] = "/tmp/parser_pack_XXXXXX";
    int fd = -1;
    FILE *fp = NULL;
    char **new_argv = NULL;
    pid_t pid = 0;
    int status = 0;
    int i = 0;
    int n = 0;
    int rc = 1;

    fd = mkstemp(tmpl);
    if (fd < 0) {
        fprintf(stderr, "parser_pack: mkstemp 失败: %s\n", strerror(errno));
        return 1;
    }
    fp = fdopen(fd, "wb");
    if (fp == NULL) {
        fprintf(stderr, "parser_pack: fdopen 失败: %s\n", strerror(errno));
        close(fd);
        unlink(tmpl);
        return 1;
    }
    if (WriteLauncher(fp) != 0) {
        fprintf(stderr, "parser_pack: 写临时启动器失败\n");
        fclose(fp);
        unlink(tmpl);
        return 1;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "parser_pack: 关闭临时文件失败\n");
        unlink(tmpl);
        return 1;
    }

    /* 新 argv: python3 <tmpl> <原参数...> */
    new_argv = (char **)calloc((size_t)argc + 2, sizeof(char *));
    if (new_argv == NULL) {
        fprintf(stderr, "parser_pack: 内存不足\n");
        unlink(tmpl);
        return 1;
    }
    new_argv[n++] = (char *)"python3";
    new_argv[n++] = tmpl;
    for (i = 1; i < argc; i++) {
        new_argv[n++] = argv[i];
    }
    new_argv[n] = NULL;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "parser_pack: fork 失败: %s\n", strerror(errno));
        free(new_argv);
        unlink(tmpl);
        return 1;
    }
    if (pid == 0) {
        execvp("python3", new_argv);
        fprintf(stderr, "parser_pack: 无法启动 python3: %s\n", strerror(errno));
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "parser_pack: waitpid 失败: %s\n", strerror(errno));
        free(new_argv);
        unlink(tmpl);
        return 1;
    }
    free(new_argv);
    unlink(tmpl);

    if (WIFEXITED(status)) {
        rc = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        rc = 128 + WTERMSIG(status);
    }
    return rc;
}

#else  /* _WIN32 */
/* ================= Windows ================= */

int main(int argc, char **argv)
{
    char tmpDir[MAX_PATH];
    char tmpPath[MAX_PATH];
    FILE *fp = NULL;
    char **new_argv = NULL;
    intptr_t rc = 0;
    int i = 0;
    int n = 0;
    DWORD dirLen = 0;

    dirLen = GetTempPathA((DWORD)sizeof(tmpDir), tmpDir);
    if ((dirLen == 0) || (dirLen >= (DWORD)sizeof(tmpDir))) {
        fprintf(stderr, "parser_pack: GetTempPath 失败\n");
        return 1;
    }
    if (GetTempFileNameA(tmpDir, "prs", 0, tmpPath) == 0) {
        fprintf(stderr, "parser_pack: GetTempFileName 失败\n");
        return 1;
    }

    fp = fopen(tmpPath, "wb");
    if (fp == NULL) {
        fprintf(stderr, "parser_pack: 打开临时文件失败\n");
        DeleteFileA(tmpPath);
        return 1;
    }
    if (WriteLauncher(fp) != 0) {
        fprintf(stderr, "parser_pack: 写临时启动器失败\n");
        fclose(fp);
        DeleteFileA(tmpPath);
        return 1;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "parser_pack: 关闭临时文件失败\n");
        DeleteFileA(tmpPath);
        return 1;
    }

    new_argv = (char **)calloc((size_t)argc + 3, sizeof(char *));
    if (new_argv == NULL) {
        fprintf(stderr, "parser_pack: 内存不足\n");
        DeleteFileA(tmpPath);
        return 1;
    }

    /* 优先 py -3 */
    new_argv[n++] = (char *)"py";
    new_argv[n++] = (char *)"-3";
    new_argv[n++] = tmpPath;
    for (i = 1; i < argc; i++) { new_argv[n++] = argv[i]; }
    new_argv[n] = NULL;

    rc = _spawnvp(_P_WAIT, "py", (const char *const *)new_argv);
    if (rc < 0) {
        /* 回退 python */
        n = 0;
        new_argv[n++] = (char *)"python";
        new_argv[n++] = tmpPath;
        for (i = 1; i < argc; i++) { new_argv[n++] = argv[i]; }
        new_argv[n] = NULL;
        rc = _spawnvp(_P_WAIT, "python", (const char *const *)new_argv);
    }
    free(new_argv);
    DeleteFileA(tmpPath);

    if (rc < 0) {
        fprintf(stderr, "parser_pack: 无法启动 Python 解释器\n");
        return 1;
    }
    return (int)rc;
}

#endif  /* _WIN32 */
"""


def make_c_string(data: bytes) -> str:
    """把 base85 字节串转成 C 字符串字面量，每行 76 列。"""
    text = data.decode("ascii")
    lines = []
    for i in range(0, len(text), 76):
        lines.append('    "%s"' % text[i:i + 76])
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(
        description="把 parser.py 打包成 C 固化启动器源码"
    )
    ap.add_argument("--parser", default="parser.py",
                    help="输入 Python 源文件（默认 parser.py）")
    ap.add_argument("--output", default="parser_pack.c",
                    help="输出 C 源文件（默认 parser_pack.c）")
    ap.add_argument("--level", type=int, default=9,
                    help="zlib 压缩级别（默认 9）")
    args = ap.parse_args()

    with open(args.parser, "rb") as f:
        src = f.read()

    packed = base64.b85encode(zlib.compress(src, args.level))
    data_block = make_c_string(packed)
    code = C_TEMPLATE.replace("__PACKED_DATA__", data_block)

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(code)

    print("已生成 %s" % args.output)
    print("  源文件: %s (%d 字节)" % (args.parser, len(src)))
    print("  base85: %d 字节" % len(packed))


if __name__ == "__main__":
    main()