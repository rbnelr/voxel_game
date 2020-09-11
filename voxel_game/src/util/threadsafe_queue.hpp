#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <vector>
#include "Tracy.hpp"

// Need to wrap locks for tracy
#define MUTEX				TracyLockableN(std::mutex,	m, "ThreadsafeQueue mutex")
#define CONDITION_VARIABLE	std::condition_variable_any	c

#define UNIQUE_LOCK			std::unique_lock<LockableBase(std::mutex)> lock(m)
#define LOCK_GUARD			std::lock_guard<LockableBase(std::mutex)> lock(m)


// based on https://stackoverflow.com/questions/15278343/c11-thread-safe-queue

// multiple producer multiple consumer threadsafe queue with T item
template <typename T>
class ThreadsafeQueue {
	//mutable std::mutex		m;
	MUTEX;
	CONDITION_VARIABLE;

	// used like a queue but using a deque to support iteration (a queue is just wrapper around a deque, so it is not less efficient to use a deque over a queue)
	std::deque<T>			q;

	// use is optional (makes sense to use this to stop threads of thread pools (ie. use this on the job queue), but does not make sense to use this on the results queue)
	bool					shutdown_flag = false;

public:
	// simply push one element onto the queue
	void push (T elem) {
		LOCK_GUARD;

		q.emplace_back( std::move(elem) );
		c.notify_one(); // TODO: It seems like it might be possible to unlock the mutex and then notify_one, to maybe reduce sync overhead, but i was not entirely convinced that this is safe https://stackoverflow.com/questions/17101922/do-i-have-to-acquire-lock-before-calling-condition-variable-notify-one
	}

	// push multiple elements onto the queue
	void push_n (T* elem, size_t count) {
		LOCK_GUARD;

		for (size_t i=0; i<count; ++i) {
			q.emplace_back( std::move(elem[i]) );
			c.notify_one();
		}
	}

	// wait to dequeue one element from the queue
	// can be called from multiple threads (multiple consumer)
	T pop_wait () {
		UNIQUE_LOCK;

		while(q.empty()) {
			c.wait(lock); // release lock as long as the wait and reaquire it afterwards.
		}

		T val = std::move(q.front());
		q.pop_front();
		return val;
	}

	// deque one element from the queue if there is one
	// can be called from multiple threads (multiple consumer)
	bool try_pop (T* out) {
		LOCK_GUARD;

		if (q.empty())
			return false;

		*out = std::move(q.front());
		q.pop_front();
		return true;
	}

	// wait until min elements are available, then dequeue up to max elements
	// writes the elements into their repective indicies in output
	// returns the number of elements dequeued
	size_t pop_n_wait (T output[], size_t min, size_t max) {
		UNIQUE_LOCK;

		while (q.size() < min) {
			c.wait(lock); // release lock as long as the wait and reaquire it afterwards.
		}

		size_t count = std::min(q.size(), max);
		for (size_t i=0; i<count; ++i) {
			output[i] = std::move(q.front());
			q.pop_front();
		}

		return count;
	}

	// dequeue up to max elements (or none); never waits
	// writes the elements into their repective indicies in output
	// returns the number of elements dequeued
	size_t pop_n (T output[], size_t max) {
		LOCK_GUARD;

		size_t count = std::min(q.size(), max);
		for (size_t i=0; i<count; ++i) {
			output[i] = std::move(q.front());
			q.pop_front();
		}

		return count;
	}

	// wait until min elements are available, then dequeue all elements
	// returns the number of elements dequeued
	size_t pop_all_wait (std::vector<T>* output, size_t min) {
		UNIQUE_LOCK;

		while (q.size() < min) {
			c.wait(lock); // release lock as long as the wait and reaquire it afterwards.
		}

		size_t count = q.size();
		output->reserve(count);

		for (size_t i=0; i<count; ++i) {
			output->emplace_back( std::move(q.front()) );
			q.pop_front();
		}

		return count;
	}

	// dequeue all elements (including none); never waits
	// returns the number of elements dequeued
	size_t pop_all (std::vector<T>* output) {
		LOCK_GUARD;

		size_t count = q.size();
		output->reserve(count);

		for (size_t i=0; i<count; ++i) {
			output->emplace_back( std::move(q.front()) );
			q.pop_front();
		}

		return count;
	}

	// wait to dequeue one element from the queue or until shutdown is set
	// returns if element was popped or shutdown was set as enum
	// can be called from multiple threads (multiple consumer)
	enum { POP, SHUTDOWN } pop_or_shutdown_wait (T* out) {
		UNIQUE_LOCK;

		while(!shutdown_flag && q.empty()) {
			c.wait(lock); // release lock as long as the wait and reaquire it afterwards.
		}
		if (shutdown_flag)
			return SHUTDOWN;

		*out = std::move(q.front());
		q.pop_front();
		return POP;
	}

	// set shutdown which all consumers can recieve via pop_or_shutdown
	void shutdown () {
		LOCK_GUARD;

		shutdown_flag = true;
		c.notify_all();
	}
	void reset_shutdown () {
		shutdown_flag = false;
	}

	// iterate queued items with template callback 'void func (T&)' in order from the oldest to the newest pushed
	// elements are allowed to be changed
	template <typename FOREACH>
	void iterate_queue (FOREACH callback) {
		LOCK_GUARD;

		for (auto it=q.begin(); it!=q.end(); ++it) {
			callback(*it);
		}
	}

	// iterate queued items with template callback 'void func (T&)' in order from the newest to the oldest pushed
	// elements are allowed to be changed
	template <typename FOREACH>
	void iterate_queue_newest_first (FOREACH callback) { // front == next to be popped, back == most recently pushed
		LOCK_GUARD;

		for (auto it=q.rbegin(); it!=q.rend(); ++it) {
			callback(*it);
		}
	}

	// remove items if template callback 'bool func (T&)' returns true
	// useful to be able to cancel queued jobs in a threadpool
	template <typename NEED_TO_CANCEL>
	void remove_if (NEED_TO_CANCEL need_to_cancel) {
		LOCK_GUARD;

		for (auto it=q.begin(); it!=q.end();) {
			if (need_to_cancel(*it)) {
				it = q.erase(it);
			} else {
				++it;
			}
		}
	}

	void clear () {
		LOCK_GUARD;

		q.clear();
	}

	template <typename COMPARATOR>
	void sort (COMPARATOR cmp) {
		LOCK_GUARD;

		std::sort(q.begin(), q.end(), cmp);
	}
};

#undef MUTEX				
#undef CONDITION_VARIABLE
#undef UNIQUE_LOCK		
#undef LOCK_GUARD		
