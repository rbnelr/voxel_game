#pragma once
#include <string_view>

namespace parse {

	//// Check char class

	constexpr inline bool is_decimal_c (char c) {
		return c >= '0' && c <= '9';
	}
	constexpr inline bool is_hex_c (char c) {
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
	}
	constexpr inline bool is_alpha_c (char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}
	constexpr inline bool is_ident_start_c (char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	constexpr inline bool is_ident_c (char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9');
	}
	constexpr inline bool is_whitespace_c (char c) {
		return c == ' ' || c == '\t';
	}

	//// returns true if <c> points to start of (whitespace, newline sequence, integer, etc.)
	//// <c> will be one after the string in question, else returns false and leaves <c> unchanged
	/* Allows code like:
		whitespace(c); // optional whitespace

		if (integer(c)) {
			
		} else if (float(c)) {
			
		}
		if (!newline(c))
			break;
	*/

	// skips "  \t  \t\t  "
	inline bool whitespace (char const*& c) {
		if (!is_whitespace_c(*c))
			return false;

		while (is_whitespace_c(*c))
			c++;
		return true;
	}

	// skips "\n" or "\r" or "\r\n" or "\n\r"
	inline bool newline (char const*& c) {
		if (*c != '\r' && *c != '\n')
			return false;

		char ch = *c;
		c++;

		// "\n" "\r" "\n\r" "\r\n" each count as one newline whilte "\n\n" "\r\r" count as two
		// ie. this code should even handle files with inconsistent newlines somewhat reasonably
		if ((*c == '\r' || *c == '\n') && ch != *c)
			c++;
		return true;
	}

	// skips "// this is a line comment" (until newline)
	inline bool line_comment (char const*& c) {
		if (c[0] != '/' || c[1] != '/')
			return false;

		c += 2;
		while (*c != '\n' && *c != '\r' && *c != '\0')
			c++;
		return true;
	}

	// skips "I_am_a_identifier_2" (2_blah does not count)
	// returns string (non-null-terminated) in <out_ident>
	inline bool identifier (char const*& c, std::string_view* out_ident) {
		if (!is_ident_start_c(*c))
			return false;

		char const* begin = c;

		while (is_ident_c(*c))
			c++; // find end of identifier

		*out_ident = std::string_view(begin, c - begin);
		return true;
	}

	// skips "-012345"
	// returns int in <out_int>
	inline bool integer (const char*& c, int* out_int) {
		const char* cur = c;

		bool neg = false;
		if (*cur == '-') {
			neg = true;
			cur++;
		} else if (*cur == '+') {
			cur++;
		}

		if (*cur < '0' || *cur > '9')
			return false;

		unsigned int out = 0;
		while (*cur >= '0' && *cur <= '9') {
			out *= 10;
			out += *cur++ - '0';
		}

		*out_int = neg ? -(int)out : (int)out;
		c = cur;
		return true;
	}

	// skips "hello World @"
	// no escaping is handled
	// returns string in <out_str>
	inline bool quoted_string (char const*& c, std::string_view* str) {
		if (*c != '"')
			return false;

		c++; // skip '"'
		char const* begin = c;

		while (*c != '"')
			c++;

		*str = std::string_view(begin, c - begin);

		c++; // skip '"'
		return true;
	}

}
