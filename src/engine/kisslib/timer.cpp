#include "timer.hpp"

#if defined(_WIN32)
	#include "clean_windows_h.hpp"

	namespace kiss {
		uint64_t get_timestamp () {
			LARGE_INTEGER li;
			QueryPerformanceCounter(&li);
			return li.QuadPart;
		}

		uint64_t get_timestamp_freq () {
			LARGE_INTEGER li;
			QueryPerformanceFrequency(&li);
			return li.QuadPart;
		}

		uint64_t timestamp_freq = get_timestamp_freq();

		void sleep_msec (uint32_t msecs) {
			Sleep((DWORD)msecs);
		}
	}
#endif
