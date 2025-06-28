#include "read_directory.hpp"

#include "assert.h"

#include "clean_windows_h.hpp"

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

//// DirectoyChangeNotifier

	// WARNING: This memory has to stay at the same address (no copy or move), since a pointer to this passed into overlapped ReadDirectoryChangesW
	struct DirectoyChangeNotifier_OSData {
		HANDLE		dir = INVALID_HANDLE_VALUE;
		OVERLAPPED	ovl = {};

		char buf[1024];

		bool directory_valid () {
			return dir != INVALID_HANDLE_VALUE && ovl.hEvent != NULL;
		}

		void init (std::string const& directory_path, bool watch_subdirs) {
			dir = CreateFileW(
				utf8_to_wchar(directory_path).c_str(),
				FILE_LIST_DIRECTORY,
				FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,
				NULL);
			auto dir_err = GetLastError();

			ovl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			if (dir == INVALID_HANDLE_VALUE || ovl.hEvent == NULL) {
				fprintf(stderr, "Directory_Watcher init failed with directory_path=\"%s\" (%s), won't monitor file changes!\n", directory_path.c_str(), dir_err == ERROR_FILE_NOT_FOUND ? "ERROR_FILE_NOT_FOUND" : "unknown error");
			}

			do_ReadDirectoryChanges(watch_subdirs);
		}
		void close () {
			if (dir != INVALID_HANDLE_VALUE)
				CloseHandle(dir);
			if (ovl.hEvent != NULL)
				CloseHandle(ovl.hEvent);
		}

		bool do_ReadDirectoryChanges (bool watch_subdirs) {
			memset(buf, 0, sizeof(buf)); // ReadDirectoryChangesW does not null terminate the filenames?? This seems to fix the problem for now (filenames were overwriting each other aaa.txt overwrote the previous filename bbbbbbbbbbb.txt, which then read aaa.txtbbbb.txt)

			auto res = ReadDirectoryChangesW(dir, buf,sizeof(buf), watch_subdirs,
				FILE_NOTIFY_CHANGE_FILE_NAME|
				FILE_NOTIFY_CHANGE_DIR_NAME|
				FILE_NOTIFY_CHANGE_SIZE|
				FILE_NOTIFY_CHANGE_LAST_WRITE|
				FILE_NOTIFY_CHANGE_CREATION,
				NULL, &ovl, NULL);
			if (!res) {
				auto err = GetLastError();
				if (err != ERROR_IO_PENDING)
					return false; // fail
			}
			return true;
		}

		ChangedFiles poll_changes (bool watch_subdirs) {
			ChangedFiles changed_files;

			if (!directory_valid())
				return {};

			DWORD bytes_returned;
			auto res = GetOverlappedResult(dir, &ovl, &bytes_returned, FALSE);
			if (!res) {
				auto err = GetLastError();
				if (err != ERROR_IO_INCOMPLETE) {
					// Error ?
				}
				return {};
			} else {
				char const* cur = buf;

				for (;;) {
					auto remaining_bytes = (uintptr_t)bytes_returned - (uintptr_t)(cur - buf);
					if (remaining_bytes == 0)
						break; // all changes processed

					assert(remaining_bytes >= sizeof(FILE_NOTIFY_INFORMATION));
					FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)cur;

					assert(remaining_bytes >= offsetof(FILE_NOTIFY_INFORMATION, FileName) +info->FileNameLength);

					std::string filepath = wchar_to_utf8(std::wstring_view(info->FileName, info->FileNameLength/2));

				#if 0
					const char* action_str = nullptr;
					switch (info->Action) {
						case FILE_ACTION_ADDED:				action_str = "FILE_ACTION_ADDED             ";	break;
						case FILE_ACTION_MODIFIED:			action_str = "FILE_ACTION_MODIFIED          ";	break;
						case FILE_ACTION_RENAMED_NEW_NAME:	action_str = "FILE_ACTION_RENAMED_NEW_NAME  ";	break;
						case FILE_ACTION_REMOVED:			action_str = "FILE_ACTION_REMOVED           ";	break;
						case FILE_ACTION_RENAMED_OLD_NAME:	action_str = "FILE_ACTION_RENAMED_OLD_NAME  ";	break;

						default:							action_str = "unknown"; break;
					}

					printf("%s to \"%s\" detected.\n", action_str, filepath.c_str());
				#endif

					file_change_e type;

					switch (info->Action) {
						case FILE_ACTION_ADDED:				type = FILE_ADDED;				break;
						case FILE_ACTION_MODIFIED:			type = FILE_MODIFIED;			break;
						case FILE_ACTION_RENAMED_NEW_NAME:	type = FILE_RENAMED_NEW_NAME;	break;
						case FILE_ACTION_REMOVED:			type = FILE_REMOVED;			break;
					}

					switch (info->Action) {
						case FILE_ACTION_ADDED:				// file was added, report it
						case FILE_ACTION_MODIFIED:			// file was modified, report it
						case FILE_ACTION_RENAMED_NEW_NAME:	// file was renamed and this is the new name (its like a file with the new name was added), report it
						case FILE_ACTION_REMOVED:			// file was deleted, report it
						{
							// all assets with dependencies on this file should be reloaded

							if (filepath.find_first_of('~') != std::string::npos) { // string contains a tilde '~' character
								// tilde characters are often used for temporary files, for ex. MSVC writes source code changes by creating a temp file with a tilde in it's name and then swaps the old and new file by renaming them
								//  so filter those files here since the user of Directory_Watcher _probably_ does not want those files
							} else {

								// replace '\' with '/' to enable simple string comparisons on the results (I try to use '/' everywhere)
								for (size_t i=0; i<filepath.size(); ++i)
									if (filepath[i] == '\\') filepath[i] = '/';

								changed_files.files.push_back({ std::move(filepath), type });
							}
						} break;

						case FILE_ACTION_RENAMED_OLD_NAME:	// file was renamed and this is the old name (its like the file with the old name was deleted), don't report it
						default:
							// do not try to reload assets with dependencies on files that are deleted or renamed (old name no longer exists), instead just keep the asset loaded
							break;
					}

					if (info->NextEntryOffset == 0)
						break; // all changes processed

					cur += info->NextEntryOffset;
				}

				ResetEvent(ovl.hEvent);

				do_ReadDirectoryChanges(watch_subdirs);

				return changed_files;
			}
		}
	};

	DirectoyChangeNotifier::DirectoyChangeNotifier (std::string_view directory_path, bool watch_subdirs):
			directory_path{directory_path}, watch_subdirs{watch_subdirs} {

		os_data = malloc(sizeof(DirectoyChangeNotifier_OSData));

		((DirectoyChangeNotifier_OSData*)os_data)->init(this->directory_path, watch_subdirs);
	}
	DirectoyChangeNotifier::~DirectoyChangeNotifier () {

		((DirectoyChangeNotifier_OSData*)os_data)->close();

		free(os_data);
	}

	ChangedFiles DirectoyChangeNotifier::poll_changes () {

		return ((DirectoyChangeNotifier_OSData*)os_data)->poll_changes(watch_subdirs);
	}
}
