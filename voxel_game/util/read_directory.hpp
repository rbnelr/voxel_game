#pragma once
#include "string.hpp"

#include <vector>

namespace kiss {
	// A read directory
	//  dirnames is a list of subdirectories in this dir
	//  filenames is a list of filenames in this dir
	// strings are utf8
	struct Directory {
		std::vector<std::string>		dirnames;
		std::vector<std::string>		filenames;
	};

	// A read directory tree
	//  dirs is a list of subdirectories in this dir
	//  filenames is a list of filenames in this dir
	// strings are utf8
	struct Directory_Tree {
		std::string						name;
		// contents
		std::vector<Directory_Tree>		subdirs;
		std::vector<std::string>		filenames;

		bool							valid; // could be file/dir not found, but also stuff like access denied
	};

	// Read a directory (ie. find out what files and subdirectories it contains)
	// strings are utf8
	// path can be empty -> search current dir
	// or a path ending in '/'
	bool read_directory (std::string_view path, Directory* result, std::string_view file_filter="*");

	// Read a directory (ie. find out what files and subdirectories it contains)
	// recursive, ie. subdirs are also read
	// strings are utf8
	// path can be empty -> search current dir
	// or a path ending in '/'
	bool read_directory_recursive (std::string_view path, Directory_Tree* result, std::string_view file_filter="*");
}
