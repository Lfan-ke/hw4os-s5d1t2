#include <stdio.h>
#include <dlfcn.h>
/* 运行时手动加载插件：dlopen→dlsym→调用→dlclose；缺符号(RTLD_NOW)立即失败。 */
int main(int argc, char **argv) {
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("HOST_FAIL: %s\n", dlerror()); return 1; }
    int (*f)(void) = (int (*)(void))dlsym(h, "answer");
    printf("answer=%d\n", f());
    void *n = dlsym(h, "nonexistent");
    printf("nonexistent=%s\n", n ? "PTR" : "NULL");
    void *b = dlopen(argv[2], RTLD_NOW);          /* 坏插件：RTLD_NOW 立即解析全部符号 */
    if (!b) printf("dlopen badplugin: %s\n", dlerror());
    else    printf("UNEXPECTED badplugin loaded\n");
    dlclose(h);
    return 0;
}
