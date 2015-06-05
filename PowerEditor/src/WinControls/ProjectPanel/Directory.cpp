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

#include "precompiledHeaders.h"
#include "Directory.h"

Directory::Directory()
	: _exists(false)
{
}

Directory::Directory(const generic_string& path)
{
	read(path);
}

void Directory::read(const generic_string& path)
{
	_path = path;
	_exists = false;
	_files.clear();
	_dirs.clear();

	generic_string searchPath = _path + TEXT("\\*.*");

	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &fd);

	if (hFind == INVALID_HANDLE_VALUE)
		return;

	_exists = true;

	while (hFind != INVALID_HANDLE_VALUE)
	{
		const generic_string file(fd.cFileName);

		struct _WIN32_FILE_ATTRIBUTE_DATA m_attr;
		if (!GetFileAttributesExW((path + TEXT("\\") + file).c_str(), GetFileExInfoStandard, &m_attr))
		{
			if (!FindNextFile(hFind, &fd))
				break;
			continue;
		}


		if (m_attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (file == TEXT(".") || file == TEXT(".."))
			{
				if (!FindNextFile(hFind, &fd))
					break;
				continue;
			}

			_dirs.insert(file);
		}
		else
		{
			_files.insert(file);
		}

		if (!FindNextFile(hFind, &fd))
			break;
	}

	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);
}

void Directory::synchronizeTo(const Directory& other)
{
	onBeginSynchronize(other);
	for (auto it = _dirs.begin(); it != _dirs.end(); ++it)
		if (other._dirs.find(*it) == other._dirs.end())
			onDirRemoved(*it);
	for (auto it = other._dirs.begin(); it != other._dirs.end(); ++it)
		if (_dirs.find(*it) == _dirs.end())
			onDirAdded(*it);
	for (auto it = _files.begin(); it != _files.end(); ++it)
		if (other._files.find(*it) == other._files.end())
			onFileRemoved(*it);
	for (auto it = other._files.begin(); it != other._files.end(); ++it)
		if (_files.find(*it) == _files.end())
			onFileAdded(*it);
	onEndSynchronize(other);

	*this = other;

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

