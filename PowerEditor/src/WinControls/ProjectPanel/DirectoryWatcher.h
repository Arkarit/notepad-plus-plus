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

#define DIRECTORYWATCHER_UPDATE DIRECTORYWATCHER_USER

// this class monitors a specific directory for changes in a separate thread.
// If a change occurs, it sends a DIRECTORYWATCHER_UPDATE message to the owner's window, with a tree item handle in lparam.
//
// unfortunately, its usage is a bit unhandy due to the process of creating a tree view element:
// 1. Create your user data containing this DirectoryWatcher
// 2. Create the tree view element with a pointer to user data
// 3. Start the DirectoryWatcher thread, passing the new tree view element.
//
// Note that this also applies to the copy constructor.
// It only creates a non-running copy, which has to be started manually with startThread() (multiple threads for a single tree item make no sense at all).
//

class DirectoryWatcher
{
	friend static DWORD threadFunc(LPVOID data);
	generic_string _filePath;
	HANDLE _hThread;
	HANDLE _hRunningEvent, _hStopEvent, _hUpdateEvent;
	bool _running;
	HWND _hWnd;

	std::map<generic_string,Directory*> _watchdirs;
	std::multimap<generic_string,HTREEITEM> _dirItems;
	DWORD _updateFrequencyMs;

	Lock _lock;

	std::multimap<generic_string,HTREEITEM> _dirItemsToAdd;
	std::multimap<generic_string,HTREEITEM> _dirItemsToRemove;

	bool _watching;
public:

	DirectoryWatcher(HWND hWnd, DWORD updateFrequencyMs = 333);
	virtual ~DirectoryWatcher();

	void addDir(const generic_string& _path, HTREEITEM _treeItem);
	void removeDir(const generic_string& _path, HTREEITEM _treeItem);
	void removeAllDirs();

	bool getWatching() const { return _watching; }
	void setWatching(bool _val) { Scopelock lock(_lock); _watching = _val; }

	void update();



private:
	bool startThread();
	void stopThread();
	int thread();
	static DWORD threadFunc(LPVOID data);
	bool post(HTREEITEM item);
	void iterateDirs();
	void updateDirs();

	// noncopyable
	DirectoryWatcher& operator= (const DirectoryWatcher&) {}
	DirectoryWatcher(const DirectoryWatcher&) {}

};

#endif

