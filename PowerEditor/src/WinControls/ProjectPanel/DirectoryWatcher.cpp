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

#include <set>


DirectoryWatcher::DirectoryWatcher(HWND hWnd, DWORD updateFrequencyMs) 
	: _hWnd(hWnd)
	, _hThread(NULL)
	, _hRunningEvent(NULL)
	, _hStopEvent(NULL)
	, _hUpdateEvent(NULL)
	, _hCompletionPort(NULL)
	, _running(false)
	, _updateFrequencyMs(updateFrequencyMs)
	, _watching(true)
	, _changeOccurred(false)
{

	_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

}

DirectoryWatcher::~DirectoryWatcher()
{
	stopThread();
	removeAllDirs();

	CloseHandle(_hCompletionPort);
}

void DirectoryWatcher::update()
{
	::SetEvent(_hUpdateEvent);
}

void DirectoryWatcher::startThread()
{

	try {
		Scopelock lock(_lock);
		if (_running)
			return;

		if (!enablePrivilege(SE_BACKUP_NAME))
			throw std::runtime_error("Could not enable privilege SE_BACKUP_NAME");
		if (!enablePrivilege(SE_RESTORE_NAME))
			throw std::runtime_error("Could not enable privilege SE_RESTORE_NAME");
		if (!enablePrivilege(SE_CHANGE_NOTIFY_NAME))
			throw std::runtime_error("Could not enable privilege SE_CHANGE_NOTIFY_NAME");

		_hRunningEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		_hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		_hUpdateEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!_hStopEvent || !_hRunningEvent || !_hUpdateEvent)
			throw std::runtime_error("Could not create DirectoryWatcher events");

		_hThread = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadFunc, (LPVOID)this, 0, NULL);
		if (!_hThread)
			throw std::runtime_error("Could not create DirectoryWatcher thread");

		::WaitForSingleObject(_hRunningEvent, INFINITE);
	}
	catch (...)
	{
		if (_hRunningEvent)
			::CloseHandle(_hRunningEvent);
		if (_hStopEvent)
			::CloseHandle(_hStopEvent);
		if (_hUpdateEvent)
			::CloseHandle(_hUpdateEvent);
		throw;
	}



}

void DirectoryWatcher::stopThread()
{
	Scopelock lock(_lock);

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
	}

}

int DirectoryWatcher::thread()
{

	_running = true;
	::SetEvent(_hRunningEvent);

    DWORD numBytes;
    DirectoryWatcherDirectory* pdi;
    LPOVERLAPPED lpOverlapped;


	for(;;)
	{
        bool queuedCompletionStatus = GetQueuedCompletionStatus(_hCompletionPort, &numBytes, (LPDWORD) &pdi, &lpOverlapped, _updateFrequencyMs) != 0;
		
		
		

		{
			Scopelock lock(_lock);
			for (auto it = _watchedDirectories.begin(); it != _watchedDirectories.end(); ++it)
			{
				DirectoryWatcherDirectory* dir = *it;
				if (dir->checkOnlineStatusChanged())
					_changedItems.insert(dir->getTreeItem());
			}
		}
		for (auto it = _changedItems.begin(); it != _changedItems.end(); ++it)
			post(*it);
		_changedItems.clear();


		if (!queuedCompletionStatus)
			continue;

		if (pdi->isStarting() || pdi->isOnline())
		{
			pdi->invoke();
			_changedItems.insert(pdi->getTreeItem());
			continue;
		}


	}

}

DWORD DirectoryWatcher::threadFunc(LPVOID data)
{
	DirectoryWatcher* pw = static_cast<DirectoryWatcher*>(data);
	return (DWORD)pw->thread();

}

bool DirectoryWatcher::post(HTREEITEM item, UINT message)
{
	if (message == DIRECTORYWATCHER_UPDATE)
		_changeOccurred = true;

	LRESULT smResult = SendMessageTimeout(_hWnd, message, 0, (LPARAM)item, SMTO_ABORTIFHUNG, 10000, NULL);
	return smResult != 0;
}


void DirectoryWatcher::addDir(const generic_string& path, HTREEITEM treeItem)
{
	Scopelock lock(_lock);

	DirectoryWatcherDirectory* dir = new DirectoryWatcherDirectory(path, treeItem, &_hCompletionPort);


	_watchedDirectories.insert(dir);

	dir->startWatch();

	
}

void DirectoryWatcher::removeDir(const generic_string& path, HTREEITEM treeItem)
{
	Scopelock lock(_lock);
}

void DirectoryWatcher::removeAllDirs()
{
	Scopelock lock(_lock);

}


bool DirectoryWatcher::enablePrivilege(LPCTSTR privilegeName)
{

	bool result = false;
	HANDLE hToken;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES tokenPrivileges = { 1 };

		if (LookupPrivilegeValue(NULL, privilegeName, &tokenPrivileges.Privileges[0].Luid))
		{
			tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, sizeof(tokenPrivileges), NULL, NULL);
			result = (GetLastError() == ERROR_SUCCESS);
		}
		CloseHandle(hToken);
	}
	return(result);
}

