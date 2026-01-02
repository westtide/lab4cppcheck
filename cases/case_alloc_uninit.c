// 预期：报“已分配但未初始化”的内存 "*p"
#include <stdlib.h>

int main(void) {
    int *p = (int *)malloc(sizeof(int));
    int v = *p;
    free(p);
    return v;
}
