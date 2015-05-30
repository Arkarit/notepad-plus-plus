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


#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <set>

class Directory {
protected:
	std::set<generic_string> _dirs;
	std::set<generic_string> _files;
	bool _exists;
	generic_string _path;
public:
	Directory() : _exists(false) {}
	Directory( const generic_string& path );
	virtual ~Directory() {}

	const generic_string& getPath() const { return _path; }
	void setPath(const generic_string& path);
	bool exists() const { return _exists; }

	const std::set<generic_string>& getDirs() const { return _dirs; }
	const std::set<generic_string>& getFiles() const { return _files; }

	bool operator== (const Directory& other) const;
	bool operator!= (const Directory& other) const;

	// synchronizeTo is basically like a copy constructor, with the difference, that it calls the
	// following virtual functions onDirAdded(), ...
	// Using this makes only sense, if you have subclassed this and evaluate those virtual methods; otherwise just use the copy ctor/assigment operator.
	// Note that the virtual methods are called BEFORE the contents of this directory are really changed.
	void synchronizeTo(const Directory& other);

protected:

	virtual void onDirAdded(const generic_string&) {}
	virtual void onDirRemoved(const generic_string&) {}
	virtual void onFileAdded(const generic_string&) {}
	virtual void onFileRemoved(const generic_string&) {}


};


#endif