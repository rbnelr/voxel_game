#include "threadpool.hpp"
#include "assert.h"

#undef WIN32_LEAN_AND_MEAN

#if defined(_WIN32)
	#include "windows.h"
	#include "avrt.h"
	
	void set_process_priority () {
	//#ifdef NDEBUG
	//	auto ret = SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	//	assert(ret != 0);
	//#endif
	}

	// https://docs.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service
	// This actually works, with this enabled the os and other app threads only interrupt us during a 2ms window every 10ms
	// Not sure how much this really helps with, but it seems like it can't hurt and lets the os know we are a game
	// TODO: There is also a "Real-Time Work Queue API" which seems like it is a threadpool which might also have benefits, have not tested yet
	void set_mcss_thread (ThreadPrio prio) {
		char const* TaskName;
		AVRT_PRIORITY avrt_prio;
		switch (prio) {
			case TPRIO_MAIN:		TaskName = "Games"; avrt_prio = AVRT_PRIORITY_HIGH;		break;
			case TPRIO_PARALLELISM:	TaskName = "Games"; avrt_prio = AVRT_PRIORITY_NORMAL;	break;
			case TPRIO_BACKGROUND:	TaskName = "Games"; avrt_prio = AVRT_PRIORITY_LOW;		break;
			default: return;
		}

		DWORD TaskIndex = 0;
		HANDLE TaskHandle = AvSetMmThreadCharacteristicsA(TaskName, &TaskIndex);
		if (TaskHandle == NULL) {
			auto err = GetLastError();
			return;
		}

		auto res = AvSetMmThreadPriority(TaskHandle, avrt_prio);
	}

	// https://docs.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
	//  ABOVE_NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST = 12  which is above NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST
	//  ABOVE_NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_LOWEST  = 8   which is equal to NORMAL_PRIORITY_CLASS + THREAD_PRIORITY_NORMAL so equal to default threads
	// 
	//  HIGH_PRIORITY_CLASS + THREAD_PRIORITY_HIGHEST caused input processing to lag (mouse lag) when 100% cpu
	void set_thread_priority (ThreadPrio prio) {
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
		// -> THREAD_PRIORITY_HIGHEST + THREAD_PRIORITY_ABOVE_NORMAL is needed to get stable framerates during load
		int winprio = THREAD_PRIORITY_NORMAL;
		switch (prio) {
			case TPRIO_MAIN:		winprio = THREAD_PRIORITY_HIGHEST; break;
			case TPRIO_PARALLELISM:	winprio = THREAD_PRIORITY_ABOVE_NORMAL; break;
			case TPRIO_BACKGROUND:	winprio = THREAD_PRIORITY_LOWEST; break;
		}

		auto ret = SetThreadPriority(GetCurrentThread(), winprio);
		assert(ret != 0);

		set_mcss_thread(prio);
	}

	void set_thread_preferred_core (int preferred_core) {
		//auto ret = SetThreadIdealProcessor(GetCurrentThread(), preferred_core);
		//assert(ret >= 0);
	}

	void set_thread_description (std::string_view description) {
		SetThreadDescription(GetCurrentThread(), kiss::utf8_to_wchar(description).c_str());
	}

	// -M̶o̶s̶t̶l̶y̶ ̶n̶o̶t̶ ̶r̶e̶q̶u̶i̶e̶d̶ ̶i̶f̶ ̶I̶ ̶u̶s̶e̶ ̶h̶i̶g̶h̶e̶r̶ ̶t̶h̶a̶n̶ ̶n̶o̶r̶m̶a̶l̶ ̶p̶r̶i̶o̶r̶i̶t̶i̶e̶s̶ ̶f̶o̶r̶ ̶m̶y̶ ̶t̶h̶r̶e̶a̶d̶
	// Nope: In some cases when our threads get scheduled out, they won't run for something like 36 ms
	// Multimedia Class Scheduler Service timings do not seem to be modified by this
#if 0
	// Set windows scheduling frequency 'timer resolution' to the commonly used 1ms to make sure that our use of cpu cores never prevents a high priority thread from running for too long
	// This should be fine for a game
	// TODO: Look at minimization or game pause etc. at some point, in those states we should timeEndPeriod to be little more nice with the os (other processes most likely override this to 1ms anyway)
	struct _SetWindowsSchedFreq {
	
		static constexpr UINT PERIOD_MS = 4;
	
		_SetWindowsSchedFreq () {
			timeBeginPeriod(PERIOD_MS);
		}
	
		~_SetWindowsSchedFreq () {
			timeEndPeriod(PERIOD_MS);
		}
	
	} _setWindowsSchedFreq; // set timeBeginPeriod at startup
#endif

#endif
