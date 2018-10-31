#include <proc_syscalls.h>

#include <addrspace.h>
#include <copyinout.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <current.h>
#include <thread.h>
#include <limits.h>
#include <syscall.h>
#include <vnode.h>
#include <vfs.h>
#include <lib.h>

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
int sys_fork(struct trapframe *tf, int32_t *retval)
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

    // copy trapframe first
    struct trapframe *parent_tf = kmalloc(sizeof(*tf));
    if (parent_tf == NULL) {
        return ENOMEM;
    }
    
    parent_tf = memcpy(parent_tf, tf, sizeof(*tf));

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
    // copy parent addrspace
    result = as_copy(parent_proc->p_addrspace, &child_proc->p_addrspace);
    if (result) {
        return result;
    }
    // copy parent pid information
    child_proc->p_pid = parent_pid;

    KASSERT(child_proc->p_pid != -1 && child_proc->pid != -1);

    // copy parent file table
    struct fileHandle *fh = NULL;
    
    for (int i=0; i<MAXFTENTRY; i++) {
        fh = parent_proc->fileTable[i];
        if (fh != NULL) {
            child_proc->fileTable[i] = fh;
            VOP_INCREF(child_proc->fileTable[i]->fh_vnode);
            fh->fh_refcount++;
        }
    }

    // copy parent working directory
    child_proc->p_cwd = parent_proc->p_cwd;
    VOP_INCREF(parent_proc->p_cwd);

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

/*
 * Wait until the given pid finishes its execution and exit.
 * 
 * Return Value : Returns pid. Upon error, returns -1 and error is set.
 */ 
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

/*
 * Replaces the currently executing program with a newly loaded program image.
 * This occurs within one process; the process id is unchanged. 
 * 
 * Return Value : Does not return. Upon error, return -1 and errno is set.
 */ 
int sys_execv(const char *program, char **args)
{
    // DEBUG LINE
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    /* The process file table and current working directory are not modified!!! */
    struct addrspace *new_as;
    struct addrspace *old_as;
	struct vnode *elf_v;
    size_t actual;
	vaddr_t entrypoint, stackptr, initial_stackptr;
	int result, argc, i;

    /*
     * ENODEV 	The device prefix of program did not exist.
     * ENOTDIR 	A non-final component of program was not a directory.
     * ENOENT 	program did not exist.
     * EISDIR 	program is a directory.
     * ENOEXEC 	program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
     * ENOMEM 	Insufficient virtual memory is available.
     * E2BIG 	The total size of the argument strings exceeeds ARG_MAX.
     * EIO 	A hard I/O error occurred.
     * EFAULT 	One of the arguments is an invalid pointer.
     */

    /* args Sanity Checks */
    if (args == NULL) {
        // DELETE THESE LINES LATER ! ! !
        panic("args == NULL");
        // DELETE THESE LINES LATER ! ! !

        return EFAULT;
    }

    char *program_copy = kmalloc(__PATH_MAX);
    result = copyinstr((const_userptr_t)program, program_copy, __PATH_MAX, &actual);
    if (result) {
        return result;
    }

    /* Calculate argc */
    argc = 0;
    while(args[argc] != NULL) {
        argc++;
    }
    
    /* args Size Check */
    int size[argc];
    int len[argc];
    for (i=0; i<argc; i++) {
        len[i] = strlen(args[i]);
        size[i] = (len[i]+1) * sizeof(char);
        if (len[i] + ((4-(len[i]%4)) % 4) > __ARG_MAX) {
            //panic("E2BIG");
            return E2BIG;
        }
    }

    /* Open Executable */
    result = vfs_open(program_copy, O_RDONLY, 0, &elf_v);
    if (result) {
        // DELETE THESE LINES LATER ! ! !
        //panic("vfs_open");
        // DELETE THESE LINES LATER ! ! !

        return result;
    }

    kfree(program_copy);
    
    /* Addrspace Creation */
    new_as = as_create();
    if (new_as == NULL) {
        // DELETE THESE LINES LATER ! ! !
        //panic("as_create");
        // DELETE THESE LINES LATER ! ! !

        vfs_close(elf_v);

        return ENOMEM;
    }

    /* Argument Backup */
    char **kargs = (char **)kmalloc(sizeof(char *) * argc);
    if (kargs == NULL) {
        // DELETE THESE LINES LATER ! ! !
        //panic("char **kargs kmalloc");
        // DELETE THESE LINES LATER ! ! !

        vfs_close(elf_v);
        as_destroy(new_as);
        return ENOMEM;
    }

    for(i=0; i<argc; i++) {
        // store each arguments on kernel heap
        kargs[i] = kmalloc(size[i]);
        if (kargs[i] == NULL) {
            for(i--; i>-1; i--) {
                kfree(kargs[i]);
            }
            kfree(kargs);
            vfs_close(elf_v);
            as_destroy(new_as);

            // DELETE THESE LINES LATER ! ! !
            //panic("kargs[i] kmalloc");
            // DELETE THESE LINES LATER ! ! !

            return ENOMEM;
        }
        result = copyinstr((userptr_t)args[i], kargs[i], size[i], &actual);
        if (result) {
            // DELETE THESE LINES LATER ! ! !
            if (EFAULT) {
                //panic("EFAULT");
            }
            if (ENAMETOOLONG) {
                //panic("ENAMETOOLONG");
            }
            // DELETE THESE LINES LATER ! ! !

            for(; i>-1; i--) {
                kfree(kargs[i]);
            }
            kfree(kargs);
            vfs_close(elf_v);
            as_destroy(new_as);

            // DELETE THESE LINES LATER ! ! !
            //panic("COPYINSTR");
            // DELETE THESE LINES LATER ! ! !

            return result;
        }    
    }

    /* Addrspace Activation */
    // throw away the old one
    old_as = proc_getas();
    
    // set the new one
    proc_setas(new_as);
    as_activate();

    /* Load the executable. */
	result = load_elf(elf_v, &entrypoint);
	if (result) {
        // DELETE THESE LINES LATER ! ! !
        //panic("LOAD_ELF");
        // DELETE THESE LINES LATER ! ! !
        
        as_deactivate(); 
        proc_setas(old_as);

        for(i--; i>-1; i--) {
            kfree(kargs[i]);
        }
        kfree(kargs);
        vfs_close(elf_v);
        as_destroy(new_as);

		return result;
	}

	vfs_close(elf_v);

    /* Define the user stack in the address space */
	result = as_define_stack(new_as, &stackptr);
	if (result) {
        // DELETE THESE LINES LATER ! ! !
        //panic("AS_DEFINE_STACK");
        // DELETE THESE LINES LATER ! ! !

		as_deactivate(); 
        proc_setas(old_as);

        for(i--; i>-1; i--) {
            kfree(kargs[i]);
        }
        kfree(kargs);
        as_destroy(new_as);
		
		return result;
	}

    /* Copyout to Userstack */
    int offset;
    char *argptr[argc+1]; // + NULL
    
    argptr[argc] = NULL;
    
    stackptr -= 4;
    bzero((void *)stackptr, 4);

    // allign the argument pointers
    for(i=0; i<argc; i++) {
        offset = size[i] + ((4-(size[i]%4)) % 4);
        KASSERT(offset % 4 == 0);
        stackptr -= offset; // progress
        argptr[i] = (char *)stackptr;
    }

    stackptr -= sizeof(char *) * (argc+1); // + NULL
    initial_stackptr = stackptr;

    // copyout argument pointers to userstack
    for(i=0; i<argc+1; i++) {
        result = copyout(&argptr[i], (userptr_t)stackptr, sizeof(char *));
        if (result) {
            // do the thing
            return result;
        }
        stackptr += 4;
    }

    // copyout arguments to userstack
    for(i=0; i<argc; i++) {
        result = copyoutstr(kargs[i], (userptr_t)argptr[i], size[i], &actual);
        if (result) {
            // do the thing
            return result;
        }
    }



    /* Recycle */    
    as_destroy(old_as);
    for(i=0; i<argc; i++) {
        kfree(kargs[i]);
    }
    kfree(kargs); 
    
    /* Warp to user mode. */
	enter_new_process(argc /*argc*/, (userptr_t)initial_stackptr /*userspace addr of argv*/,
			  (userptr_t)initial_stackptr /*userspace addr of environment*/,
			  initial_stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

}