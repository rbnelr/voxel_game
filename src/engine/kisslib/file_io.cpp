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
	
	bool load_binary_file (const char* filename, void* data, uint64_t size) {
		auto f = fopen(filename, "rb");
		if (!f)
			return false;

		uint64_t sz = get_file_size(f);
		if (sz != size)
			return false;

		auto ret = fread(data, 1,sz, f);

		fclose(f);
		return true;
	}

	raw_data load_binary_file (const char* filename, uint64_t* size) {
		auto f = fopen(filename, "rb");
		if (!f)
			return nullptr;

		uint64_t sz = get_file_size(f);
		auto data = std::make_unique<byte[]>(sz);

		auto ret = fread(data.get(), 1,sz, f);

		fclose(f);

		*size = sz;
		return data;
	}

	bool save_binary_file (const char* filename, void const* data, uint64_t size) {
		auto f = fopen(filename, "wb");
		if (!f)
			return false;

		auto ret = fwrite(data, 1,size, f);

		fclose(f);
		return true;
	}

	// reads text file into a std::string (overwriting it's previous contents)
	// returns false on fail (file not found etc.)
	bool load_text_file (const char* filename, std::string* out) {
		FILE* f = fopen(filename, "rb"); // read binary because i don't want to convert "\r\n" to "\n"
		if (!f)
			return false; // fail

		uint64_t file_size = get_file_size(f);

		out->resize(file_size);

		auto ret = fread(&(*out)[0], 1,file_size, f);
		assert(ret == file_size);

		fclose(f);
		return true;
	}

	// reads text file into a std::string
	// returns "" on fail (file not found etc.)
	std::string load_text_file (const char* filename) {
		std::string s;
		load_text_file(filename, &s);
		return s;
	}

	// saves a text file
	bool save_text_file (const char* filename, std::string_view str) {
		return save_binary_file(filename, str.data(), str.size());
	}

	// there is no memrchr in standard c++
	//  (strrchr does a redundant strlen or is less efficient)
	//  (memchr returns the first not last occurrance)
	inline char const* my_memrchr (char const* s, char c, size_t size) {
		char const* cur = s + size;
		while (cur != s) {
			cur--;
			if (*cur == c)
				return cur;
		}
		return nullptr;
	}

	std::string_view get_path (std::string_view filepath, std::string_view* out_filename, char slash) {
		char const* end = filepath.data() + filepath.size();

		char const* p = my_memrchr(filepath.data(), slash, filepath.size());
		if (!p) {
			// no slash found => path is empty, filename is whole input
			if (out_filename) *out_filename = filepath;
			return std::string_view(filepath.data(), 0);
		}

		if (out_filename) *out_filename = std::string_view(p+1, end - (p+1)); // exclude slash from filename
		return std::string_view(filepath.data(), p+1 - filepath.data()); // include slash in path
	}

	std::string_view get_ext (std::string_view filepath, std::string_view* out_filename, bool include_dot_in_ext) {
		char const* end = filepath.data() + filepath.size();

		char const* p = my_memrchr(filepath.data(), '.', filepath.size());
		if (!p)
			p = filepath.data(); // no splitchar found => left substr is empty

		if (out_filename)
			*out_filename = std::string_view(filepath.data(), p - filepath.data()); // exclude '.' from filename
		
		if (!include_dot_in_ext)
			p++; // exclude '.' from extension
		return std::string_view(p, end - p);
	}
}

#if defined(_WIN32)
	#include "windows.h"
	#include "shlwapi.h"

	namespace kiss {
		std::string try_make_path_relative (const char* relative_to, const char* path) {
			char relative_path[MAX_PATH];
			if (PathRelativePathToA(relative_path, relative_to, FILE_ATTRIBUTE_DIRECTORY, path, FILE_ATTRIBUTE_NORMAL)) {
				std::string_view str(relative_path);

				// remove any leading ".\"
				if (str.rfind(".\\", 0) == 0) {
					str = str.substr(2);
				}

				// Do not allow backwards pathds
				if (str.find("..") == std::string::npos) {
					return std::string(str);
				}
			}
			// otherwise return old absolute path
			return path;
		}

		// This barely even works
		// Doesn't open on the application dir, desktop or anywhere useful
		// Can't seem to manage to select the start dir either
		// returns absolute paths when I need them to be relative if are relative to application dir
		bool file_open_dialog (std::string* out_filename, const char* initial_dir) {
			char filepath[MAX_PATH] = { 0 };
			char current_dir[MAX_PATH] = { 0 };
		
			if (!GetCurrentDirectoryA(MAX_PATH, current_dir))
				current_dir[0] = '\0'; // just to make sure

			if (initial_dir[0] == '\0')
				initial_dir = current_dir;

			OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
			ofn.hwndOwner = GetActiveWindow();
			ofn.lpstrFile = filepath;
			ofn.nMaxFile = sizeof(filepath);
			ofn.lpstrInitialDir = initial_dir;
		
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
			auto res = GetOpenFileNameA(&ofn);
			if (!res) {
				return false;
			}

			*out_filename = try_make_path_relative(current_dir, filepath);
			return true;
		}

		bool create_directory (const char* dirname) {
			auto res = CreateDirectoryA(dirname, NULL); // C has no way of creating directories, are you kidding me?
			return res != 0;
		}
	}
#endif
