// 预期：报未初始化结构体成员 "s.a"
struct S {
    int a;
};

int main(void) {
    S s;
    return s.a;
}
