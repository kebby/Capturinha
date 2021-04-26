//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once
#include "types.h"

// these are your entry points
// -------------------------------------------------------------------------------

struct ScreenMode
{
    uint width;
    uint height;
    bool fullscreen;
};

// time
// -------------------------------------------------------------------------------

int64 GetTicks(); // raw timer ticks
double GetTime(); // time since program start in seconds

struct SystemTime {
    uint year;
    uint month;
    uint dayOfWeek;
    uint day;
    uint hour;
    uint minute;
    uint second;
    uint milliseconds;
};

SystemTime GetSystemTime(); // wall-clock time

// streams / files
// -------------------------------------------------------------------------------

struct Stream
{
    enum class From { Start, Current, End };

    virtual ~Stream() {};

    virtual bool CanRead() const { return false; }
    virtual bool CanWrite() const { return false; }
    virtual bool CanSeek() const { return false; }

    virtual uint64 Read(void* ptr, uint64 len) = 0;
    virtual uint64 Write(const void* ptr, uint64 len) = 0;


    virtual uint64 Length() const { return 0; }
    virtual uint64 Seek(int64, From) { return 0; }
    virtual uint64 Pos() { return (uint64)Seek(0, From::Current); }

    virtual RCPtr<Buffer> Map() { return RCPtr<Buffer>(); }
};

enum class OpenFileMode
{
    Read,
    Create,
    Append,
    RandomAccess,
};

bool FileExists(const char* path);

Stream* OpenFile(const char* path, OpenFileMode mode = OpenFileMode::Read);
RCPtr<Buffer> LoadFile(const char* path);

String ReadFileUTF8(const char* path);
void WriteFileUTF8(const String& str, const char* path);

RCPtr<Buffer> LoadResource(int name, int type);

// debug output
// -------------------------------------------------------------------------------

#if _DEBUG
void DPrintF(const char* format, ...);
#else
inline void DPrintF(const char*, ...) {}
#endif
[[ noreturn ]]
void Fatal(const char* format, ...);

void DbgOpenLog(const char* filename);
void DbgCloseLog();


// multithreading
// -------------------------------------------------------------------------------

class ThreadLock
{
public:
    ThreadLock();
    ~ThreadLock();
    void Lock();
    void Unlock();

private:
    void* P = nullptr;
};

class ScopeLock
{
public:
    ScopeLock(ThreadLock& lock) : Lock(lock) { Lock.Lock(); }
    ~ScopeLock() { Lock.Unlock(); }

private:
    ThreadLock &Lock;
};

// -------------------------------------------------------------------------------

class ThreadEvent
{
public:
    ThreadEvent(bool autoReset = true);
    ~ThreadEvent();

    void Fire();
    void Reset();
    void Wait();
    bool Wait(int timeoutMs);

    void* GetRawEvent() const;

private:
    void* P = nullptr;
};

// -------------------------------------------------------------------------------

class Thread
{
public:
    Thread(Func<void (Thread&)> threadFunc);
    ~Thread();

    bool IsRunning() { return !ExitEv.Wait(0); }

    void Wait() { ExitEv.Wait(); }
    bool Wait(int timeoutMs) { return !ExitEv.Wait(timeoutMs); }

    void Terminate() { ExitEv.Fire(); }

    static void Sleep(int ms);

private:
    struct Priv;
    Priv* P = nullptr;

    ThreadEvent ExitEv = ThreadEvent(false);
};

// -------------------------------------------------------------------------------

template <typename T, int SIZE> class Queue
{
public:

    bool Enqueue(const T &value)
    {
        ScopeLock lock(Lock);
        if (IsFull()) return false;
        Buffer[(Write++) % SIZE] = value;
        return true;
    }

    bool Dequeue(T &value)
    {
        ScopeLock lock(Lock);
        if (IsEmpty()) return false;
        value = Buffer[(Read++) % SIZE];
        if (Write >= SIZE && Read >= SIZE)
        {
            Write -= SIZE;
            Read -= SIZE;
        }
        return true;
    }

    bool Peek(T& value)
    {
        ScopeLock lock(Lock);
        if (IsEmpty()) return false;
        value = Buffer[Read % SIZE];
        return true;
    }

    int  Count() { ScopeLock lock(Lock);  return (int)(Write - Read); }
    bool IsEmpty() { ScopeLock lock(Lock); return Write == Read; }
    bool IsFull() { ScopeLock lock(Lock); return Write - Read == SIZE; }

private:
    ThreadLock Lock;
    uint Read = 0;
    uint Write = 0;
    T Buffer[SIZE];
};

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------

bool IsFullscreen();
void SetScrollLock(bool on);

