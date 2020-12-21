#pragma once
#include "kisslib/threadpool.hpp"
#include "kisslib/kissmath.hpp"

inline const int logical_cores = std::thread::hardware_concurrency();

// win32:
// NORMAL_PRIORITY_CLASS:
//  THREAD_PRIORITY_IDLE 			1
//  THREAD_PRIORITY_LOWEST 			6
//  THREAD_PRIORITY_BELOW_NORMAL 	7
//  THREAD_PRIORITY_NORMAL 			8
//  THREAD_PRIORITY_ABOVE_NORMAL 	9
//  THREAD_PRIORITY_HIGHEST 		10
//  THREAD_PRIORITY_TIME_CRITICAL 	15

// setting my threads to some value about NORMAL seems to be required to even get my main thread & parallelism_threads to run at all
// when the scheduler is loaded with other apps and my background threads
// -> THREAD_PRIORITY_ABOVE_NORMAL is needed to get stable framerates during load
const int main_thread_prio = THREAD_PRIORITY_ABOVE_NORMAL;
const int background_threads_prio = THREAD_PRIORITY_LOWEST;
const int parallelism_threads_prio = THREAD_PRIORITY_ABOVE_NORMAL;

// NOPE: a̶s̶ ̶m̶a̶n̶y̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶a̶s̶ ̶t̶h̶e̶r̶e̶ ̶a̶r̶e̶ ̶l̶o̶g̶i̶c̶a̶l̶ ̶c̶o̶r̶e̶s̶ ̶t̶o̶ ̶a̶l̶l̶o̶w̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶t̶o̶ ̶u̶s̶e̶ ̶e̶v̶e̶n̶ ̶t̶h̶e̶ ̶m̶a̶i̶n̶ ̶t̶h̶r̶e̶a̶d̶'̶s̶ ̶t̶i̶m̶e̶ ̶w̶h̶e̶n̶ ̶w̶e̶ ̶a̶r̶e̶ ̶g̶p̶u̶ ̶b̶o̶t̶t̶l̶e̶n̶e̶c̶k̶e̶d̶ ̶o̶r̶ ̶a̶t̶ ̶a̶n̶ ̶f̶p̶s̶ ̶c̶a̶p̶
// A threadpool for async background work tasks
// keep a reasonable amount of cores free from background work because lower thread priority is not enough to ensure that these threads get preempted when high prio threads need to run
// this is because of limited frequency of the scheduling interrupt 'timer resolution' on windows at least
// the main thread should be able to run after waiting and there need to be enough additional cores free for the os tasks, else mainthread often gets preemted for ver long (1ms - 10+ ms) causing serious lag
// cores:  1 -> threads: 1
// cores:  2 -> threads: 1
// cores:  4 -> threads: 2
// cores:  6 -> threads: 4
// cores:  8 -> threads: 5
// cores: 12 -> threads: 8
// cores: 16 -> threads: 12
// cores: 24 -> threads: 18
// cores: 32 -> threads: 24
inline const int background_threads  = clamp(roundi((float)logical_cores * 0.79f) - 1, 1, logical_cores);

// cores:  1 -> threads: 1
// cores:  2 -> threads: 1
// cores:  4 -> threads: 3
// cores:  6 -> threads: 4
// cores:  8 -> threads: 6
// cores: 12 -> threads: 10
// cores: 16 -> threads: 13
// cores: 24 -> threads: 20
// cores: 32 -> threads: 28
#if defined(NDEBUG) || 1
inline const int parallelism_threads = clamp(roundi((float)logical_cores * 0.894f) - 1, 1, logical_cores);
#else
inline const int parallelism_threads = 1;
#endif
