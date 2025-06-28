#pragma once
#include "stdarg.h"
#include <string>
#include <string_view>
#include <regex>

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

	static constexpr inline bool is_whitespace_c (char c) {
		return c == ' ' || c == '\t';
	}

	// remove whitespace at front and back
	std::string_view trim (std::string_view sv);

	template <typename FUNC>
	std::string regex_replace (std::string const& str, std::regex const& re, FUNC for_match) {
		size_t cur = 0;

		std::string out;

		for (auto it = std::sregex_iterator(str.begin(), str.end(), re); it != std::sregex_iterator(); ++it) {
			auto match = *it;

			out += str.substr(cur, match.position());
			cur = match.position() + match.length();

			out += for_match(match);
		}

		out += str.substr(cur, str.size());

		return out;
	}
}
