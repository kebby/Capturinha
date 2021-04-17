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

enum OpenFileMode
{
    Read,
    Create,
    Append,
    RandomAccess,
};

Stream* OpenFile(const char* path, OpenFileMode mode = OpenFileMode::Read);
RCPtr<Buffer> LoadFile(const char* path);

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


class Random
{
    // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
    // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
    uint64 state;
    uint64 inc = 0x7335193deadbeef;

    uint pcg32_random_r()
    {
        uint64 oldstate = state;
        // Advance internal state
        state = oldstate * 6364136223846793005ULL + (inc | 1);
        // Calculate output function (XSH RR), uses old state for max ILP
        uint xorshifted = (uint)(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint rot = oldstate >> 59u;
        return (xorshifted >> rot) | (xorshifted << ((32-rot) & 31));
    }

public:

    Random() { Seed(GetTicks()); }
    Random(uint64 seed) { Seed(seed); }
    void Seed(uint64 s) { state = s; }

    uint Uint(int max) { return pcg32_random_r() % max; }
    int Int(int min, int max) { return pcg32_random_r() % (max - min) + min; }
    float Float(float min, float max) {
        return pcg32_random_r() * (max - min) / 4294967296.0f + min;
    }
    float Float(float max) { return Float(0, max); }
    float Float() { return Float(0, 1); }
};

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
