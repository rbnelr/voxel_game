#include "string.hpp"

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
