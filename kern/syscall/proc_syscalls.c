#include <proc_syscalls.h>

#include <addrspace.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <thread.h>
#include <limits.h>
#include <syscall.h>

/*
 * Provides the pid of the current process.
 * 
 * Return Value : curproc->pid 
 * 
 */ 
int sys_getpid(int32_t *retval)
{
    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    *retval = proc->pid;

    return 0;
}

/*
 * Duplicates the current process to a newly created process. 
 * 
 * Return Value : Return twice.
 *                In the parent process, returns child process' pid.
 *                In the child process, returns 0.
 * 
 *                Upon error, returns -1 and error is set.
 */ 
int sys_fork(struct trapframe *parent_tf, int32_t *retval)
{
    /* 
    * EMPROC 	    The current user already has too many processes.
    * ENPROC		There are already too many processes on the system.
    * ENOMEM		Sufficient virtual memory for the new process was not available.
    */

    /*
    fork duplicates the currently running process. The two copies are identical,
    except that one (the "new" one, or "child"), has a new, unique process id,
    and in the other (the "parent") the process id is unchanged.

    The process id must be greater than 0.
    */
    struct proc *child_proc = NULL;
    int parent_pid;
    int result;

    // load current process
    KASSERT(curproc != NULL);
    struct proc *parent_proc = curproc;
    KASSERT(parent_proc != NULL);

    if (parent_proc->pid < PID_MIN) {
        panic("Go check your pid implementation!");
    }

    // remember the pid of current proc    
    parent_pid = parent_proc->pid;

    // create new proc
    child_proc = proc_create(parent_proc->p_name);
    if (child_proc == NULL) {
        return ENOMEM;
    }

    // copy things from the current proc
    child_proc->p_numthreads = parent_proc->p_numthreads; // unsigned p_numthreads
    // struct addrspace *p_addrspace
    // new room for our child
    child_proc->p_addrspace = as_create();
    if (child_proc->p_addrspace == NULL) {
        return ENOMEM;
    }
    // copy parent
    result = as_copy(parent_proc->p_addrspace, &child_proc->p_addrspace);
    if (result) {
        return result;
    }
    child_proc->p_cwd = parent_proc->p_cwd; // struct vnode p_cwd
    child_proc->p_pid = parent_pid; // pit_t p_pid

    KASSERT(child_proc->p_pid != -1 && child_proc->pid != -1);
    // file handle copy section
    struct fileHandle *fh = NULL;
    
    for (int i=0; i<MAXFTENTRY; i++) {
        fh = parent_proc->fileTable[i];
        if (fh != NULL) {
            child_proc->fileTable[i] = fh;
        }
    }

    // thread_fork properly (and implement enter_forked_process later!)
    thread_fork(child_proc->p_name, child_proc, enter_forked_process, (void *)parent_tf, 0);

    *retval = child_proc->pid;

    return 0;
}