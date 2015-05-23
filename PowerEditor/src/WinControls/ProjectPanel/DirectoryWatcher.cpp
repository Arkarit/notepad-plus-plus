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
#include "DirectoryWatcher.h"


DirectoryWatcher::DirectoryWatcher(const generic_string& filePath) : _filePath(filePath), _hThread(NULL), _hRunningEvent(NULL), _hStopEvent(NULL), _valid(false)
{
	// Create manually reset non-signaled event
	_hRunningEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!_hRunningEvent)
		return;
	_hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!_hStopEvent)
	{
		::CloseHandle(_hRunningEvent);
		return;
	}
	_hThread = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadFunc, (LPVOID)this, 0, NULL);
	if (!_hThread)
	{
		::CloseHandle(_hRunningEvent);
		::CloseHandle(_hStopEvent);
		return;
	}

	::WaitForSingleObject(_hRunningEvent, INFINITE);
}


DirectoryWatcher::~DirectoryWatcher ()
{
	if (_valid)
	{
		::SetEvent(_hStopEvent);
		::WaitForSingleObject(_hThread, INFINITE);
		::CloseHandle(_hThread);
		::CloseHandle(_hRunningEvent);
		::CloseHandle(_hStopEvent);
	}

}

int DirectoryWatcher::thread()
{
	HANDLE hFindChange = FindFirstChangeNotification(_filePath.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (!hFindChange)
	{
		return -1;
	}
	bool nonExistentMode = hFindChange == INVALID_HANDLE_VALUE;

	_valid = true;
	::SetEvent(_hRunningEvent);

	HANDLE handles[2];
	handles[0] = hFindChange;
	handles[1] = _hStopEvent;

	bool exitThread = false;

	while (!exitThread)
	{

		if (nonExistentMode)
		{
			// if the directory we wanted to monitor does not exist, we have no other chance than to poll it for creation.
			if (!PathFileExists(_filePath.c_str()))
			{
				// poll every 2 secs
				DWORD cause = WaitForSingleObject(_hStopEvent, 2000);
				if (cause == WAIT_OBJECT_0)
					exitThread = true;
			}
			else
			{
				if (hFindChange != INVALID_HANDLE_VALUE)
					FindCloseChangeNotification(hFindChange);

				hFindChange = FindFirstChangeNotification(_filePath.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
				handles[0] = hFindChange;
				if (hFindChange != INVALID_HANDLE_VALUE)
				{
					nonExistentMode = false;
					//TODO: send message to update the directory tree
				}
			}
		}
		else
		{
			DWORD cause = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
			if (cause == WAIT_FAILED)
			{
				DWORD error = GetLastError();
				if (error == ERROR_INVALID_HANDLE)
				{
					// the directory was removed
					nonExistentMode = true;
				}
				else
					exitThread = true;
			}
			else if (cause == WAIT_OBJECT_0)
			{
				//TODO: send message to update the directory tree
				if (PathFileExists(_filePath.c_str()))
					FindNextChangeNotification(hFindChange);
				else
					nonExistentMode = true;
			}
			else
				exitThread = true;
		}
	}

	if (hFindChange != INVALID_HANDLE_VALUE)
		FindCloseChangeNotification(hFindChange);

	return 0;

}

DWORD DirectoryWatcher::threadFunc(LPVOID data)
{
	DirectoryWatcher* pw = static_cast<DirectoryWatcher*>(data);
	return (DWORD)pw->thread();

}
