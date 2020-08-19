#include "threadpool.hpp"
#include "assert.h"

#undef WIN32_LEAN_AND_MEAN

#if defined(_WIN32)
	#include "windows.h"
	void set_process_priority () {
	//#ifdef NDEBUG
	//	auto ret = SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	//	assert(ret != 0);
	//#endif
	}

	// https://docs.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
	//  ABOVE_NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST = 12  which is above NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST
	//  ABOVE_NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_LOWEST  = 8   which is equal to NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_NORMAL so equal to default threads
	// 
	//  HIGH_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST caused input processing to lag (mouse lag) when 100% cpu
	void set_thread_priority (ThreadPriority prio) {
		auto ret = SetThreadPriority(GetCurrentThread(), prio == ThreadPriority::HIGH ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_LOWEST);
		assert(ret != 0);
	}

	void set_thread_preferred_core (int preferred_core) {
		auto ret = SetThreadIdealProcessor(GetCurrentThread(), preferred_core);
		assert(ret >= 0);
	}

	void set_thread_description (std::string_view description) {
		SetThreadDescription(GetCurrentThread(), kiss::utf8_to_wchar(description).c_str());
	}

	// Set windows scheduling frequency 'timer resolution' to the commonly used 1ms to make sure that our use of cpu cores never prevents a high priority thread from running for too long
	// This should be fine for a game
	// TODO: Look at minimization or game pause etc. at some point, in those states we should timeEndPeriod to be little more nice with the os (other processes most likely override this to 1ms anyway)
	struct _SetWindowsSchedFreq {

		static constexpr UINT PERIOD_MS = 1000;

		_SetWindowsSchedFreq () {
			timeBeginPeriod(PERIOD_MS);
		}

		~_SetWindowsSchedFreq () {
			timeEndPeriod(PERIOD_MS);
		}

	} _setWindowsSchedFreq; // set timeBeginPeriod at startup
#endif
