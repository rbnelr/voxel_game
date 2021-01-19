#pragma once
#include "common.hpp"
#include "kisslib/parse.hpp"

/*
	void syntax_error (char const* reason, string_view filename, int line_no) {
		clog(ERROR, "ShaderPreprocessor: syntax error around \"%s\":%d!\n>> %s\n", filename.data(), line_no, reason);
	}

	void uniform_found (string_view glsl_type, bool is_ptr, string_view name) {
		gl::type type;
		if (is_ptr) {
			type = POINTER;
		} else {
			if (!get_type(glsl_type, &type))
				return;
		}

		if (shader->uniforms.find(name) == shader->uniforms.end())
			shader->uniforms.emplace(string(name), type);
	}

	void ident_found (string_view ident, const char* c) {
		if (ident.compare("uniform") == 0) {
			string_view type, name;

			whitespace(&c);

			// type name
			if (!identifier(&c, &type))
				return;

			whitespace(&c);

			bool is_ptr = false;
			// NV pointer pointers (*, ** etc.)
			while (*c == '*') {
				c++;
				is_ptr = true;
				whitespace(&c);
			}

			// uniform name
			if (!identifier(&c, &name))
				return;

			uniform_found(type, is_ptr, name);
		}
	}
	*/
	
inline std::string operator+ (std::string_view const& l, std::string_view const& r) {
	std::string s;
	s.reserve(l.size() + r.size()); // null terminator is implicit see https://stackoverflow.com/questions/30111288/stdstringreserve-and-end-of-string-0
	s.insert(0, l);
	s.insert(l.size(), r);
	return s;
}

inline bool preprocess_include_file (char const* filename, std::string* result, std::vector<std::string>* src_files) {
	using namespace parse;

	if (!contains(*src_files, std::string_view(filename)))
		src_files->push_back(filename);

	std::string source;
	if (!kiss::load_text_file(filename, &source)) {
		clog(ERROR, "[Shaders] \"%s\": could not find file \"%s\"!\n", filename);
		return false;
	}

	bool success = true;

	auto path = kiss::get_path(filename);
	
	int line_no = 1;

	const auto inc_len = strlen("include");

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
					clog(ERROR, "[Shaders] \"%s\": could not find file \"%s\"!\n", filename);
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

/*

// map line_no in the preprocessed text to the source file + line_no
//  based on the //$lineno lines the ShaderPreprocessor emits in the preprocessed output
int map_line_no (string_view preprocessed, int pp_line_no, string_view* out_filename) {
	char const* cur = preprocessed.data();

	int pp_line = 1;

	string_view filename;
	int src_line;

	while (*cur) {
		if (cur[0] == '/' && cur[1] == '/' && cur[2] == '$') {
			cur += 3; // skip //$

			string_view cmd;
			whitespace(&cur);
			identifier(&cur, &cmd); // $lineno
			whitespace(&cur);
			integer(&cur, &src_line);
			whitespace(&cur);
			quoted_string(&cur, &filename);

			pp_line++;

		} else if (*cur == '\n') {
			pp_line++;
			src_line++;
		}

		cur++;

		if (pp_line == pp_line_no) {
			break;
		}
	}

	if (pp_line != pp_line_no) {
		return -1; // invalid pp_line_no passed in or bug
	}

	*out_filename = filename;
	return src_line;
}

// TODO: Bug here? shader compile errors are cut off

// map line numbers in shader log to real file line numbers
std::string map_shader_log (std::string const& shader_log, string_view preprocessed) {
	// line number syntax on my NV GTX 1080 seems to be like "0(<line_no>): warning"

	std::regex re("0\\(([0-9]+)\\)\\s:");
	auto begin = std::sregex_iterator(shader_log.begin(), shader_log.end(), re);
	auto end = std::sregex_iterator();

	std::string str;

	size_t latest_match_end = 0;

	for (auto it=begin; it!=end; ++it) {
		auto outter = (*it)[0];
		auto line_no_match = (*it)[1];

		size_t a = outter.first - shader_log.begin();
		size_t b = outter.second - shader_log.begin();

		if (latest_match_end < a) {
			str += shader_log.substr(latest_match_end, a - latest_match_end);
		}

		std::string line_no_pp_str = std::string(line_no_match);

		int line_no_pp;
		char const* cur = line_no_pp_str.c_str();
		if (!integer(&cur, &line_no_pp))
			return "<error>";

		string_view filename;
		int line_no = map_line_no(preprocessed, line_no_pp, &filename);

		str += prints("%s:%d :", std::string(filename).c_str(), line_no);

		latest_match_end = b;
	}

	if (latest_match_end < shader_log.size()) {
		str += shader_log.substr(latest_match_end, shader_log.size() - latest_match_end);
	}

	return str;
}

*/
