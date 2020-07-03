#include "read_directory.hpp"

#include "assert.h"

#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"

namespace kiss {
	
	bool _read_directory (std::string_view path, std::vector<std::string>* dirnames, std::vector<std::string>* filenames, std::string_view file_filter) {
		WIN32_FIND_DATAW data;

		assert(path.size() == 0 || (path.size() > 0 && path.back() == '/'));
		assert(file_filter.find_first_of('*') != file_filter.npos);

		std::string search_str;
		search_str = path;
		search_str += file_filter;

		bool fail = false;

		HANDLE hFindFile = FindFirstFileW(utf8_to_wchar(search_str).c_str(), &data);
		auto err = GetLastError();

		if (hFindFile == INVALID_HANDLE_VALUE) {

			if (err == ERROR_FILE_NOT_FOUND) {
				// no files match the pattern
				// success
			} else if (err == ERROR_PATH_NOT_FOUND) {
				fprintf(stderr, "read_directory(\"%s\"): FindFirstFile failed! [%x] (ERROR_PATH_NOT_FOUND)\n", search_str.c_str(), err);
				fail = true;
			} else {
				fprintf(stderr, "read_directory(\"%s\"): FindFirstFile failed! [%x]\n", search_str.c_str(), err);
				fail = true;
			}

		} else {

			for (;;) { // for all files returned by FindFirstFileW / FindNextFileW

				auto filename = wchar_to_utf8(data.cFileName);

				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					if (	strcmp(filename.c_str(), ".") == 0 ||
							strcmp(filename.c_str(), "..") == 0 ) {
						// found directory represents the current directory or the parent directory, don't include this in the output
					} else {
						dirnames->emplace_back(std::move( filename +'/' ));
					}
				} else {
					filenames->emplace_back(filename);
				}

				auto ret = FindNextFileW(hFindFile, &data);
				auto err = GetLastError();
				if (ret == 0) {
					if (err == ERROR_NO_MORE_FILES) {
						break;
					} else {
						// TODO: in which cases would this happen?
						assert(false);
						fprintf(stderr, "find_files: FindNextFile failed! [%x]", err);

						dirnames->clear();
						filenames->clear();

						fail = true;
						break;
					}
				}
			}

		}

		FindClose(hFindFile);

		return !fail;
	}

	// Read a directory (ie. find out what files and subdirectories it contains)
	// strings are utf8
	// path can be empty -> search current dir
	// or a path ending in '/'
	bool read_directory (std::string_view path, Directory* result, std::string_view file_filter) {
		result->dirnames.clear();
		result->filenames.clear();

		std::string s;
		if (!(path.size() == 0 || path.back() == '/')) {
			s = path;
			s += '/';
			for (size_t i=0; i<s.size(); ++i)
				if (s[i] == '\\') s[i] = '/';
			path = s;
		}

		return _read_directory(path, &result->dirnames, &result->filenames, file_filter);
	}

	bool _read_directory_recursive (std::string_view dir_path, std::string_view dir_name, Directory_Tree* result, std::string_view file_filter) {
		assert(dir_path.size() == 0 || (dir_path.size() > 0 && dir_path.back() == '/'));
		assert(dir_name.size() == 0 || (dir_name.size() > 0 && dir_name.back() == '/'));

		result->name = dir_name;

		std::vector<std::string>	dirnames;

		std::string dir_full;
		dir_full = dir_path;
		dir_full += dir_name;

		result->valid = _read_directory(dir_full, &dirnames, &result->filenames, file_filter);

		for (auto& d : dirnames) {
			Directory_Tree subdir;
			_read_directory_recursive(dir_full, d, &subdir, file_filter);
			result->subdirs.emplace_back( std::move(subdir) );
		}

		return result->valid;
	}

	// Read a directory (ie. find out what files and subdirectories it contains)
	// recursive, ie. subdirs are also read
	// strings are utf8
	// path can be empty -> search current dir
	// or a path ending in '/'
	bool read_directory_recursive (std::string_view path, Directory_Tree* result, std::string_view file_filter) {
		result->subdirs.clear();
		result->filenames.clear();

		std::string s;
		if (!(path.size() == 0 || path.back() == '/')) {
			s = path;
			s += '/';
			for (size_t i=0; i<s.size(); ++i)
				if (s[i] == '\\') s[i] = '/';
			path = s;
		}

		return _read_directory_recursive("", path, result, file_filter);
	}
}
