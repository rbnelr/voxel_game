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
	// can be called from multiple threads (multiple producer)
	void push (T elem) {
		LOCK_GUARD;

		q.emplace_back( std::move(elem) );
		c.notify_one();
	}

	// wait to dequeue one element from the queue
	// can be called from multiple threads (multiple consumer)
	T pop () {
		UNIQUE_LOCK;

		while(q.empty()) {
			c.wait(lock); // release lock as long as the wait and reaquire it afterwards.
		}

		T val = std::move(q.front());
		q.pop_front();
		return val;
	}

	// wait to dequeue one element from the queue or until shutdown is set
	// returns if element was popped or shutdown was set as enum
	// can be called from multiple threads (multiple consumer)
	enum { POP, SHUTDOWN } pop_or_shutdown (T* out) {
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

	// deque multiple elements from the queue if there is at least one and return true
	// or return false of queue is empty
	// can be called from multiple threads (multiple consumer)
	// (pushed on the back of results)
	bool pop_all (std::vector<T>* results) {
		LOCK_GUARD;

		if (q.empty())
			return false;

		while (!q.empty()) {
			results->push_back(std::move(q.front()));
			q.pop_front();
		}
		return true;
	}
	// deque multiple elements from the queue (or zero if queue is empty) 
	// can be called from multiple threads (multiple consumer)
	std::vector<T> pop_all () {
		std::vector<T> results;
		pop_all(&results);
		return results;
	}

	// set shutdown which all consumers can recieve via pop_or_shutdown
	void shutdown () {
		LOCK_GUARD;

		shutdown_flag = true;
		c.notify_all();
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
