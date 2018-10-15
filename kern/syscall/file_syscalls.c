#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <proc.h>
#include <current.h>
#include <synch.h>

/*
 * Opens a file.
 * 
 * Return Value : Non-negative integer upon success. -1 upon error.
 */ 


int
sys_open(const char *filename, int flags) {
    
    KASSERT(curproc != NULL);
    struct proc *proc = curproc; // load current process
    KASSERT(proc != NULL);

    // create new file handle
    struct fileHandle *fh;
    fh = fh_create();

    // open a file and populate vnode in the file handle struct
    result = vfs_open(filename, flags, 0, &fh->vnode);
	if (result) {
        // failed to open
		return -1;
	}

    int fd = 0; // A file descriptor

    // CRITICAL SECTION STARTS
    // don't forget we're accessing a shared resource!
    lock_acquire(proc->p_lock);
    KASSERT(lock_do_i_hold(proc->p_lock));

    // probe fileTable array, find the first empty space 
    while(proc->fileTable[fd] != NULL) {
        fd++;
    }
    KASSERT(proc->fileTable[fd] != NULL);

    // assign the file handle to file table entry
    proc->fileTable[fd] = fh;
    KASSERT(proc->fileTable[fd] == fh);

    // CRITICAL SECTION ENDS
    lock_release(curproc->p_lock);
    KASSERT(!lock_do_i_hold(proc->p_lock));

    return fd;
}