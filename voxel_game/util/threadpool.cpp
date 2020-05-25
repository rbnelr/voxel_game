#include "threadpool.hpp"
#include "assert.h"

#if defined(_WIN32)
	#include "windows.h"
	void set_process_high_priority () {
		auto ret = SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		assert(ret != 0);
	}

	void set_thread_high_priority () {
		auto ret = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		assert(ret != 0);
	}

	void set_thread_preferred_core (int preferred_core) {
		auto ret = SetThreadIdealProcessor(GetCurrentThread(), preferred_core);
		assert(ret >= 0);
	}

	void set_thread_description (std::string_view description) {
		SetThreadDescription(GetCurrentThread(), kiss::utf8_to_wchar(description).c_str());
	}
#endif
