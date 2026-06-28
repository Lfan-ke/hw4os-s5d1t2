/* 引用一个未定义符号：-shared 默认允许、留到装载期(RTLD_NOW)才失败。 */
extern int missing_extern_symbol(void);
int bad(void) { return missing_extern_symbol(); }
