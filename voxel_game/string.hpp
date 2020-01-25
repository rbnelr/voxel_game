#pragma once
#include "stdarg.h"
#include <string>

namespace kiss {

	template <typename T>
	struct basic_string_view {
		T const*	begin;
		T const*	end;

		/*constexpr*/ T const* c_str () const { return begin; } // constexpr caused a internal compiler error in MSVC17 at some point
		/*constexpr*/ size_t size () const { return end -begin; }

		constexpr basic_string_view (std::basic_string<T> const& s): begin{s.data()}, end{s.data() + s.size()} {}
		constexpr basic_string_view (char const* s): begin{s}, end{s + strlen(s)} {}

		explicit operator std::basic_string<T> () { return std::basic_string<T>(begin, size()); }
	};

	using string_view = basic_string_view<char>;

	typedef char const* cstr;

	// Printf that appends to a std::string
	void vprints (std::string* s, cstr format, va_list vl);

	// Printf that appends to a std::string
	void prints (std::string* s, cstr format, ...);

	// Printf that outputs to a std::string
	std::string prints (cstr format, ...);
}
