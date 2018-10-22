#include <types.h>
#include <mips/trapframe.h>

int sys_fork(struct trapframe *, int32_t *);
int sys_getpid(int32_t *);