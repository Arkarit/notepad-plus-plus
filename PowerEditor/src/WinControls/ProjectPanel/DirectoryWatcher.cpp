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
	, _hCompletionPort(NULL)
	, _running(false)
	, _updateFrequencyMs(updateFrequencyMs)
{
	// create a basic IoCompletionPort without any directories assigned to it
	_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

}

DirectoryWatcher::~DirectoryWatcher()
{
	removeAllDirs();
	stopThread();

	CloseHandle(_hCompletionPort);
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
		if (!_hRunningEvent)
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
		throw;
	}



}

void DirectoryWatcher::stopThread()
{
	Scopelock lock(_lock);


	if (_running)
	{
		assert(_validDirectories.empty() && "Please remove all directories before stopping thread");
		removeAllDirs(); // to be sure

		PostQueuedCompletionStatus(_hCompletionPort, 0, PQCS_SIGNAL_STOP, NULL);

		::WaitForSingleObject(_hThread, INFINITE);
		::CloseHandle(_hThread);
		::CloseHandle(_hRunningEvent);
		_running = false;
		_hThread = NULL;
		_hRunningEvent = NULL;
	}

}

int DirectoryWatcher::thread()
{

	_running = true;
	::SetEvent(_hRunningEvent);

    DWORD numBytes;
    LPOVERLAPPED lpOverlapped;


	for(;;)
	{
		WatchedDirectory* watcherDirectory = NULL;
		DWORD sig;
        bool queuedCompletionStatusResult = GetQueuedCompletionStatus(_hCompletionPort, &numBytes, &sig, &lpOverlapped, _updateFrequencyMs) != 0;

		if (sig == PQCS_SIGNAL_STOP)
			break;
		else if (sig == PQCS_SIGNAL_UPDATE)
			;//queuedCompletionStatusResult = false;
		else
			watcherDirectory = (WatchedDirectory*) sig;


		{
			Scopelock lock(_lock);

			for (auto it = _watchedDirectoriesMap.begin(); it != _watchedDirectoriesMap.end(); ++it)
			{
				WatchedDirectory* dir = it->second;
				if (dir->checkOnlineStatusChanged())
					_changedItems.insert(dir->getTreeItems().begin(), dir->getTreeItems().end());
			}
		}

		post(_changedItems);

		std::set<HTREEITEM> postImmediate;

		if (!queuedCompletionStatusResult || !watcherDirectory)
			continue;

		{
			Scopelock lock(_lock);
			if (watcherDirectory->isMoribund())
			{
				_validDirectories.erase(watcherDirectory);
				delete watcherDirectory;
				continue;
			}

			if (watcherDirectory->isStarting() || watcherDirectory->isOnline())
			{
				if (watcherDirectory->isImmediate())
					postImmediate.insert(watcherDirectory->getTreeItems().begin(), watcherDirectory->getTreeItems().end());
				else
					_changedItems.insert(watcherDirectory->getTreeItems().begin(), watcherDirectory->getTreeItems().end());
				watcherDirectory->invoke();
			}
		}

		post(postImmediate);

	}

	return 0;

}

DWORD DirectoryWatcher::threadFunc(LPVOID data)
{
	DirectoryWatcher* pw = static_cast<DirectoryWatcher*>(data);
	return (DWORD)pw->thread();

}

bool DirectoryWatcher::post(HTREEITEM item, UINT message)
{
	LRESULT smResult = SendMessageTimeout(_hWnd, message, 0, (LPARAM)item, SMTO_ABORTIFHUNG, 10000, NULL);
	return smResult != 0;
}


void DirectoryWatcher::post(std::set<HTREEITEM>& items, UINT message /*= DIRECTORYWATCHER_UPDATE*/)
{
	// lost messages (posting failed) are postponed
	std::set<HTREEITEM> lostMessages;
	for (auto it = items.begin(); it != items.end(); ++it)
		if (!post(*it, message))
			lostMessages.insert(*it);
	items.clear();

	// avoid overflow
	if (_changedItems.size() < 2000 && lostMessages.size() < 2000)
		_changedItems.insert(lostMessages.begin(), lostMessages.end());
}

void DirectoryWatcher::addDir(const generic_string& path, HTREEITEM treeItem)
{
	Scopelock lock(_lock);

	if (_watchedDirectoriesMap.find(path) == _watchedDirectoriesMap.end())
	{
		WatchedDirectory* dir = new WatchedDirectory(path, &_hCompletionPort);
		_watchedDirectoriesMap[path] = dir;
		dir->addTreeItem(treeItem);
		_validDirectories.insert(dir);
		dir->startWatch();
	}
	else
	{
		_watchedDirectoriesMap[path]->addTreeItem(treeItem);
	}
	
}

void DirectoryWatcher::removeDir(const generic_string& path, HTREEITEM treeItem)
{
	Scopelock lock(_lock);
	assert (_watchedDirectoriesMap.find(path) != _watchedDirectoriesMap.end());
	if (_watchedDirectoriesMap.find(path) == _watchedDirectoriesMap.end())
		return;
	WatchedDirectory* wDir = _watchedDirectoriesMap[path];
	wDir->removeTreeItem(treeItem);
	if (wDir->numTreeItems() == 0)
	{
		if (!_running)
		{
			// This should only occur (and is only allowed), if the worker thread could not be created on begin.
			// hard delete all watched directories, and don't wait for the thread to delete them.
			_watchedDirectoriesMap.erase(path);
			delete wDir;
			_validDirectories.erase(wDir);
		}
		else
		{
			_watchedDirectoriesMap.erase(path);
			if (wDir->deleteOrSetMoribund())
				_validDirectories.erase(wDir);
		}
	}
}

void DirectoryWatcher::removeAllDirs()
{
	{
		Scopelock lock(_lock);
		if (!_running)
		{
			// This should only occur (and is only allowed), if the worker thread could not be created on begin.
			// hard delete all watched directories, and don't wait for the thread to delete them.
			while (!_validDirectories.empty())
			{
				delete *_validDirectories.begin();
				_validDirectories.erase(_validDirectories.begin());
			}
		}
		else
		{
			for (auto it =_watchedDirectoriesMap.begin(); it != _watchedDirectoriesMap.end(); ++it)
			{
				if (it->second->deleteOrSetMoribund())
					_validDirectories.erase(it->second);
			}
		}
	}
	int count=0;
	while (!_validDirectories.empty())
	{
		Sleep(50);
		// emergency exit if deleting directories takes too long (4 seconds). Better safe than sorry.
		if (count++ > 80)
		{
			assert(0);
			break;
		}

	}
	_watchedDirectoriesMap.clear();
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

DirectoryWatcher::WatchedDirectory::WatchedDirectory(const generic_string& dir, HANDLE* hCompletionPort) : _state(starting)
, _dir(dir)
, _hDir(INVALID_HANDLE_VALUE)
, _hCompletionPort(hCompletionPort)
, _immediate(false)
{
	std::wstringstream ss;
	ss << TEXT("created: 0x") << std::hex << std::setfill(TEXT('0')) << std::setw(8) << (unsigned long) this << TEXT("\n");
	OutputDebugStringW(ss.str().c_str());
}

DirectoryWatcher::WatchedDirectory::~WatchedDirectory()
{
	std::wstringstream ss;
	ss << TEXT("deleted: 0x") << std::hex << std::setfill(TEXT('0')) << std::setw(8) << (unsigned long) this << TEXT("\n");
	OutputDebugStringW(ss.str().c_str());
	if (_hDir != INVALID_HANDLE_VALUE)
		CloseHandle(_hDir);
}

void DirectoryWatcher::WatchedDirectory::addTreeItem(HTREEITEM treeItem)
{
	assert(_treeItems.find(treeItem) == _treeItems.end());
	_treeItems.insert(treeItem);
}

void DirectoryWatcher::WatchedDirectory::removeTreeItem(HTREEITEM treeItem)
{
	assert(_treeItems.find(treeItem) != _treeItems.end());
	_treeItems.erase(treeItem);
}

size_t DirectoryWatcher::WatchedDirectory::numTreeItems() const
{
	return _treeItems.size();
}

bool DirectoryWatcher::WatchedDirectory::deleteOrSetMoribund()
{

	_state = moribund;
	if (_hDir != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_hDir);
		_hDir = INVALID_HANDLE_VALUE;
		return false;
	}
	else
	{
		delete this;
		return true;
	}
}

void DirectoryWatcher::WatchedDirectory::startWatch()
{
	_immediate = true;
	if (!PathFileExists(_dir.c_str()))
	{
		if (_state != offline)
		{
			CloseHandle(_hDir);
			_hDir = INVALID_HANDLE_VALUE;
			_state = offline;
		}
		return;
	}
	assert(_hDir == INVALID_HANDLE_VALUE);
	_hDir = CreateFile(_dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
	if (_hDir == INVALID_HANDLE_VALUE)
	{
		_state = offline;
		return;
	}

	*_hCompletionPort = CreateIoCompletionPort(_hDir, *_hCompletionPort, (DWORD)this, 0);
	if (!_hCompletionPort)
	{
		CloseHandle(_hDir);
		_hDir = INVALID_HANDLE_VALUE;
		_state = offline;
		return;
	}
	PostQueuedCompletionStatus(*_hCompletionPort, sizeof(WatchedDirectory), (ULONG_PTR)this, &_overlapped);
	_state = online;
}

bool DirectoryWatcher::WatchedDirectory::checkOnlineStatusChanged()
{
	if (_state != offline && _state != online)
		return false;

	bool exists = PathFileExists(_dir.c_str()) != 0;
	bool online = _state != offline;
	bool stateChanged = online != exists;

	if (stateChanged)
		startWatch();

	return stateChanged;
}

bool DirectoryWatcher::WatchedDirectory::invoke()
{

	_immediate = false;

	if (_state != starting && _state != online)
		return false;

	// we are not interested in any details, so we always clear the buffer without evaluating it
	memset(&_overlapped, 0, sizeof(OVERLAPPED));
	if (!ReadDirectoryChangesW(_hDir, _buffer, READBUFFER_SIZE, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
		&_bufferLength, &_overlapped, NULL))
	{
		_state = offline;
		return false;
	}

	_state = online;
	return true;
}
