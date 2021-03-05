#pragma once
#include "common.hpp"
#include "kisslib/parse.hpp"
#include "opengl_helper.hpp"

inline std::string operator+ (std::string_view const& l, std::string_view const& r) {
	std::string s;
	s.reserve(l.size() + r.size()); // null terminator is implicit see https://stackoverflow.com/questions/30111288/stdstringreserve-and-end-of-string-0
	s.insert(0, l);
	s.insert(l.size(), r);
	return s;
}

inline bool preprocess_include_file (char const* filename, std::string* result, std::vector<std::string>* src_files) {
	using namespace parse;
	const auto inc_len = strlen("include");

	if (!contains(*src_files, std::string_view(filename)))
		src_files->push_back(filename);

	std::string source;
	if (!kiss::load_text_file(filename, &source)) {
		clog(ERROR, "[Shaders] \"%s\": could not find file \"%s\"!", filename);
		return false;
	}

	bool success = true;

	auto path = kiss::get_path(filename);
	
	int line_no = 1;

	char const* c = source.c_str();
	while (*c != '\0') { // for all lines
		char const* line = c;

		whitespace(c);

		char const* inc_begin = c;

		if (*c == '#') {
			c++; // skip '#'
			whitespace(c);

			if (strncmp(c, "include", inc_len) == 0) {
				c += inc_len;

				whitespace(c);

				std::string_view inc_filename;
				if (!quoted_string(c, &inc_filename)) {
					clog(ERROR, "[Shaders] \"%s\": could not find file \"%s\"!", filename);
					success = false;
				} else {

					std::string inc_filepath = path + inc_filename;

					prints(result, "#line 1 \"%s\"\n", inc_filepath.c_str());

					if (!preprocess_include_file(inc_filepath.c_str(), result, src_files)) // include file text instead of ' #include "filename" '
						success = false;

					prints(result, "#line %d \"%s\"\n", line_no, filename);

					line = c; // set line to after ' #include "filename" '
				}
			}
		}

		// skip to after end of line line
		while (!newline(c) && *c != '\0')
			c++;
		
		line_no++;

		result->append(std::string_view(line, c - line));
	}

	return success;
}

// Need ability to define per-shader macros, but simply prepending the macros to the source code does not work
// because first line in glsl shader needs to be "#versiom ...."
// So workaround this fact by scanning for the right place to insert
inline std::string preprocessor_insert_macro_defs (std::string const& source, char const* filename, std::string const& macros) {
	// assume first line is always #version, so simply scan for first newline
	char const* c = source.c_str();
	while (!parse::newline(c) && *c != '\0')
		c++;

	std::string result;
	result.reserve(source.capacity());

	result += std::string_view(source.c_str(), c - source.c_str()); // #version line
	result += macros;
	prints(&result, "#line 1 \"%s\"\n", filename); // reset source line number
	result += std::string_view(c, source.size() - (c - source.c_str()));
	return result;
}
