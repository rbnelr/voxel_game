#pragma once
#include <thread>
#include "macros.hpp"
#include "threadsafe_queue.hpp"
#include "string.hpp"

#ifdef TRACY_ENABLE
	#include "tracy/Tracy.hpp"
	
	#define THREADPOOL_PROFILER_SCOPED(name) ZoneScopedN(name)
#else
	#define THREADPOOL_PROFILER_SCOPED(name)
#endif

enum ThreadPrio {
	TPRIO_MAIN,
	TPRIO_PARALLELISM,
	TPRIO_BACKGROUND,
};

// std::thread::hardware_concurrency() gets the number of cpu threads

// Is is probaby reasonable to set a game process priority to above_normal, so that background apps don't interfere with the games performance too much,
// As long as we don't use 100% of the cpu the background apps should run fine, and we might have less random framedrops from being preempted
void set_process_priority ();

// used to set priority of gameloop thread to be higher than normal threads to hopefully avoid cpu spiked which cause framedrops and so that background worker threads are sheduled like they should be
// also used in threadpool threads when 'high_prio' is true to allow non-background 'parallellism', ie. starting work that needs to be done this frame and having the render thread and the threadpool work on it at the same time
//  preempting the background threadpool and hopefully being done in time
void set_thread_priority (ThreadPrio prio);

// Set a desired cpu core for the current thread to run on
void set_thread_preferred_core (int core_index);

// Set description of current thread (mainly for debugging)
// allows for easy overview of threads in debugger
void set_thread_description (std::string_view description);

// Job threadpool
// threadpool.push(Job) to queue a job for execution on a thread
// threads call Job.execute() and std::move() the return value into threadpool.results
// threadpool.try_pop() to get results
// jobs and job results should be default constructable and small and moveable
template <typename JOB>
class Threadpool {
	NO_MOVE_COPY_CLASS(Threadpool)

	std::vector< std::thread >	threads;

	void thread_main (std::string thread_name, ThreadPrio prio/*, int preferred_core*/) { // thread_name mainly for debugging
		set_thread_priority(prio);

		//set_thread_preferred_core(preferred_core);
		set_thread_description(thread_name);

		// Wait for one job to pop and execute or until shutdown signal is sent via jobs.shutdown()
		for (;;) {
			std::unique_ptr<JOB> job;
			if (jobs.pop_or_shutdown_wait(&job) == decltype(jobs)::SHUTDOWN)
				return;

			job->execute();
			results.push(std::move(job));
		}
	}

	std::string thread_base_name;
	ThreadPrio prio;

public:
	// jobs.push(Job) to queue work to be executed by a thread
	ThreadsafeQueue<std::unique_ptr<JOB>> jobs;
	// jobs.try_pop(Job) to dequeue the results of the jobs
	ThreadsafeQueue<std::unique_ptr<JOB>> results;

	// don't start threads
	Threadpool () {}
	// start thread_count threads
	Threadpool (int thread_count, ThreadPrio prio, std::string thread_base_name="<threadpool>") {
		start_threads(thread_count, prio, std::move(thread_base_name));
	}

	// start thread_count threads
	void start_threads (int thread_count, ThreadPrio prio, std::string thread_base_name="<threadpool>") {
		THREADPOOL_PROFILER_SCOPED("Threadpool::start_threads");
		
		//// Threadpools are ideally used with  thread_count <= cpu_core_count  to make use of the cpu without the threads preempting each other (although I don't check the thread count)
		//// I'm just assuming for now that with  high_prio==false -> thread_count==cpu_core_count -> ie. set each threads affinity to one of the cores
		////  and with high_prio==true you leave at least one core free for the main thread, so we assign the cores 1-n to the threads so that the main thread can be on core 0
		//int cpu_core = prio == ThreadPriority::HIGH ? 1 : 0;

		for (int i=0; i<thread_count; ++i) {
			threads.emplace_back( &Threadpool::thread_main, this, kiss::prints("%s #%d", thread_base_name.c_str(), i), prio/*, cpu_core++*/);
		}

		this->thread_base_name = std::move(thread_base_name);
		this->prio = prio;
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
		THREADPOOL_PROFILER_SCOPED("Threadpool::contribute_work");

		// Wait for one job to pop and execute or until shutdown signal is sent via jobs.shutdown()
		for (;;) {
			JOB job;
			if (!jobs.try_pop(&job))
				return;

			job.execute();
			results.push(std::move(job));
		}
	}

	// optional manual shutdown
	void shutdown () {
		THREADPOOL_PROFILER_SCOPED("Threadpool::shutdown");

		if (!threads.empty())
			jobs.shutdown(); // set shutdown to all threads

		for (auto& t : threads)
			t.join(); // wait for all threads to exit thread_main

		jobs.reset_shutdown();

		threads.clear();
		jobs.clear();
		results.clear();
	}

	void flush () {
		// TODO: could not find a better way of doing this yet. simply restart the threads for now

		int thread_count = (int)threads.size();

		shutdown();

		start_threads(thread_count, prio, std::move(thread_base_name));
	}

	~Threadpool () {
		shutdown();
	}

	int thread_count () {
		return (int)threads.size();
	}
};
