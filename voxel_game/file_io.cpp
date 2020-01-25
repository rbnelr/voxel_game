#pragma once
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#include "file_io.hpp"
#include "stdio.h"
#include "assert.h"

namespace kiss {

	uint64_t get_file_size (FILE* f) {
		fseek(f, 0, SEEK_END);
		uint64_t file_size = ftell(f); // only 32 support for now
		rewind(f);
		return file_size;
	}

	// reads text file into a std::string (overwriting it's previous contents)
	// returns false on fail (file not found etc.)
	bool read_text_file (const char* filename, std::string* out) {
		FILE* f = fopen(filename, "rb"); // read binary because i don't want to convert "\r\n" to "\n"
		if (!f) return false; // fail

		uint64_t file_size = get_file_size(f);

		out->resize(file_size);

		auto ret = fread(&(*out)[0], 1,file_size, f);
		assert(ret == file_size);

		fclose(f);
		return true;
	}

	// reads text file into a std::string
	// returns "" on fail (file not found etc.)
	std::string read_text_file (const char* filename) {
		std::string s;
		read_text_file(filename, &s);
		return s;
	}
}
