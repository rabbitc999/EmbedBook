#include <stdio.h>
#include <stdint.h>

/* 显示二进制位 */
void ShowBin(uint64_t bin, uint8_t Length) {
	uint8_t showIndex;
    uint64_t high = 1ULL << (Length - 1);
	for (showIndex = 1; showIndex <= Length; showIndex++) {
		if ((bin & high) != 0) {
			putchar('1');
		} else {
			putchar('0');
		}
		bin = bin << 1;
		if (showIndex % 4 == 0) {
			putchar(' ');
		}
	}
}

/* 显示IEEE754详细信息 */
void ShowFloatIEEE754(float x) {
    union {
        float Number;
        struct {
            uint32_t M : 23;
            uint32_t E : 8;
            uint32_t S : 1;
        } Pack;
    } Current = { .Number =x };
    
    printf("S="); ShowBin(Current.Pack.S, 1);
    printf(", E="); ShowBin(Current.Pack.E, 8);
    printf(", M=1."); ShowBin(Current.Pack.M, 23);
    putchar('\n');
}

/* 显示IEEE754详细信息 */
void ShowDoubleIEEE754(double x) {
    union {
        double Number;
        struct {
            uint64_t M : 52;
            uint64_t E : 11;
            uint64_t S : 1;
        } Pack;
    } Current = { .Number =x };
    
    printf("S="); ShowBin(Current.Pack.S, 1);
    printf(", E="); ShowBin(Current.Pack.E, 11);
    printf(", M=1."); ShowBin(Current.Pack.M, 52);
    putchar('\n');
}

/* 主函数 */
int main(int argc, char **argv) {
    double a = 0.1, b = 0.2, c = 0.3;
    printf("A=%.2f   IEEE754: ", a); ShowDoubleIEEE754(a);
    printf("B=%.2f   IEEE754: ", b); ShowDoubleIEEE754(b);
    printf("C=%.2f   IEEE754: ", c); ShowDoubleIEEE754(c);
    printf("A+B=%.2f IEEE754: ", a + b); ShowDoubleIEEE754(a + b);
	return 0;
}

