#include <stdio.h>
/* 必须保持 printf("hello\n")：gcc 会把它优化成 puts，故 nm 出 'U puts'（pitfall #4）。 */
int main(void) { printf("hello\n"); return 0; }
