// 预期：修改前报 "x" 可能未初始化；修改后抑制该告警
int main(int argc, char **argv) {
    int x;
    if (argc > 1) {
        x = 42;
    }
    return x;
}
