#pragma once
#include "string.hpp"
#include "macros.hpp"

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

	enum file_change_e {
		FILECHANGE_NONE			= 0,
		FILE_ADDED				= 1,
		FILE_MODIFIED			= 2,
		FILE_RENAMED_NEW_NAME	= 4,
		FILE_REMOVED			= 8,
	};

	struct File_Change {
		std::string		filename; // relative to directory_path
		file_change_e	changes;
	};
	struct ChangedFiles {
		std::vector<File_Change> files;

		// was file changed with any of [filter] operations? returns the change (or FILECHANGE_NONE(0) if not in changed files)
		inline file_change_e contains (std::string const& filename, file_change_e filter=FILECHANGE_NONE) {
			for (auto& file : files) {
				if (file.filename == filename && (filter == FILECHANGE_NONE || (file.changes & filter) != 0)) {
					return file.changes;
				}
			}
			return FILECHANGE_NONE;
		}
		inline file_change_e contains_any (std::vector<std::string> const& filenames, file_change_e filter=FILECHANGE_NONE) {
			for (auto& file : files) {
				for (auto& caller_file : filenames) {
					if (file.filename == caller_file && (filter == FILECHANGE_NONE || (file.changes & filter) != 0)) {
						return file.changes;
					}
				}
			}
			return FILECHANGE_NONE;
		}

		// shortcut to know if any changes occurred, if you are feeling lazy
		bool any () {
			return !files.empty();
		}
	};

	class DirectoyChangeNotifier {
		std::string	directory_path;
		bool watch_subdirs;

		void* os_data = nullptr;

	public:

		DirectoyChangeNotifier (std::string_view directory_path, bool watch_subdirs=true);
		~DirectoyChangeNotifier ();

		// 
		ChangedFiles poll_changes ();
	};
}

ENUM_BITFLAG_OPERATORS(kiss::file_change_e)
