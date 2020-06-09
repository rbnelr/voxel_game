#pragma once
#include "stdarg.h"
#include <string>
#include <string_view>

namespace kiss {
	// Printf that appends to a std::string
	void vprints (std::string* s, char const* format, va_list vl);

	// Printf that appends to a std::string
	void prints (std::string* s, char const* format, ...);

	// Printf that outputs to a std::string
	std::string prints (char const* format, ...);

	// Convert utf8 'multibyte' strings to windows wchar 'unicode' strings
	// WARNING: utf8 must be null terminated, which string_view does not garantuee
	std::basic_string<wchar_t> utf8_to_wchar (std::string_view utf8);

	// Convert windows wchar 'unicode' to utf8 'multibyte' strings
	// WARNING: wchar must be null terminated, which string_view does not garantuee
	std::string wchar_to_utf8 (std::basic_string_view<wchar_t> wchar);

	std::string format_thousands (int i, char sep=',');
	std::string format_thousands (unsigned i, char sep=',');
	std::string format_thousands (long long i, char sep=',');
	std::string format_thousands (unsigned long long i, char sep=',');

	static constexpr inline bool is_whitespace_c (char c) {
		return c == ' ' || c == '\t';
	}

	// remove whitespace at front and back
	std::string_view trim (std::string_view sv);

	// wrapper class around a std::string to use as a key for unordered_maps that allows find with string_view to avoid heap allocs
	struct map_string {
		std::string str; // store string views for actual keys
		std::string_view sv; // use string view for actual comparison and hashing

		map_string (std::string str) {
			this->str = std::move(str);
			this->sv = this->str;
		}
		map_string (std::string_view sv): sv{sv} {} // string_view gets implicitly converted so that we can do unordered_map.find(string_view)

		bool operator== (map_string const& r) const {
			return sv.compare(r.sv) == 0;
		}
	};
}
namespace std {
	template<> struct hash<kiss::map_string> {
		size_t operator() (kiss::map_string const& s) const {
			return std::hash<std::string_view>()(s.sv);
		}
	};
}
