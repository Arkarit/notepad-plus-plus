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
#include <sstream>
#include <iomanip>

#include "resource.h"
#include "Locking.h"
#include "Directory.h"

#define DIRECTORYWATCHER_UPDATE			DIRECTORYWATCHER_USER

#define READBUFFER_SIZE 4096

// Thread signals stop and update
#define PQCS_SIGNAL_STOP 0
#define PQCS_SIGNAL_UPDATE 1


// This class monitors specific directories for changes in a separate thread.
// If a change occurs, it sends a DIRECTORYWATCHER_UPDATE message to the owner's window, with a tree item handle in lparam.
//
// It works by 
// a) watching valid directories by using an IoCompletionPort/ReadDirectoryChangesW and
// b) watching offline directories (e.g. removed USB drive, offline network drive) by periodically polling them for getting online again and
//    getting them back into a) when they are.
// The latter is done, because ReadDirectoryChangesW is not very good in monitoring offline directories; the parent directory would have to be
// watched, which would have made this whole stuff even more complicated.
//
// According to Notepad++'s simple approach, this class is specialized to only support TreeView's.
// Only "name changes" (rename, create, delete) are watched, since only those are currently interesting for Notepad++.
// Once a change is detected, a DIRECTORYWATCHER_UPDATE message with the according HTREEITEM in lparam is sent to the window, which was set in ctor.
// The details provided by ReadDirectoryChangesW are discarded; the receiving class is responsible for updating the directory itself.
// The change messages are sent a bit delayed to combine as many as possible into one, except on creation (otherwise each tree item open would
// last a second or so).
//
// Credits go to Wes Jones http://www.codeproject.com/Articles/950/CDirectoryChangeWatcher-ReadDirectoryChangesW-all
// for explaining the technique of this microsoftish complex approach.
// According to his license, no code of his classes was used directly, it was only used as a source for inspiration.

class DirectoryWatcher
{

	// each watched directory has a WatchedDirectory instance, which is responsible for monitoring the directory.
	// It may have multiple tree items assigned to it, since multiple tree items may refer to the same directory.
	class WatchedDirectory {
		enum State {starting, online, moribund, offline};
		State _state;
		generic_string _dir;
		HANDLE _hDir;
		std::set<HTREEITEM> _treeItems;
		OVERLAPPED  _overlapped;
		CHAR _buffer[READBUFFER_SIZE];
		DWORD _bufferLength;
		HANDLE* _hCompletionPort;
		bool _immediate;
	public:
		WatchedDirectory( const generic_string& dir, HANDLE* hCompletionPort);
		virtual ~WatchedDirectory();

		// one WatcherDirectory can have multiple tree items assigned to it, all of which have to be informed when the directory changes.
		void addTreeItem(HTREEITEM treeItem);
		void removeTreeItem(HTREEITEM treeItem);
		size_t numTreeItems() const;
		const std::set<HTREEITEM>& getTreeItems() const {
			return _treeItems; 
		}

		// states
		bool isStarting() const { return _state == starting; }
		bool isOnline() const { return _state == online; }
		bool isOffline() const { return _state == offline; }
		bool isMoribund() const { return _state == moribund; }

		// immediate posting flag - true directly after creation, otherwise always false
		bool isImmediate() const { return _immediate; }

		// WatchedDirectory is a bit bitchy on deleting.
		// If you simply delete it from outside, it releases its directory handle in the dtor - and this leads
		// to a GetQueuedCompletionStatus invocation with an invalid pointer to the deleted this!
		// So, if the directory is online, in this call only the directory handle is released.
		// The final deletion then takes place in the invoked GetQueuedCompletionStatus in the worker thread.
		// For offline directories, this is not the case, and so it is deleted instantly here.
		// 
		// returns true if instantly deleted, false on queued deletion
		bool deleteOrSetMoribund();

		// start watching this directory; assign it to the ioCompletionPort
		void startWatch();

		// This is called periodically in the worker thread and switches the technique between off- and online
		bool checkOnlineStatusChanged();

		// Invoke watching
		bool invoke();

	};

	HANDLE _hThread;
	HANDLE _hRunningEvent;

	HANDLE _hCompletionPort;

	bool _running;
	HWND _hWnd;

	DWORD _updateFrequencyMs;

	Lock _lock;

	std::map<generic_string,WatchedDirectory*> _watchedDirectoriesMap;
	std::set<WatchedDirectory*> _validDirectories;
	std::set<HTREEITEM> _changedItems;

public:

	DirectoryWatcher(HWND hWnd, DWORD updateFrequencyMs = 1000);
	virtual ~DirectoryWatcher();

	// startThread() must be called manually after creation. Throws std::runtime_error if fails to create events/resources (not very likely)
	void startThread();
	// stopThread() is called automatically on destruction
	void stopThread();

	// adding and removing directories
	void addDir(const generic_string& path, HTREEITEM treeItem);
	void removeDir(const generic_string& path, HTREEITEM treeItem);
	void removeAllDirs();

	// force an update (instant posting of all delayed messages)
	void update() {
		PostQueuedCompletionStatus(_hCompletionPort, 0, PQCS_SIGNAL_UPDATE, NULL);
	}

private:
	int thread();
	static DWORD threadFunc(LPVOID data);
	bool post(HTREEITEM item, UINT message = DIRECTORYWATCHER_UPDATE);
	void post(std::set<HTREEITEM>& items, UINT message = DIRECTORYWATCHER_UPDATE);
	bool enablePrivilege(LPCTSTR privName);


	// noncopyable
	DirectoryWatcher& operator= (const DirectoryWatcher&) {}
	DirectoryWatcher(const DirectoryWatcher&) {}


};

#endif

