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