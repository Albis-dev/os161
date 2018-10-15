#include <types.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <vnode.h>
#include <proc.h>
#include <current.h>
#include <synch.h>

#include <file_syscalls.h>

/*
 * Opens a file.
 * 
 * Using vfs_open(), populate the file handle structure and assign
 * a process-specific file descriptor to it.
 * 
 * Return Value : Non-negative integer upon success. 1 upon error.
 */ 
int
sys_open(char *filename, int flags, int32_t *retval) {
    
    int result = 0;

    KASSERT(curproc != NULL);
    struct proc *proc = curproc; // load current process
    KASSERT(proc != NULL);

    // create new file handle
    struct fileHandle *fh;
    fh = fh_create();

    // open a file and populate vnode in the file handle struct
    result = vfs_open(filename, flags, 0, &fh->fh_vnode);
	if (result) {
        // failed to open
        return result;
	}

    int fd = 0; // A file descriptor

    // don't forget we're accessing a shared resource!
    spinlock_acquire(&proc->p_lock);
    KASSERT(spinlock_do_i_hold(&proc->p_lock));

    // probe fileTable array, find the first empty space 
    while(proc->fileTable[fd] != NULL) {
        fd++;
    }
    KASSERT(proc->fileTable[fd] == NULL);

    // assign the file handle to file table entry
    proc->fileTable[fd] = fh;
    KASSERT(proc->fileTable[fd] == fh);

    spinlock_release(&curproc->p_lock);
    KASSERT(!spinlock_do_i_hold(&proc->p_lock));

    *retval = fd;
    return 0;
}

/*
 * Closes a file.
 * 
 * Using vfs_close(), fetch the file handle structure,
 * decrement vnode refcount and destroy the file handle if the refcount == 0
 * 
 * Return Value : Always return 0. (Never fails)
 */ 
int
sys_close(int fd) {

    struct fileHandle *fh;

    KASSERT(curproc != NULL);
    struct proc *proc = curproc; // load current process
    KASSERT(proc != NULL);

    spinlock_acquire(&proc->p_lock);
    // query the file table and get the current file handle
    fh = proc->fileTable[fd];
    KASSERT(fh != NULL);
    spinlock_release(&proc->p_lock);

    lock_acquire(fh->fh_lock);
    // decrements vnode's reference count
    // it will automatically destroy the vnode if the refcount hits zero
    // see vnode.c
    vfs_close(fh->fh_vnode);

    // destroy the file handle if no one use the file
    if (fh->fh_vnode->vn_refcount == 0) {
        lock_release(fh->fh_lock);
        fh_destroy(fh);
    }

    lock_release(fh->fh_lock);

    return 0; 
}

/*
 * Writes to a file.
 * 
 * Using VOP_WRITE() macro, initialize uio using uio_kinit() (see uio.h) and execute.
 * 
 * Return Value : Non-negative integer upon success. 1 upon error.
 */ 