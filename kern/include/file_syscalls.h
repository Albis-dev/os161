#include <types.h>

int sys_open(char *, int, int32_t *);
int sys_close(int);
int sys_write(int, void *, size_t, ssize_t *);
int sys_read(int, void *, size_t, ssize_t *);
int sys_lseek(int, off_t, int, off_t *);