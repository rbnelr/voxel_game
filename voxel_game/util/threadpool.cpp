#include "threadpool.hpp"

#if defined(_WIN32)
	#include "windows.h"
	#include "assert.h"

	std::basic_string<wchar_t> utf8_to_wchar (std::string const& utf8) {

		// overallocate, this might be more performant than having to process the utf8 twice
		std::basic_string<wchar_t> wstr (utf8.size() +1, '\0'); // wchar string can never be longer than number of utf8 bytes, right?

		auto res = (size_t)MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, &wstr[0], (int)wstr.size());
		assert(res > 0 && res <= wstr.size());

		wstr.resize(res -1);

		return wstr;
	}

	void set_high_thread_priority () {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}

	void set_thread_description (std::string const& description) {
		SetThreadDescription(GetCurrentThread(), utf8_to_wchar(description).c_str());
	}
#endif
