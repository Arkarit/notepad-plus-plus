#ifndef LOCKING_H
#define LOCKING_H

#include <windows.h>

class Lock
{
protected:
    CRITICAL_SECTION critsec;
public:
    Lock() {
		InitializeCriticalSection(&critsec); 
	}
    virtual ~Lock() {
		DeleteCriticalSection(&critsec); 
	}
    void lock() { 
		EnterCriticalSection(&critsec); 
	}
    void unlock(){
		LeaveCriticalSection(&critsec); 
	}
#if(_WIN32_WINNT >= 0x0400)
	bool trylock(){
		return TryEnterCriticalSection(&critsec) != 0;
	}
#endif

private:
	// noncopyable
	Lock(const Lock&) {}
	Lock& operator= (const Lock&) {return *this;}

};

class Scopelock
{
protected:
    Lock* _lock;
public:
    Scopelock(Lock& lock): _lock(&lock)  { _lock->lock(); }
    virtual ~Scopelock()  { _lock->unlock(); }
private:
	// noncopyable
	Scopelock(const Scopelock&) {}
	Scopelock& operator= (const Scopelock&) {return *this;}

};

#endif
