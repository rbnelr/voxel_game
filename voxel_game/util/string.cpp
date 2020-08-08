#include "string.hpp"

#include "assert.h"

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"

namespace kiss {
	// Printf that appends to a std::string
	void vprints (std::string* s, char const* format, va_list vl) { // print 
		size_t old_size = s->size();
		for (;;) {
			auto ret = vsnprintf(&(*s)[old_size], s->size() -old_size +1, format, vl); // i think i'm technically not allowed to overwrite the null terminator
			ret = ret >= 0 ? ret : 0;
			bool was_bienough = (size_t)ret < (s->size() -old_size +1);
			s->resize(old_size +ret);
			if (was_bienough) break;
			// buffer was to small, buffer size was increased
			// now snprintf has to succeed, so call it again
		}
	}

	// Printf that appends to a std::string
	void prints (std::string* s, char const* format, ...) {
		va_list vl;
		va_start(vl, format);

		vprints(s, format, vl);

		va_end(vl);
	}

	// Printf that outputs to a std::string
	std::string prints (char const* format, ...) {
		va_list vl;
		va_start(vl, format);

		std::string ret;
		vprints(&ret, format, vl);

		va_end(vl);

		return std::move(ret);
	}

	std::basic_string<wchar_t> utf8_to_wchar (std::string_view utf8) { // utf8 must be null terminated, which string_view does not garantuee

																	   // overallocate, this might be more performant than having to call MultiByteToWideChar twice
																	   // allocate zeroed wchar buffer able to fit as many chars (plus null terminator) as there are utf8 bytes
																	   // this should always be enough chars, right?
		std::basic_string<wchar_t> wstr (utf8.size() +1, '\0');

		auto res = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), -1, &wstr[0], (int)wstr.size());
		assert(res > 0 && res <= wstr.size());

		wstr.resize(res -1);
		//wstr.shrink_to_fit(); don't do shrink to fit because in my use cases wchars are only used temporarily to pass to win32 functions like FindFirstFileW etc.

		return wstr;
	}
	std::string wchar_to_utf8 (std::basic_string_view<wchar_t> wchar) { // wchar must be null terminated, which string_view does not garantuee

																		// overallocate, this might be more performant than having to call MultiByteToWideChar twice
																		// allocate zeroed buffer able to fit 4x the amount of wchars (plus null terminator)
																		// this should always be enough chars, right?
		std::string utf8 (wchar.size() * 4 +1, '\0'); // TODO: why did I use 4 instead of 2 when wchar is 2 bytes? Is it because wchars are also a variable length encoding? shouldnt it still work with 2 though?

													  // WC_NO_BEST_FIT_CHARS sometimes throws erros ?
		auto res = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wchar.data(), -1, &utf8[0], (int)utf8.size(), NULL, NULL);
		auto err = GetLastError();
		assert(res > 0 && res <= utf8.size());

		utf8.resize(res -1);
		utf8.shrink_to_fit();

		return utf8;

		/*
		Old code where i did a size determining pass first
		std::string filepath;

		auto required_size = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, info->FileName, -1, nullptr, 0, NULL, NULL);
		if (required_size == 0)
		break; // fail, do not continue reloading shaders

		assert(required_size >= 1); // required_size includes the null terminator
		filepath.resize(required_size);

		auto actual_size = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, info->FileName, -1, &filepath[0], (int)filepath.size(), NULL, NULL);
		if (actual_size != (int)filepath.size())
		break; // fail, do not continue reloading shaders

		filepath.resize(required_size -1); // remove redundant null terminator

		*/
	}

	std::string _format_thousands (long long i, char sep, const char* printformat) {
		std::string dst;
		dst.reserve(27);

		char src[27];

		int num, commas;

		num = snprintf(src, 27, printformat, i);

		char* cur = src;

		if (*cur == '-' || *cur == '+') {
			dst.push_back(*cur++);
			num--;
		}

		for (commas = 2 - num % 3; *cur; commas = (commas + 1) % 3) {
			dst.push_back(*cur++);
			if (commas == 1 && *cur != '\0') {
				dst.push_back(sep);
			}
		}

		return dst;
	}
	std::string format_thousands (int i, char sep) {
		return _format_thousands(i, sep, "%d");
	}
	std::string format_thousands (unsigned i, char sep) {
		return _format_thousands(i, sep, "%u");
	}
	std::string format_thousands (long long i, char sep) {
		return _format_thousands(i, sep, "%lld");
	}
	std::string format_thousands (unsigned long long i, char sep) {
		return _format_thousands(i, sep, "%llu");
	}

	std::string_view trim (std::string_view sv) {
		size_t start=0, end=sv.size();

		while (start<sv.size() && is_whitespace_c(sv[start]))
			start++;

		while (end>0 && is_whitespace_c(sv[end-1]))
			end--;

		return sv.substr(start, end - start);
	}
}
