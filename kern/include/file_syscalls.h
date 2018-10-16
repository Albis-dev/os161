#include <types.h>

int sys_open(char *, int, int32_t *);
int sys_close(int);
int sys_write(int, const void *, size_t);