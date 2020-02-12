#pragma once
#include <thread>
#include "threadsafe_queue.hpp"
#include "string.hpp"

// Set description of current thread (mainly for debugging)
// allows for easy overview of threads in debugger
void set_thread_description (std::string const& description);

// std::thread::hardware_concurrency() gets the number of cpu threads

// set priority of gameloop thread to be higher than normal threads to hopefully avoid cpu spiked which cause framedrops
void set_gameloop_thread_priority ();

// threadpool
// threadpool.push(Job) to queue a job for execution on a thread
// threads call Job.execute() and push the return value into threadpool.results
// threadpool.try_pop() to get results
template <typename Job>
class Threadpool {
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
	typedef decltype(std::declval<Job>().execute()) Result;

	// jobs.push(Job) to queue work to be executed by a thread
	ThreadsafeQueue<Job>		jobs;
	// jobs.try_pop(Job) to dequeue the results of the jobs
	ThreadsafeQueue<Result>	results;

	// don't start threads
	Threadpool () {}
	// start thread_count threads
	Threadpool (int thread_count, std::string thread_base_name="<threadpool>") {
		start_threads(thread_count, thread_base_name);
	}

	// start thread_count threads
	void start_threads (int thread_count, std::string thread_base_name="<threadpool>") {
		for (int i=0; i<thread_count; ++i) {
			threads.emplace_back( &Threadpool::thread_main, this, kiss::prints("%s #%d", thread_base_name.c_str(), i) );
		}
	}

	// no copy or move of this class can be allowed, because the threads that might be running have the 'this' pointer
	Threadpool (Threadpool const& other) = delete;
	Threadpool (Threadpool&& other) = delete;
	Threadpool& operator= (Threadpool const& other) = delete;
	Threadpool& operator= (Threadpool&& other) = delete;

	~Threadpool () {
		jobs.shutdown(); // set shutdown to all threads

		for (auto& t : threads)
			t.join(); // wait for all threads to exit thread_main
	}

	int thread_count () {
		return (int)threads.size();
	}
};
