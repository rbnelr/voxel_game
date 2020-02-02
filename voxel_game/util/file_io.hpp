#pragma once
#include "stdint.h"
#include <string>
#include <string_view>

namespace kiss {
	uint64_t get_file_size (FILE* f);

	// reads text file into a std::string (overwriting it's previous contents)
	// returns false on fail (file not found etc.)
	bool read_text_file (const char* filename, std::string* out);

	// reads text file into a std::string
	// returns "" on fail (file not found etc.)
	std::string read_text_file (const char* filename);

	// out_filename is optional
	// "hello/world.txt" => path: "hello/" out_filename: "world.txt"
	// "world.txt"       => path:  ""      out_filename: "world.txt"
	std::string_view get_path (std::string_view filepath, std::string_view* out_filename=nullptr);

	// out_filename is optional
	// "hello/world.txt" => ext: "txt"   out_filename: "hello/world"
	// "world.txt"       => ext: "txt"   out_filename: "world"
	// "world"           => ext: ""      out_filename: "world"
	// ".txt"            => ext: "txt"   out_filename: ""
	std::string_view get_ext (std::string_view filepath, std::string_view* out_filename=nullptr);
}
