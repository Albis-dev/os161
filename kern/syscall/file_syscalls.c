/*
 * File System Calls
 * 
 * Don't forget to dispatch using system call dispatcher.
 * (arch/mips/syscall/syscall.c)
 */

#include <file_syscalls.h>

#include <current.h>
#include <copyinout.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <proc.h>
#include <synch.h>
#include <spl.h>
#include <types.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

/*
 * Opens a file.
 * 
 * Using vfs_open(), populate the file handle structure and assign
 * a process-specific file descriptor to it.
 * 
 * Return Value : Non-negative integer(file descriptor) upon success.
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

    // access mode
    fh->fh_accmode = flags & O_ACCMODE;

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
 * Return Value : Always 0 (Never fails)
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
    } else {
        lock_release(fh->fh_lock);
    }

    return 0; 
}

/*
 * Writes to a file.
 * 
 * Using VOP_WRITE() macro, initialize uio using uio_kinit() (see uio.h) and execute.
 * 
 * Return Value : Bytes written
 */ 
int
sys_write(int fd, void *buf, size_t buflen, ssize_t *retval)
{
    /*
     * EBADF 	fd is not a valid file descriptor, or was not opened for writing.
     * EFAULT 	Part or all of the address space pointed to by buf is invalid.
     * ENOSPC 	There is no free space remaining on the filesystem containing the file.
     * EIO 	A hardware I/O error occurred writing the data.
     */

    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    // EBADF check section
    struct fileHandle *fh;

    spinlock_acquire(&proc->p_lock);
    fh = proc->fileTable[fd];
    spinlock_release(&proc->p_lock);

    lock_acquire(fh->fh_lock);

    if (fh == NULL) {
        /* invalid fd */
        return EBADF;
    } else if (fh->fh_accmode == O_RDONLY) {
        /* incorrect access mode */
        return EBADF;
    } else if (fh->fh_vnode == NULL) {
        /* haven't initialized */
        return EBADF;
    }

    int result;

    // Initialize uio structure
    struct iovec iov;
    struct uio myuio;

    uio_kinit(&iov, &myuio, (void *)buf, buflen, fh->fh_offset, UIO_WRITE);
    myuio.uio_segflg = UIO_USERSPACE;
    myuio.uio_space = proc_getas();

    // write to file
    result = VOP_WRITE(fh->fh_vnode, &myuio);
    // update the offset whether the operation failed or not.
    *retval = myuio.uio_offset - fh->fh_offset;
    fh->fh_offset = myuio.uio_offset;
    lock_release(fh->fh_lock);

    if (result) {
        // seek failed or IO failed (mostly EFAULT)
        return result;
    }
    return 0;
}

/*
 * Reads a file, store it to given buffer address.
 * 
 * 
 * Return Value : Bytes read. Error number upon failure.
 */ 
int sys_read(int fd, void *buf, size_t buflen, ssize_t *retval) {
    /*
    * EBADF 	fd is not a valid file descriptor, or was not opened for reading.
    * EFAULT 	Part or all of the address space pointed to by buf is invalid.
    * EIO 	    A hardware I/O error occurred reading the data.
    */

    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    // EBADF check section
    struct fileHandle *fh;

    spinlock_acquire(&proc->p_lock);
    fh = proc->fileTable[fd];
    spinlock_release(&proc->p_lock);

    lock_acquire(fh->fh_lock);

    if (fh == NULL) {
        /* invalid fd */
        return EBADF;
    } else if (fh->fh_accmode == O_WRONLY) {
        /* incorrect access mode */
        return EBADF;
    } else if (fh->fh_vnode == NULL) {
        /* haven't initialized */
        return EBADF;
    }

    int result;

    // Initialize uio structure
    struct iovec iov;
    struct uio myuio;

    uio_kinit(&iov, &myuio, (void *)buf, buflen, fh->fh_offset, UIO_READ);
    myuio.uio_segflg = UIO_USERSPACE;
    myuio.uio_space = proc_getas();

    result = VOP_READ(fh->fh_vnode, &myuio);

    *retval = myuio.uio_offset - fh->fh_offset;
    fh->fh_offset = myuio.uio_offset;
    lock_release(fh->fh_lock);

    if (result) {
        return result;
    }

    return 0;
}

int sys_lseek (int fd, off_t pos, int whence, off_t *retval)
{   
    int result;

    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    struct fileHandle *fh;

    spinlock_acquire(&proc->p_lock);
    fh = proc->fileTable[fd];
    spinlock_release(&proc->p_lock);

    // sanity checks
    if (fh == NULL) {
        /* Invalid fd */
        return EBADF;
    } else if (!((whence == SEEK_SET) ||
                 (whence == SEEK_CUR) ||
                 (whence == SEEK_END))) {
        /* Invalid argument */
        return ESPIPE;
    } else if (pos < 0) {
        return EINVAL;
    } else {
        lock_acquire(fh->fh_lock);

        KASSERT(fh->fh_vnode != NULL);
        KASSERT(lock_do_i_hold(fh->fh_lock));
        result = VOP_ISSEEKABLE(fh->fh_vnode);
        if (result == 0) {
            /* Non-seekable file */
            lock_release(fh->fh_lock);
            return ESPIPE;
        }
    }   

    KASSERT(lock_do_i_hold(fh->fh_lock));

    if (whence == SEEK_SET) {
        fh->fh_offset = pos;
    } else if (whence == SEEK_CUR) {
        fh->fh_offset += pos;
    } else if (whence == SEEK_END) {
        struct stat filestat;

        result = VOP_STAT(fh->fh_vnode, &filestat);
        if (result) {
            /* Can't check filesize */
            return result;
        }

        fh->fh_offset = filestat.st_size + pos;
    }

    *retval = fh->fh_offset;

    lock_release(fh->fh_lock);
    KASSERT(!lock_do_i_hold(fh->fh_lock));


    return 0;
}

/*
 * Store the name of current working directory in the given buffer.
 * Should be atomic.
 * 
 * Return Value : Bytes written to the buffer. Error number upon failure.
 */ 
int sys___getcwd(char *buf, size_t buflen, int32_t *retval)
{
    /*
     * ENOENT 	A component of the pathname no longer exists.
     * EIO		A hard I/O error occurred.
     * EFAULT		buf points to an invalid address.
     */
    int old_p_level;
    old_p_level = splhigh();

    // sanity checks
    // is the buffer NULL?
    if (buf == NULL) {
        splx(old_p_level);
        return EFAULT;
    }
    
    // load current process
    KASSERT(curproc != NULL);
    struct proc *proc = curproc;
    KASSERT(proc != NULL);

    // fetch p_cwd
    struct vnode *cwd = proc->p_cwd;
    KASSERT(cwd != NULL);

    // 1. initialize uio struct
        // uio_read bc it's kernel -> uio
        // uio_userspace
        // iovec->iov_ubase = buf
        // iovec->iov_len = buflen
        // uio->uio_space = curproc->p_addrspace
    struct iovec iov;
    struct uio myuio;

    uio_kinit(&iov, &myuio, (void *)buf, buflen, 0, UIO_READ);
    myuio.uio_segflg = UIO_USERSPACE;
    myuio.uio_space = proc_getas();

    // 2. use it
        // vop_namefile(vnode, uio);
    int result;
    result = VOP_NAMEFILE(cwd, &myuio);
    if (result) {
        splx(old_p_level); 
        return result;
    }
    
    *retval = myuio.uio_offset;

    // set spl
    splx(old_p_level);

    return 0;
}

