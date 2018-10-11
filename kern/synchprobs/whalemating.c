/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

// counter for each role
static volatile int male_count;
static volatile int female_count;
static volatile int matchmaker_count;

// general purpose lock
static struct lock *lock = NULL;
// use this cv to wake things up when the match is ready
static struct cv *male_lobby = NULL;
static struct cv *female_lobby = NULL;
static struct cv *matchmaker_lobby = NULL;

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	// counter
	male_count = 0;
	female_count = 0;
	matchmaker_count = 0;
	// gerneral purpose lock
	lock = lock_create("lock");
	// cv for each role
	male_lobby = cv_create("male_lobby");
	female_lobby = cv_create("female_lobby");
	matchmaker_lobby = cv_create("matchmaker_lobby");

	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	lock_destroy(lock);
	lock = NULL;

	cv_destroy(male_lobby);
	male_lobby = NULL;
	cv_destroy(female_lobby);
	female_lobby = NULL;
	cv_destroy(matchmaker_lobby);
	matchmaker_lobby = NULL;

	return;
}

void
male(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */
	male_start(index);

	lock_acquire(lock);
	male_count++; // new male whale has arrived!
	// let me see if other whales are ready to play.
	if (female_count > 0 && matchmaker_count > 0) {
		// seems like they're waiting for you!
		cv_signal(female_lobby, lock);
		cv_signal(matchmaker_lobby, lock);
	}
	else {
		// seems like they're not here yet...
		cv_wait(male_lobby, lock); // please wait for a bit
	}
	male_count--; // bye!
	lock_release(lock);

	// and have fun
	male_end(index);

	return;
}

void
female(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */
	female_start(index);

	lock_acquire(lock);
	female_count++; // new female whale has arrived!
	// let me see if other whales are ready to play.
	if (male_count > 0 && matchmaker_count > 0) {
		// seems like they're waiting for you!
		cv_signal(male_lobby, lock);
		cv_signal(matchmaker_lobby, lock);
	}
	else {
		// seems like they're not here yet...
		cv_wait(female_lobby, lock); // please wait for a bit
	}
	female_count--; // bye!
	lock_release(lock);

	// and have fun
	female_end(index);

	return;
}

void
matchmaker(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */
	matchmaker_start(index);

	lock_acquire(lock);
	matchmaker_count++; // new matchmaker whale has arrived!
	// let me see if other whales are ready to play.
	if (male_count > 0 && female_count > 0) {
		// seems like they're waiting for you!
		cv_signal(male_lobby, lock);
		cv_signal(female_lobby, lock);
	}
	else {
		// seems like they're not here yet...
		cv_wait(matchmaker_lobby, lock); // please wait for a bit
	}
	matchmaker_count--; // bye!
	lock_release(lock);

	// and have fun
	matchmaker_end(index);

	return;
}
