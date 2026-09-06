#include <stdio.h>

int max(int a, int b) {
    return a > b ? a : b; 
}

int min(int a, int b) {
    return a < b ? a : b;
 }

typedef int (*Callback)(int a, int b);

int main(void) {
    int a = 10, b = 11;

    // 通过具体函数调用
    printf("max(a, b) = %d\n", max(a, b));
    printf("min(a, b) = %d\n", min(a, b));
    
    Callback p;
    
    // 通过函数指针调用函数
    p = &max;
    printf("p(a, b) = %d\n", p(a, b));
    p = min;
    printf("p(a, b) = %d\n", p(a, b));
    
    return 0;
}
