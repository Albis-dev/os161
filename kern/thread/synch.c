/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <spl.h>
#include <cpu.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	// add stuff here as needed
	struct semaphore *sem;

	sem = sem_create("binary_sem", 1); // create a binary semaphore, "binary_sem"
	lock->lk_sem = sem;
	
	lock->lk_holder = NULL;
	// END OF ADDED STUFFS

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	// add stuff here as needed
	KASSERT(lock->lk_holder == NULL);
	sem_destroy(lock->lk_sem);
	// END OF ADDED STUFFS

	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	int old_p_level; // to store an old priority level value. check spl.h for more details.

	/* Call this (atomically) before waiting for a lock */
	old_p_level = splhigh();
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);
	splx(old_p_level);

	// Write this
	old_p_level = splhigh();
	P(lock->lk_sem);
	KASSERT(lock->lk_sem->sem_count == 0);
	if (!CURCPU_EXISTS()) {
		panic("CPU DOENS'T EXIST!");
	}
	lock->lk_holder = curcpu->c_curthread;
	KASSERT(lock->lk_holder != NULL);
	splx(old_p_level);

	/* Call this (atomically) once the lock is acquired */
	old_p_level = splhigh();
	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
	splx(old_p_level);
}

void
lock_release(struct lock *lock)
{
	int old_p_level;

	// Write this
	old_p_level = splhigh();
	V(lock->lk_sem);
	KASSERT(lock->lk_sem->sem_count == 1);
	lock->lk_holder = NULL;
	splx(old_p_level);

	/* Call this (atomically) when the lock is released */
	old_p_level = splhigh();
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
	splx(old_p_level);
}

bool
lock_do_i_hold(struct lock *lock)
{
	
	// Write this
	if (!CURCPU_EXISTS()){
		return false;
	}
	return(lock->lk_holder == curcpu->c_curthread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	// add stuff here as needed
	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_lock);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);
	spinlock_acquire(&cv->cv_lock);
	KASSERT(wchan_isempty(cv->cv_wchan, &cv->cv_lock));

	// add stuff here as needed
	wchan_destroy(cv->cv_wchan);
	spinlock_release(&cv->cv_lock);
	spinlock_cleanup(&cv->cv_lock);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);
	KASSERT(spinlock_do_i_hold(&cv->cv_lock));

	lock_release(lock);

	wchan_sleep(cv->cv_wchan, &cv->cv_lock);
	spinlock_release(&cv->cv_lock);
	KASSERT(!spinlock_do_i_hold(&cv->cv_lock));

	lock_acquire(lock);
	KASSERT(lock_do_i_hold(lock));
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);
	KASSERT(spinlock_do_i_hold(&cv->cv_lock));
	
	wchan_wakeone(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
	KASSERT(!spinlock_do_i_hold(&cv->cv_lock));	
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);
	KASSERT(spinlock_do_i_hold(&cv->cv_lock));

	wchan_wakeall(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
	KASSERT(!spinlock_do_i_hold(&cv->cv_lock));	
}

////////////////////////////////////////////////////////////
//
// RW-lock
// Source : https://arxiv.org/pdf/1309.4507.pdf
struct rwlock *
rwlock_create(const char *name)
{
	struct rwlock *rwlock;

	// malloc for struct
	rwlock = kmalloc(sizeof(*rwlock));
	if (rwlock == NULL) {
		return NULL;
	}

	// assign name
	rwlock->rwlock_name = kstrdup(name);
	if (rwlock->rwlock_name==NULL) {
		kfree(rwlock);
		return NULL;
	}

	// init semaphores (in, out, wrt)
	rwlock->in = sem_create("in", 1);
	if (rwlock->in == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->out = sem_create("out", 1);
	if (rwlock->out == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		sem_destroy(rwlock->in);
		return NULL;
	}

	rwlock->wrt = sem_create("wrt", 0);
	if (rwlock->wrt == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		sem_destroy(rwlock->in);
		sem_destroy(rwlock->out);
		return NULL;
	}

	rwlock->isWriterWaiting = false;
	rwlock->ctrin = 0;
	rwlock->ctrout = 0;

	return rwlock;
}
void rwlock_destroy(struct rwlock *rwlock)
{
	sem_destroy(rwlock->in);
	sem_destroy(rwlock->out);
	sem_destroy(rwlock->wrt);
	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

void rwlock_acquire_read(struct rwlock *rwlock)
{	
	int old_p_level;
	old_p_level = splhigh();

	// Wait in
	P(rwlock->in);
	// ctrin++
	rwlock->ctrin++;
	// Signal in
	V(rwlock->in);

	splx(old_p_level);
}
void rwlock_release_read(struct rwlock *rwlock)
{
	int old_p_level;
	old_p_level = splhigh();

	// Wait out
	P(rwlock->out);
	// ctrout++
	rwlock->ctrout++;
	// if (wait==1 && ctrin == ctrout)
	// then Signal wrt
	if (rwlock->isWriterWaiting && (rwlock->ctrin == rwlock->ctrout)){
		V(rwlock->wrt);
	}
	// Sig out
	V(rwlock->out);

	splx(old_p_level);
}
void rwlock_acquire_write(struct rwlock *rwlock)
{
	int old_p_level;
	old_p_level = splhigh();
	
	// Wait in
	P(rwlock->in);
	// Wait out
	P(rwlock->out);
	// if (ctrin==ctrout)
	// then Sig out
	// else
	//		wait=1
	//		Sig out
	// 		Wait wrt
	// 		wait=0
	if (rwlock->ctrin == rwlock->ctrout) {
		V(rwlock->out);
	}
	else {
		rwlock->isWriterWaiting = true;
		V(rwlock->out);
		P(rwlock->wrt);
		rwlock->isWriterWaiting = false;
	}
	splx(old_p_level);
}
void rwlock_release_write(struct rwlock *rwlock)
{
	int old_p_level;
	old_p_level = splhigh();
	// Sig in
	V(rwlock->in);
	splx(old_p_level);
}
