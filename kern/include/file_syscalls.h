#include <types.h>

int sys_open(const char *, int, int32_t *);
int sys_close(int);
int sys_write(int, void *, size_t, ssize_t *);
int sys_read(int, void *, size_t, ssize_t *);
int sys_lseek(int, off_t, int, off_t *);
int sys___getcwd(char *, size_t, int32_t *);
int sys_chdir(const char *);
int sys_dup2(int, int, int32_t *);