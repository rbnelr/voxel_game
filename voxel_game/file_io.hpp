#pragma once
#include "stdint.h"
#include <string>

namespace kiss {
	uint64_t get_file_size (FILE* f);

	// reads text file into a std::string (overwriting it's previous contents)
	// returns false on fail (file not found etc.)
	bool read_text_file (const char* filename, std::string* out);

	// reads text file into a std::string
	// returns "" on fail (file not found etc.)
	std::string read_text_file (const char* filename);
}
