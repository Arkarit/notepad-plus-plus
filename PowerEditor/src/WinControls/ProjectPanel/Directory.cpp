// This file is part of Notepad++ project
// Copyright (C)2003 Don HO <don.h@free.fr>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Note that the GPL places important restrictions on "derived works", yet
// it does not provide a detailed definition of that term.  To avoid      
// misunderstandings, we consider an application to constitute a          
// "derivative work" for the purpose of this license if it does any of the
// following:                                                             
// 1. Integrates source code from Notepad++.
// 2. Integrates/includes/aggregates Notepad++ into a proprietary executable
//    installer, such as those produced by InstallShield.
// 3. Links to a library or executes a program that does any of the above.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "Directory.h"
#include <Shlwapi.h>
#include <tchar.h>
#include <assert.h>

Directory::Directory(bool hideEmptyDirs)
	: _exists(false)
	, _hideEmptyDirs(hideEmptyDirs)
	, _wasRead(false)
{
	_lastWriteTime.dwLowDateTime = 0;
	_lastWriteTime.dwHighDateTime = 0;
	enablePrivileges();
	_filters.emplace_back(generic_string(TEXT("*.*")));
}

Directory::Directory(const generic_string& path, const std::vector<generic_string>& filters, bool hideEmptyDirs, bool autoread)
	: _exists(false)
	, _path(path)
	, _filters(filters)
	, _hideEmptyDirs(hideEmptyDirs)
	, _wasRead(false)
{
	_lastWriteTime.dwLowDateTime = 0;
	_lastWriteTime.dwHighDateTime = 0;
	enablePrivileges();
	if (autoread)
		read();
}

bool Directory::read()
{
	_exists = false;
	_lastWriteTime.dwLowDateTime = 0;
	_lastWriteTime.dwHighDateTime = 0;
	_files.clear();
	_dirs.clear();
	_invisibleDirs.clear();

	// first, read directories, never filtered.
	append(TEXT("*.*"), true);

	// then read files, by each filter
	if (_filters.empty())
	{
		append(TEXT("*.*"), false);
	}
	else
	{
		for (size_t i=0; i<_filters.size(); ++i)
			append(_filters[i], false);
	}


	readLastWriteTime(&_lastWriteTime);

	_wasRead = true;

	return _exists;

}

bool Directory::readIfChanged(bool respectEmptyDirs)
{
	if (!_wasRead)
	{
		read();
		return true;
	}

	if (respectEmptyDirs && _hideEmptyDirs)
	{
		if (!writeTimeHasChanged() && !containsDataChanged())
			return false;
	}
	else
	{
		if (!writeTimeHasChanged())
			return false;
	}

	const Directory cmp(_path,_filters);
	if (*this != cmp)
	{
		*this = cmp;
		return true;
	}
	return false;
}

void Directory::append(const generic_string& filter, bool readDirs)
{
	const bool readFiles = !readDirs;

	generic_string searchPath = _path + TEXT("\\") + filter;

	WIN32_FIND_DATA fd;

	HANDLE hFind = FindFirstFileEx(searchPath.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);

	if (hFind == INVALID_HANDLE_VALUE)
		return;

	_exists = true;

	do
	{
		const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		const bool isFile = !isDir;

		if (readDirs && isDir 
			&& _tcscmp(fd.cFileName, TEXT(".")) != 0 && _tcscmp(fd.cFileName, TEXT("..")))
		{
			insertDir(_path, generic_string(fd.cFileName));
		}
		else if (readFiles && isFile)
		{
			_files.insert(generic_string(fd.cFileName));
		}

	} while( FindNextFile(hFind, &fd));

	if (!FindClose(hFind))
	{
		// a FindClose fail should only happen when using a wrong handle
		assert(false);
	}


}

void Directory::insertDir(const generic_string& parentPath, const generic_string& filename)
{
	if (_hideEmptyDirs)
	{
		generic_string childPath = parentPath + TEXT("\\") + filename;
		if (!containsData(childPath))
		{
			_invisibleDirs.emplace_back(filename);
			return;
		}
	}
	_dirs.insert(filename);
}

bool Directory::containsDataChanged() const
{
	for (auto& dir : _dirs)
	{
		generic_string childPath = _path + TEXT("\\") + dir;
		if (!containsData(childPath))
			return true;
	}
	for (auto& dir : _invisibleDirs)
	{
		generic_string childPath = _path + TEXT("\\") + dir;
		if (containsData(childPath))
			return true;
	}
	return false;
}

// containsData() checks, if a directory or its subdirs contain any data which match the filters.
bool Directory::containsData(const generic_string& path) const
{
	if (_filters.empty())
		return containsData(path, TEXT("*.*"));

	const size_t filtersSize = _filters.size();
	for (size_t i=0; i<filtersSize; ++i)
	{
		if (containsData(path, _filters[i]))
			return true;
	}
	return false;
}

// containsData() recursive call
bool Directory::containsData(const generic_string& path, const generic_string& filter) const
{
	generic_string searchPath = path + TEXT("\\") + filter;

	// first step: check only files. If one is found, it contains data.
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFileEx(searchPath.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);

	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		// check only files in this pass, ignore directories.
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			// data was found - return true instantly.
			FindClose(hFind);
			return true;
		}

	} while (FindNextFile(hFind, &fd));

	if (!FindClose(hFind))
	{
		// a FindClose fail should only happen when using a wrong handle
		assert(false);
	}

	// no file was found in this directory. Now check the sub directories.
	hFind = FindFirstFileEx(searchPath.c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, NULL, 0);

	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		const bool isFile = !isDir;

		if (isDir && _tcscmp(fd.cFileName, TEXT(".")) != 0 && _tcscmp(fd.cFileName, TEXT("..")))
		{
			// check if this child directory contains data; if this is the case, return true
			const generic_string childPath( path + TEXT("\\") + fd.cFileName);
			if (containsData(childPath))
			{
				FindClose(hFind);
				return true;
			}
		}
		else if (isFile)
		{
			// data was found - return true. This could happen in case a file was created between 1st and 2nd loop.
			FindClose(hFind);
			return true;
		}

	} while (FindNextFile(hFind, &fd));

	if (!FindClose(hFind))
	{
		// a FindClose fail should only happen when using a wrong handle
		assert(false);
	}

	return false;

}

bool Directory::writeTimeHasChanged() const
{
	FILETIME newFiletime = {0};

	// if the new filetime can not be determined, return true for sure. The exact differences will be tested later.
	if (!readLastWriteTime(&newFiletime))
		return true;

	// we could successfully get the filetime, but before this it was 0 - definitely a change
	if (!(_lastWriteTime.dwHighDateTime || _lastWriteTime.dwLowDateTime))
		return true;

	// compare file times
	return (_lastWriteTime.dwLowDateTime != newFiletime.dwLowDateTime)
		|| (_lastWriteTime.dwHighDateTime != newFiletime.dwHighDateTime);

}

void Directory::setFilters(const std::vector<generic_string>& filters, bool autoread)
{
	if (autoread)
	{
		// autoread does an expensive read(), so we check equality first in this case.
		if (_filters != filters)
		{
			_filters = filters;
			read();
		}
	} 
	else
	{
		_filters = filters;
	}
}

void Directory::synchronizeTo(const Directory& other)
{
	onBeginSynchronize(other);
	for (auto &dir : _dirs)
		if (other._dirs.find(dir) == other._dirs.end())
			onDirRemoved(dir);
	for (auto& dir : other._dirs)
		if (_dirs.find(dir) == _dirs.end())
			onDirAdded(dir);
	for (auto& dir : _files)
		if (other._files.find(dir) == other._files.end())
			onFileRemoved(dir);
	for (auto& dir : other._files)
		if (_files.find(dir) == _files.end())
			onFileAdded(dir);
	onEndSynchronize(other);

	*this = other;

}

void Directory::setHideEmptyDirs(bool hideEmptyDirs, bool autoread)
{
	if (_hideEmptyDirs != hideEmptyDirs)
	{
		_hideEmptyDirs = hideEmptyDirs;
		if (autoread)
			read();
	}
}

// read last write time reads the last write time into "filetime".
// Under some circumstances, it might happen that the file time can not be determined, in this case it returns false.
bool Directory::readLastWriteTime(_Out_ FILETIME* filetime) const
{
	assert(filetime != NULL);
	if (filetime == NULL)
		return false;

	if (_path.empty())
	{
		filetime->dwHighDateTime = 0;
		filetime->dwLowDateTime = 0;
		return true;
	}

	// root directories need an approach with a "CreateFile"
	// The reason is, that a FindFirstFile on a root directory is not allowed.
	// This approach WOULD work for all other directories too, but while the directory is opened, it is also LOCKED, (FILE_SHARE_DELETE does not work for the directory itself)
	// So, we don't use this for other directories.
	// A root directory is not often removed; so locking it for a very short period should not be a problem.
	// The worst thing which could probably happen, is that a "Safe remove hardware" is denied under very awkward circumstances.
	if (PathIsRoot(_path.c_str()))
	{
		HANDLE hDir = CreateFile(_path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hDir == INVALID_HANDLE_VALUE)
			return false;

		FILETIME creationTime, lastAccessTime, lastWriteTime;
		bool fileTimeResult = GetFileTime( hDir, &creationTime, &lastAccessTime, &lastWriteTime) != 0;
		CloseHandle(hDir);
		if (fileTimeResult)
		{
			*filetime = lastWriteTime;
			return true;
		}
			
		return false;
	}


	generic_string searchPath(_path+TEXT("\\."));

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &fd);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	FindClose(hFind);

	*filetime = fd.ftLastWriteTime;
	return true;

}

bool Directory::operator==(const Directory& other) const
{
	if (_exists != other._exists
		|| _dirs.size() != other._dirs.size()
		|| _files.size() != other._files.size())
		return false;

	return _dirs == other._dirs && _files == other._files;
}

bool Directory::operator!=(const Directory& other) const
{
	return !operator== (other);
}

// necessary to access root directory file time
void Directory::enablePrivileges()
{
	static bool privilegesEnabled = false;
	if (privilegesEnabled)
		return;

	enablePrivilege(SE_BACKUP_NAME);
	enablePrivilege(SE_RESTORE_NAME);

	privilegesEnabled = true;
}

bool Directory::enablePrivilege(LPCTSTR privilegeName)
{

	bool result = false;
	HANDLE hToken;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		return false;

	TOKEN_PRIVILEGES tokenPrivileges = { 1 };

	if (LookupPrivilegeValue(NULL, privilegeName, &tokenPrivileges.Privileges[0].Luid))
	{
		tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, sizeof(tokenPrivileges), NULL, NULL);
		result = (GetLastError() == ERROR_SUCCESS);
	}
	CloseHandle(hToken);

	return result;
}
