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


DirectoryWatcher::DirectoryWatcher(HWND hWnd, const generic_string& filePath) 
	: _hWnd(hWnd)
	, _filePath(filePath)
	, _treeItem(NULL)
	, _hThread(NULL)
	, _hRunningEvent(NULL)
	, _hStopEvent(NULL)
	, _running(false)
{
}


DirectoryWatcher::DirectoryWatcher(const DirectoryWatcher& other) 
	: _hWnd(other._hWnd)
	, _filePath(other._filePath)
	, _treeItem(NULL)
	, _hThread(NULL)
	, _hRunningEvent(NULL)
	, _hStopEvent(NULL)
	, _running(false)
{
}

DirectoryWatcher::~DirectoryWatcher()
{
	stopThread();
}

void DirectoryWatcher::startThread(HTREEITEM treeItem)
{
	_treeItem = treeItem;
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

void DirectoryWatcher::stopThread()
{
	if (_running)
	{
		::SetEvent(_hStopEvent);
		::WaitForSingleObject(_hThread, INFINITE);
		::CloseHandle(_hThread);
		::CloseHandle(_hRunningEvent);
		::CloseHandle(_hStopEvent);
		_running = false;
		_hThread = NULL;
		_hRunningEvent = NULL;
		_hStopEvent = NULL;
		_treeItem = NULL;
	}

}

// this is the main thread routine to monitor directory changes.
// Unfortunately, Windows afaik offers no way to monitor a non-existent directory. FindFirstChangeNotification() returns INVALID_HANDLE_VALUE in this case.
// It might be possible to monitor the next valid parent dir, but this is complicated (even drive letters may be missing).
// So we choose another way:
//
// There are two modes, an existing mode and a non-existing mode.
//
// The existing mode waits for either a directory change or the thread stop signal.
// If it finds a directory change, it informs the tree view about it.
// It then checks, if the directory has been removed completely, and if so, it switches to non-existing mode.
// If not, it continues to wait for changes.
//
// The non-existing mode waits for 2 seconds for the thread stop signal.
// If the 2 secs have passed without a stop signal, it checks the existence of the directory. If it now exists, it informs the tree view and switches to existing mode.
// If it still does not exist, it continues waiting.

int DirectoryWatcher::thread()
{
	HANDLE hFindChange = FindFirstChangeNotification(_filePath.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (!hFindChange)
	{
		return -1;
	}
	bool nonExistentMode = hFindChange == INVALID_HANDLE_VALUE;

	_running = true;
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
					post();
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
				post();
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

void DirectoryWatcher::post()
{
	SendMessage(_hWnd, DIRECTORYWATCHER_UPDATE, 0, (LPARAM)_treeItem);
}
