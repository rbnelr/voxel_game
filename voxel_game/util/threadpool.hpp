#pragma once
#include <thread>
#include "threadsafe_queue.hpp"
#include "string.hpp"
#include "optick.h"

// std::thread::hardware_concurrency() gets the number of cpu threads

// Is is probaby reasonable to set a game process priority to high, so that background apps don't interfere with the games performance too much,
// As long as we don't use 100% of the cpu the background apps should run fine, and we might have less random framedrops from being preempted
void set_process_high_priority ();

// used to set priority of gameloop thread to be higher than normal threads to hopefully avoid cpu spiked which cause framedrops and so that background worker threads are sheduled like they should be
// also used in threadpool threads when 'high_prio' is true to allow non-background 'parallellism', ie. starting work that needs to be done this frame and having the render thread and the threadpool work on it at the same time
//  preempting the background threadpool and hopefully being done in time
void set_thread_high_priority ();

// Set a desired cpu core for the current thread to run on
void set_thread_preferred_core (int core_index);

// Set description of current thread (mainly for debugging)
// allows for easy overview of threads in debugger
void set_thread_description (std::string_view description);

// threadpool
// threadpool.push(Job) to queue a job for execution on a thread
// threads call Job.execute() and push the return value into threadpool.results
// threadpool.try_pop() to get results
template <typename Job>
class Threadpool {
	std::vector< std::thread >	threads;

	void thread_main (std::string thread_name, bool high_prio, int preferred_core) { // thread_name mainly for debugging
		OPTICK_THREAD(thread_name.c_str()); // save since, OPTICK_THREAD copies the thread name

		if (high_prio)
			set_thread_high_priority();
		set_thread_preferred_core(preferred_core);
		set_thread_description(thread_name);

		// Wait for one job to pop and execute or until shutdown signal is sent via jobs.shutdown()
		Job job;
		while (jobs.pop_or_shutdown(&job) != decltype(jobs)::SHUTDOWN) {
			OPTICK_EVENT("Threadpool execute job");

			results.push(job.execute());
		}
	}

public:
	typedef decltype(std::declval<Job>().execute()) Result;

	// jobs.push(Job) to queue work to be executed by a thread
	ThreadsafeQueue<Job>	jobs;
	// jobs.try_pop(Job) to dequeue the results of the jobs
	ThreadsafeQueue<Result>	results;

	// don't start threads
	Threadpool () {}
	// start thread_count threads
	Threadpool (int thread_count, bool high_prio=false, std::string thread_base_name="<threadpool>") {
		start_threads(thread_count, high_prio, thread_base_name);
	}

	// start thread_count threads
	void start_threads (int thread_count, bool high_prio=false, std::string thread_base_name="<threadpool>") {
		// Threadpools are ideally used with  thread_count <= cpu_core_count  to make use of the cpu without the threads preempting each other (although I don't check the thread count)
		// I'm just assuming for now that with  high_prio==false -> thread_count==cpu_core_count -> ie. set each threads affinity to one of the cores
		//  and with high_prio==true you leave at least one core free for the main thread, so we assign the cores 1-n to the threads so that the main thread can be on core 0
		int cpu_core = high_prio ? 1 : 0;

		for (int i=0; i<thread_count; ++i) {
			threads.emplace_back( &Threadpool::thread_main, this, kiss::prints("%s #%d", thread_base_name.c_str(), i), high_prio, cpu_core++);
		}
	}

	// can be called from the producer thread to work on the jobs itself
	// useful when the producer needs to wait for the jobs to be done anyway
	// returns when jobs queue is empty, ie. all jobs are being processed
	/* pattern:
		main_thread:
			
			for (job : jobs)
				threadpool.jobs.push(job)

			threadpool.contribute_work()

			for (i in range jobs.count())
				res = threadpool.results.pop()
	*/
	void contribute_work () {
		OPTICK_EVENT("Threadpool contribute_work");
		
		// Wait for one job to pop and execute or until shutdown signal is sent via jobs.shutdown()
		Job job;
		while (jobs.try_pop(&job)) {
			results.push(job.execute());
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
