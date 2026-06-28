/* batch vvp never uses interactive mode; stub readline so we need no -lreadline */
#include <stdlib.h>
char* readline(const char* p){ (void)p; return (char*)0; }
void  add_history(const char* s){ (void)s; }
