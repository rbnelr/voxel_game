#pragma once
#include "stdint.h"
#include <string>
#include <string_view>
#include <memory>

namespace kiss {
	uint64_t get_file_size (FILE* f);

	// reads text file into a std::string (overwriting it's previous contents)
	// returns false on fail (file not found etc.)
	bool load_text_file (const char* filename, std::string* out);

	// reads text file into a std::string
	// returns "" on fail (file not found etc.)
	std::string load_text_file (const char* filename);

	typedef unsigned char byte;
	typedef std::unique_ptr<byte[]> raw_data;

	raw_data load_binary_file (const char* filename, uint64_t* size);

	void save_binary_file (const char* filename, void* data, uint64_t size);

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
