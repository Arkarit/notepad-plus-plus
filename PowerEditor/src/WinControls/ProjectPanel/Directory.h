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
	virtual void onDirAdded(const generic_string&) {}
	virtual void onDirRemoved(const generic_string&) {}
	virtual void onFileAdded(const generic_string&) {}
	virtual void onFileRemoved(const generic_string&) {}


};


#endif