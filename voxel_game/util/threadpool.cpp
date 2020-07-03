#include "threadpool.hpp"

#if defined(_WIN32)
	#include "windows.h"

	void set_high_thread_priority () {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}

	void set_thread_description (std::string_view description) {
		SetThreadDescription(GetCurrentThread(), kiss::utf8_to_wchar(description).c_str());
	}
#endif
