/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

#include <current.h>

#define LOOPCOUNT 1000
/*
 * Use these stubs to test your reader-writer locks.
 */

// Global counter variable for testing mutual-exclusion features.
static volatile unsigned long ctr;

// Synchronization primitives
static struct rwlock *rwlock = NULL;
static struct cv *cv = NULL;
static struct lock *lock = NULL;

/* 
 * Since thread_fork() returns its own return code,
 * if a function running on a forked thread wants to return some integer value,
 * it will store its return code to this global variable.    
 */
static unsigned long virtual_rc;

/*
 * A helper function.
*/
void reader(void *junk, unsigned long j) {
	(void)junk;
	(void)j;

	virtual_rc = 0;
	
	rwlock_acquire_read(rwlock);
	virtual_rc = ctr;
	KASSERT(virtual_rc == ctr);
	rwlock_release_read(rwlock);

	lock_acquire(lock);
	cv_signal(cv, lock);
	lock_release(lock);
}

/*
 * A helper function.
*/
void writer(void *junk, unsigned long j) {
	(void)junk;
	(void)j;

	unsigned long old_ctr;

	rwlock_acquire_read(rwlock);
	old_ctr = ctr;
	ctr++;
	KASSERT(ctr - old_ctr == 1);
	rwlock_release_read(rwlock);

	lock_acquire(lock);
	cv_signal(cv, lock);
	lock_release(lock);
}

/*
 * (rwt1) RW TEST 1
 *  
 * - WHAT IT DOES
 * 1. Initialize all the synchronization primitives.
 * 2. Make "ctr" to LOOPCOUNT by using writer() helper function.
 * 3. Check if ctr == LOOPCOUNT
 * 
 * - PURPOSE
 * This test checks the basic mutual exclusion feature of R/W lock.
 */
int test1() {
	ctr = 0;
	KASSERT(ctr == 0);

	rwlock = rwlock_create("testlock");
	KASSERT(rwlock != NULL);

	cv = cv_create("rwtesthread");
	KASSERT(cv != NULL);

	lock = lock_create("rwtestthread");
	KASSERT(lock != NULL);

	for (int i=0; i<LOOPCOUNT; i++) {
		thread_fork("rwt1", NULL, writer, NULL, 0);
	}
	while (ctr != LOOPCOUNT) {
		lock_acquire(lock);
		cv_wait(cv, lock);
		lock_release(lock);
	}
	KASSERT(ctr == LOOPCOUNT);

	thread_fork("rwt1", NULL, (void(*))reader, NULL, 0);
	lock_acquire(lock);
	cv_wait(cv, lock);
	lock_release(lock);

	kprintf_n("READER GETS : %lu\n\n", virtual_rc);
	kprintf_n("ACTUAL COUNTER IS : %lu\n\n", ctr);

	if (virtual_rc != LOOPCOUNT || ctr != LOOPCOUNT) {
		panic("TEST FAILED!");
	}
	
	return 0;
}

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("This test panics when it fails\n");

	test1();

	success(TEST161_SUCCESS, SECRET, "rwt1");	

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
