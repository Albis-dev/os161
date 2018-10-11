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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

// lock for quadrants
// need to be acquired just before a car enters a quadrant
// need to be released right after a car leaves a quadrant
/*
 * Example)
 * 
 * lock_acquire(q0);
 * inQuadrant(0);
 * ...
 * lock_acquire(q1); 
 * inQuadrant(1);
 * lock_release(q0);
 * ...
 * leaveInteresction();
 * lock_release(q1);
 */ 

static struct lock *lk_q0 = NULL;
static struct lock *lk_q1 = NULL;
static struct lock *lk_q2 = NULL;
static struct lock *lk_q3 = NULL;
static struct lock *lk_q[4];

// no more than 3 cars can concurrently exist on the road.
static struct lock *lk_car = NULL;
static int car = 0;

// cv for entrance
static struct cv *cv_entrance = NULL;

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {
	lk_q0 = lock_create("q0");
	lk_q1 = lock_create("q1");
	lk_q2 = lock_create("q2");
	lk_q3 = lock_create("q3");
	lk_q[0] = lk_q0;
	lk_q[1] = lk_q1;
	lk_q[2] = lk_q2;
	lk_q[3] = lk_q3;

	lk_car = lock_create("car");

	cv_entrance = cv_create("entrance");

	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(lk_q0);
	lock_destroy(lk_q1);
	lock_destroy(lk_q2);
	lock_destroy(lk_q3);
	lock_destroy(lk_car);
	cv_destroy(cv_entrance);

	return;
}

void
welcome(){
	// Welcome to our intersection.
	// Let me see if there's a room for you.
	lock_acquire(lk_car);
	if (car == 3) {
		// Sorry we're full now. Please wait for a bit.
		cv_wait(cv_entrance, lk_car);
	}
	// We found a spot. You're good to go!
	KASSERT(car < 3);
	car++;
	lock_release(lk_car);
	
	return;
}

void
bye(){
	lock_acquire(lk_car);
	car--;
	KASSERT(car < 3);
	cv_signal(cv_entrance, lk_car);
	lock_release(lk_car);

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	/*
	 *	Route :
	 *  	1st Quadrant -> Leave Intersection
	 */
	int first_q = direction;

	welcome();

	lock_acquire(lk_q[first_q]); 	// "I'm gonna enter the first quadrant."
	inQuadrant(direction, index); 	// "I'm in."
	
	leaveIntersection(index);		// "I'm out. Bye!"
	lock_release(lk_q[first_q]); 	// "You can use the first one."

	bye();

	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{	
	/*
	 *	Route :
	 *  	1st Quadrant -> 2nd Quadrant -> Leave Intersection
	 */

	int first_q = direction;
	int second_q = (direction + 3) % 4;

	welcome();

	lock_acquire(lk_q[first_q]); 	// "I'm gonna enter the first quadrant."
	inQuadrant(first_q, index); 	// "I'm in."

	lock_acquire(lk_q[second_q]); 	// "I'm gonna enter the second quadrant."
	inQuadrant(second_q, index); 	// "I'm in."

	lock_release(lk_q[first_q]); 	// "You can use the first one."
	
	leaveIntersection(index); 		// "I'm out. Bye!"

	lock_release(lk_q[second_q]); 	// "You can use the second one."
	
	bye();

	return;

}
void
turnleft(uint32_t direction, uint32_t index)
{
	/*
	 *	Route :
	 *  	1st Quadrant -> 2nd Quadrant 
	 * 		-> 3rd Quadrant -> Leave Intersection
	 */
	int first_q = direction;
	int second_q = (direction + 3) % 4;
	int third_q = (direction + 2) % 4;

	welcome();

	lock_acquire(lk_q[first_q]); 	// "I'm gonna enter the first quadrant."
	inQuadrant(first_q, index); 	// "I'm now in the first quadrant."

	lock_acquire(lk_q[second_q]); 	// "I'm gonna enter the second quadrant."
	inQuadrant(second_q, index);	// "I'm now in the second quadrant."

	lock_release(lk_q[first_q]); 	// "You can use the first one."

	lock_acquire(lk_q[third_q]); 	// "I'm gonna enter the third quadrant."
	inQuadrant(third_q, index); 	// "I'm now in the third quadrant."

	lock_release(lk_q[second_q]);   // "You can use the second one."
	
	leaveIntersection(index); 		// "I'm out. Bye!"
	lock_release(lk_q[third_q]);	// "You can use the third one."

	bye();

	return;
}
