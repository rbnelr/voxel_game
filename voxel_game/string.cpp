#include "string.hpp"

namespace kiss {
	// Printf that appends to a std::string
	void vprints (std::string* s, cstr format, va_list vl) { // print 
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
	void prints (std::string* s, cstr format, ...) {
		va_list vl;
		va_start(vl, format);

		vprints(s, format, vl);

		va_end(vl);
	}

	// Printf that outputs to a std::string
	std::string prints (cstr format, ...) {
		va_list vl;
		va_start(vl, format);

		std::string ret;
		vprints(&ret, format, vl);

		va_end(vl);

		return std::move(ret);
	}
}
