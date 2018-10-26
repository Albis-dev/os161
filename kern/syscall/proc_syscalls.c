#include <proc_syscalls.h>

#include <addrspace.h>
#include <copyinout.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/wait.h>
#include <current.h>
#include <thread.h>
#include <limits.h>
#include <syscall.h>
#include <vnode.h>

/*
 * Provides the pid of the current process.
 * 
 * Return Value : curproc->pid 
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
    child_proc->p_pid = parent_pid; // pid_t p_pid

    KASSERT(child_proc->p_pid != -1 && child_proc->pid != -1);
    // file handle copy section
    struct fileHandle *fh = NULL;
    
    for (int i=0; i<MAXFTENTRY; i++) {
        fh = parent_proc->fileTable[i];
        if (fh != NULL) {
            child_proc->fileTable[i] = fh;
            VOP_INCREF(child_proc->fileTable[i]->fh_vnode);
        }
    }

    // thread_fork properly (and implement enter_forked_process later!)
    thread_fork(child_proc->p_name, child_proc, enter_forked_process, (void *)parent_tf, 0);

    *retval = child_proc->pid;

    return 0;
}

/*
 * Encode the given exitcode and store it in proc structure.
 * 
 * Return Value : Does not return
 */ 
int sys__exit(int exitcode)
{
    if (!(exitcode == 0 || exitcode == 1 || exitcode == 2  || exitcode == 3)){
        panic("Invalid exitcode");
    }

    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    // store encoded exit code
    proc->exitcode = _MKWAIT_EXIT(exitcode);
    V(proc->sem_exit);

    // thread can exit now
    thread_exit();

    return 0;
}

int sys_waitpid(pid_t pid, int *status, int options, int32_t *retval)
{
    /*
     * EINVAL 	The options argument requested invalid or unsupported options.
     * ECHILD 	The pid argument named a process that was not a child of the current process.
     * ESRCH 	The pid argument named a nonexistent process.
     * EFAULT 	The status argument was an invalid pointer.
     */
    int result;

    // "child" means the process we're waiting for

    /* sanity checks */
    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    // is the pid valid?
    if (pid < PID_MIN || pid > PID_MAX) {
        // pid out of range
        return ESRCH;
    }
    // is the "options" argument 0?
    if (options != 0) {
        // since we do not support other than 0
        return EINVAL;
    }
    // does the child exist?
    struct proc *childproc = proc_fetch(pid);
    if (childproc == NULL) {
        // child process does not exist
        return ESRCH;
    }
    // is this process the parent?
    if (childproc->p_pid != proc->pid) {
        // it's not your child!
        return ECHILD;
    }

    // set a return value 
    *retval = childproc->pid;

    // while the child has not exited...

    P(childproc->sem_exit);
    
    KASSERT(childproc->exitcode != -1);
    // store the encoded exitcode to *status
    result = copyout(&childproc->exitcode, (userptr_t)status, sizeof(childproc->exitcode));
    if (result) {
        panic("copyout is not working!");
        return result;
    }

    // unlink the proc table
    proc_deregister(childproc);

    // destroy the child process
    proc_destroy(childproc);

    return 0;
}