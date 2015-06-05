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


#ifndef DIRECTORYWATCHER_H
#define  DIRECTORYWATCHER_H

#include <map>
#include <set>

#include "resource.h"
#include "Locking.h"
#include "Directory.h"

#define DIRECTORYWATCHER_UPDATE			DIRECTORYWATCHER_USER
#define DIRECTORYWATCHER_UPDATE_DONE	DIRECTORYWATCHER_USER + 1


#define READBUFFER_SIZE 4096

// this class monitors specific directories for changes in a separate thread.
// If a change occurs, it sends a DIRECTORYWATCHER_UPDATE message to the owner's window, with a tree item handle in lparam.
//
// It works by polling all directories in a given update frequency.
// If changes are detected, and DIRECTORYWATCHER_UPDATE have been sent during a cycle, at last a DIRECTORYWATCHER_UPDATE_DONE is sent.

// The main part of this class is a thread, which constantly polls an arbitrary number of directories for changes in a given frequency.
// The first attempt was to use 

class DirectoryWatcher
{

	class DirectoryWatcherDirectory {
		enum State {starting, online, stopping, invalid, offline};
		State _state;
		generic_string _dir;
		HANDLE _hDir;
		HTREEITEM _hTreeItem;
		OVERLAPPED  _overlapped;
		CHAR _buffer[READBUFFER_SIZE];
		DWORD _bufferLength;
		HANDLE* _hCompletionPort;

	public:
		DirectoryWatcherDirectory( const generic_string& dir, HTREEITEM hTreeItem, HANDLE* hCompletionPort) 
			: _state(starting) 
			, _dir(dir)
			, _hDir(INVALID_HANDLE_VALUE)
			, _hTreeItem(hTreeItem)
			, _hCompletionPort(hCompletionPort)
		{
		}

		virtual ~DirectoryWatcherDirectory() {
			if (_hDir != INVALID_HANDLE_VALUE)
				CloseHandle(_hDir);
		}

		void startWatch() {
			if (!PathFileExists(_dir.c_str()))
			{
				if (_state != offline)
				{
					CloseHandle( _hDir );
					_hDir = INVALID_HANDLE_VALUE;
					_state = offline;
				}
				return;
			}
			assert (_hDir == INVALID_HANDLE_VALUE);
			_hDir = CreateFile(_dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED, NULL);
			if( _hDir == INVALID_HANDLE_VALUE )
			{
				_state = offline;
				return;
			}

			*_hCompletionPort = CreateIoCompletionPort(_hDir, *_hCompletionPort, (DWORD)this, 0);
			if( !_hCompletionPort )
			{
				CloseHandle( _hDir );
				_hDir = INVALID_HANDLE_VALUE;
				_state = offline;
				return;
			}
			PostQueuedCompletionStatus(*_hCompletionPort, sizeof(DirectoryWatcherDirectory), (ULONG_PTR)this, &_overlapped);
			_state = online;
		}

		bool isStarting() const { return _state == starting; }
		bool isOnline() const { return _state == online; }

		HTREEITEM getTreeItem() const { return _hTreeItem; }

		bool checkOnlineStatusChanged() {

			if (_state != offline && _state != online)
				return false;

			bool exists = PathFileExists(_dir.c_str()) != 0;
			bool online = _state != offline;
			bool stateChanged = online != exists;

			if (stateChanged)
				startWatch();

			return stateChanged;

		}

		bool invoke() {
			if (_state != starting && _state != online)
				return false;

			// we are not interested in any details, so we always clear the buffer without evaluating it
			memset(&_overlapped, 0, sizeof(OVERLAPPED));
			if( !ReadDirectoryChangesW( _hDir, _buffer, READBUFFER_SIZE, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
							            &_bufferLength, &_overlapped, NULL))
			{
				//TODO
DWORD lorem = GetLastError();
				return false;
			}

			_state = online;
			return true;
		}

	};

	HANDLE _hThread;
	HANDLE _hRunningEvent, _hStopEvent, _hUpdateEvent;

	HANDLE _hCompletionPort;

	bool _running;
	HWND _hWnd;

	DWORD _updateFrequencyMs;
	bool _changeOccurred;

	Lock _lock;

	bool _watching;

	std::set<DirectoryWatcherDirectory*> _watchedDirectories;
	std::set<HTREEITEM> _changedItems;

public:

	DirectoryWatcher(HWND hWnd, DWORD updateFrequencyMs = 1000);
	virtual ~DirectoryWatcher();

	// startThread() must be called manually after creation. Throws std::runtime_error if fails to create events/resources (not very likely)
	void startThread();
	// stopThread() is called automatically on destruction
	void stopThread();

	void addDir(const generic_string& path, HTREEITEM treeItem);
	void removeDir(const generic_string& path, HTREEITEM treeItem);
	void removeAllDirs();

	bool getWatching() const { return _watching; }
	void setWatching(bool val) { Scopelock lock(_lock); _watching = val; }

	void update();

private:
	int thread();
	static DWORD threadFunc(LPVOID data);
	bool post(HTREEITEM item, UINT message = DIRECTORYWATCHER_UPDATE);
	bool enablePrivilege(LPCTSTR privName);


	// noncopyable
	DirectoryWatcher& operator= (const DirectoryWatcher&) {}
	DirectoryWatcher(const DirectoryWatcher&) {}


};

#endif

