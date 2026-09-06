#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
int main(int argc, char **argv)
{
    union {   // 联合体每一个成员共用一个内存
        uint16_t hw16;   // 半字16位长
        uint8_t b8[2];   // 8字节长
    } un;
    un.hw16 = 0x0102;
    if(un.b8[0] == 1 && un.b8[1] == 2) {
        printf("大端模式\n");
    }  else if (un.b8[0] == 2 && un.b8[1] == 1) {
        printf("小端模式\n");
    }  else {
        printf("未知\n");
    }  
    return 0;
}

