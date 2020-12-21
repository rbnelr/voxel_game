#pragma once
#include "kisslib/threadpool.hpp"
#include "kisslib/kissmath.hpp"

inline const int logical_cores = std::thread::hardware_concurrency();

// NOPE: a̶s̶ ̶m̶a̶n̶y̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶a̶s̶ ̶t̶h̶e̶r̶e̶ ̶a̶r̶e̶ ̶l̶o̶g̶i̶c̶a̶l̶ ̶c̶o̶r̶e̶s̶ ̶t̶o̶ ̶a̶l̶l̶o̶w̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶t̶o̶ ̶u̶s̶e̶ ̶e̶v̶e̶n̶ ̶t̶h̶e̶ ̶m̶a̶i̶n̶ ̶t̶h̶r̶e̶a̶d̶'̶s̶ ̶t̶i̶m̶e̶ ̶w̶h̶e̶n̶ ̶w̶e̶ ̶a̶r̶e̶ ̶g̶p̶u̶ ̶b̶o̶t̶t̶l̶e̶n̶e̶c̶k̶e̶d̶ ̶o̶r̶ ̶a̶t̶ ̶a̶n̶ ̶f̶p̶s̶ ̶c̶a̶p̶
// A threadpool for async background work tasks
// keep a reasonable amount of cores free from background work because lower thread priority is not enough to ensure that these threads get preempted when high prio threads need to run
// this is because of limited frequency of the scheduling interrupt 'timer resolution' on windows at least
// the main thread should be able to run after waiting and there need to be enough additional cores free for the os tasks, else mainthread often gets preemted for ver long (1ms - 10+ ms) causing serious lag

inline const int background_threads  = clamp(roundi((float)logical_cores * 0.6f) - 1, 1, logical_cores);

#if defined(NDEBUG) || 1
inline const int parallelism_threads = clamp(roundi((float)logical_cores * 0.84f) - 1, 1, logical_cores);
#else
inline const int parallelism_threads = 1;
#endif
