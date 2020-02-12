#pragma once
#include <thread>
#include "threadsafe_queue.hpp"

// Set description of current thread (mainly for debugging)
// allows for easy overview of threads in debugger
void set_thread_description (std::string const& description);

// threadpool
// threadpool.push(Job) to queue a job for execution on a thread
// threads call Job.execute() and push the return value into threadpool.results
// threadpool.try_pop() to get results
template <typename Job>
class threadpool {
	std::vector< std::thread >	threads;

	void thread_main (std::string thread_name) { // thread_name mainly for debugging
		set_thread_description(thread_name);

		// Wait for one job to pop and execute or until shutdown signal is sent via jobs.shutdown()
		Job job;
		while (jobs.pop_or_shutdown(&job) != decltype(jobs)::SHUTDOWN) {
			results.push(job.execute());
		}
	}

public:
	typedef decltype(Job.execute()) Result;

	// jobs.push(Job) to queue work to be executed by a thread
	threadsafe_queue<Job>		jobs;
	// jobs.try_pop(Job) to dequeue the results of the jobs
	threadsafe_queue<Result>	results;

	// don't start threads
	threadpool () {}
	// start thread_count threads
	threadpool (int thread_count, std::string thread_base_name="<threadpool>") {
		start_threads(thread_count, thread_base_name);
	}

	// start thread_count threads
	void start_threads (int thread_count, std::string thread_base_name="<threadpool>") {
		for (int i=0; i<thread_count; ++i) {
			threads.emplace_back( &threadpool::thread_main, this, prints("%s #%d", thread_base_name.c_str(), i) );
		}
	}

	// no copy or move of this class can be allowed, because the threads that might be running have the 'this' pointer
	threadpool (threadpool const& other) = delete;
	threadpool (threadpool&& other) = delete;
	threadpool& operator= (threadpool const& other) = delete;
	threadpool& operator= (threadpool&& other) = delete;

	~threadpool () {
		jobs.shutdown(); // set shutdown to all threads

		for (auto& t : threads)
			t.join(); // wait for all threads to exit thread_main
	}

	int thread_count () {
		return (int)threads.size();
	}
};
