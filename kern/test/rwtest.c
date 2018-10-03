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

/*
 * You can stress-test the R/W lock implementation by increasing LOOPCOUNT.
 * But make sure to set it back to 1 when you are going to run test161.
 */
#define NTHREADS 32
#define LOOPCOUNT 1

/*
 * Use these stubs to test your reader-writer locks.
 */

// Global counter variable for testing mutual-exclusion features.
static volatile unsigned long ctr;

// Synchronization primitives
struct rwlock *rwlock = NULL;
struct semaphore *donesem = NULL;

/* 
 * Since thread_fork() returns its own return code,
 * if a function running on a forked thread wants to return some integer value,
 * it will store its return code to this global variable.    
 */
static unsigned long virtual_rc;

/*
 * Read ctr while the rwlock is held.
 */
void reader(void *junk, unsigned long j) {
	(void)junk;
	(void)j;

	random_yielder(10);

	virtual_rc = 0;
	
	rwlock_acquire_read(rwlock);
	kprintf(".");
	virtual_rc = ctr; // the "read" part
	KASSERT(virtual_rc == ctr); // must have same value
	rwlock_release_read(rwlock);

	V(donesem);
}

/*
 * Increment ctr by 1.
 */
void writer(void *junk, unsigned long j) {
	(void)junk;
	(void)j;

	random_yielder(10);

	unsigned long old_ctr;

	rwlock_acquire_read(rwlock);
	kprintf("*");
	old_ctr = ctr;
	ctr++;
	KASSERT(ctr - old_ctr == 1);
	rwlock_release_read(rwlock);

	V(donesem);
}

/*
 * Initialize all the synchronization primitives.
 */
void synch_init() {
	ctr = 0;
	KASSERT(ctr == 0);

	rwlock = rwlock_create("testlock");
	KASSERT(rwlock != NULL);
	
	donesem = sem_create("donesem", 0);
}

/*
 * Destroy all the synchronization primitives.
 */
void synch_destroy() {
	rwlock_destroy(rwlock);
	sem_destroy(donesem);
	rwlock = NULL;
	donesem = NULL;
}

/*
 * (rwt1) RW TEST 1
 *  
 * - WHAT IT DOES
 * 1. Initialize all the synchronization primitives.
 * 2. Make a race condition between reader() and writer() then let them run.
 * 	  (reader() will panic if R/W lock is not working properly)
 * 3. Repeat LOOPCOUNT times.
 * 
 * - PURPOSE
 * This test checks the basic mutual exclusion feature of R/W lock.
 */
int test1() {
	int result; 

	synch_init();

	for (int j=0; j<LOOPCOUNT; j++) {
		for (int i=0; i<NTHREADS; i++) {
			result = thread_fork("rwt1", NULL, writer, NULL, 0);
			if (result) {
				panic("FORK FAILED");
			}
		}

		result = thread_fork("rwt1", NULL, reader, NULL, 0);
		if (result) {
			panic("FORK FAILED");
		}
		
		for (int i=0; i<NTHREADS+1; i++) {
			P(donesem);
		}

		kprintf_n(" reader() : %lu\n\n", virtual_rc);
	}

	synch_destroy();

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

/*
 * (rwt2) RW TEST 2
 *  
 * - WHAT IT DOES
 * 1. Fork NTHREADS threads running reader().
 * 2. Fork one thread running writer().
 * 3. Fork NTHREADS threads running reader(). (again!)
 * 4. Repeat this LOOPCOUNT times.
 * 
 * - PURPOSE
 * This test checks if the writer doesn't starve under extreme circumstance.
 */
int test2() {
	int result;

	synch_init();

	for(int j=0; j<LOOPCOUNT; j++) {
		for (int i=0; i<NTHREADS; i++) {
			result = thread_fork("rwt2", NULL, reader, NULL, 0);
			if (result) {
				panic("FORK FAILED");
			}
		}
		result = thread_fork("rwt2", NULL, writer, NULL, 0);
		if (result) {
			panic("FORK FAILED");
		}
		for (int i=0; i<NTHREADS; i++) {
			result = thread_fork("rwt2", NULL, reader, NULL, 0);
			if (result) {
				panic("FORK FAILED");
			}
		}

		for (int i=0; i<NTHREADS*2+1; i++) {
			P(donesem);
		}
	}

	KASSERT(ctr == LOOPCOUNT);

	synch_destroy();

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	test2();

	success(TEST161_SUCCESS, SECRET, "rwt2");

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
