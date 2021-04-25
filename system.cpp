
// // Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers

// Windows Header Files
#include <windows.h>
#include <winuser.h>
#include <shellapi.h>
#include <mfapi.h>
#include <ShellScalingApi.h>
#include <shlwapi.h>

//#include "Resource.h"
#include "system.h"

#include <stdio.h>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "shlwapi.lib")



// Global Variables:
HINSTANCE hInst = 0;
HWND hWnd = 0;

float UIScale;


//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------

const char* ErrorString(DWORD id)

{
    thread_local static char buf[1024];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, sizeof(buf), NULL);
    return buf;
}

const char* LastErrorString() { return ErrorString(GetLastError()); }
//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------

uint AtomicInc(uint& a) { return InterlockedIncrement(&a); }
uint AtomicDec(uint& a) { return InterlockedDecrement(&a); }

//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------

static int64 perfFreq = 0, lastTicks = 0, curTicks = 0;
static double invPerfFreq = 0;

int64 GetTicks()
{
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    return pc.QuadPart;
}

double GetTime()
{
    if (!perfFreq)
    {
        LARGE_INTEGER pf;
        QueryPerformanceFrequency(&pf);
        perfFreq = pf.QuadPart;
        invPerfFreq = 1.0 / (double)perfFreq;
    }

    int64 ticks = GetTicks();
    if (!lastTicks) lastTicks = ticks;
    int64 delta = ticks - lastTicks;
    lastTicks = ticks;
    curTicks += delta;

    return (double)curTicks * invPerfFreq;
}

SystemTime GetSystemTime()
{
    SYSTEMTIME st = {};
    GetSystemTime(&st);

    return SystemTime 
    {
        .year = st.wYear,
        .month = st.wMonth,
        .dayOfWeek = st.wDayOfWeek,
        .day = st.wDay,
        .hour = st.wHour,
        .minute = st.wMinute,
        .second = st.wSecond,
        .milliseconds = st.wMilliseconds,
    };
}

// buffers
// -------------------------------------------------------------------------------

struct MemBuffer : Buffer
{
    MemBuffer(uint8 *p, uint64 l, bool o) : Buffer(p, l), own(o) {}
    ~MemBuffer() override { if (own) delete ptr; }
    bool own;
};

RCPtr<Buffer> Buffer::New(uint64 size)
{
    return RCPtr<Buffer>(new MemBuffer(new uint8[size], size, true));
}

RCPtr<Buffer> Buffer::FromMemory(void* ptr, uint64 size, bool transferOwnership)
{
    return RCPtr<Buffer>(new MemBuffer((uint8*)ptr, size, transferOwnership));
}


RCPtr<Buffer> Buffer::Part(const RCPtr<Buffer> buffer, uint64 offset, uint64 size)
{
    struct BufferPart : Buffer
    {
        BufferPart(const RCPtr<Buffer>& buffer, uint64 o, uint64 s) : Buffer(buffer->ptr + o, s), ref(buffer) {}
        RCPtr<Buffer> ref;
    };

    return RCPtr<Buffer>(new BufferPart(buffer, offset, size));
}

// debug output
// -------------------------------------------------------------------------------

static Stream* LogFile = nullptr;
static ThreadLock LogLock;

static constexpr int DbgSize = 4096;
static thread_local char DbgBuffer[DbgSize];

void DbgOpenLog(const char* filename)
{
    LogFile = OpenFile(filename, OpenFileMode::Create);
}

void DbgCloseLog()
{
    Delete(LogFile);
}

static void Dbg(const char* message)
{
    ScopeLock lock(LogLock);
    OutputDebugString(message);
    if (LogFile)
    {
        LogFile->Write(message, strlen(message));
    }
}

#define PRINTF_INTERNAL() { \
    va_list args; \
    va_start(args, format); \
    int len = vsnprintf_s(DbgBuffer, DbgSize, format, args); \
    if (len < 0) len = 0; \
    va_end(args); \
    DbgBuffer[len] = 0; \
}

#ifdef _DEBUG
void DPrintF(const char* format, ...)
{
    PRINTF_INTERNAL();
    Dbg(DbgBuffer);
}
#endif

[[noreturn]]
void Fatal(const char* format, ...)
{
    PRINTF_INTERNAL();
    Dbg("\n");
    Dbg(DbgBuffer);
    Dbg("\n");
    DbgCloseLog();

    MessageBox(hWnd, DbgBuffer, "Train Engine", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

[[noreturn]]
void OnAssert(const char* file, int line, const char* expr)
{
    Fatal("%s(%d): Assertion failed: %s\n", file, line, expr);
}

// streams
// -------------------------------------------------------------------------------

struct BufferStream : Stream
{
    RCPtr<Buffer> buffer;
    uint64 pos = 0;

    explicit BufferStream(const RCPtr<Buffer>& b) : buffer(b) {}

    uint64 Read(void* ptr, uint64 len) override
    {
        len = Min(len, buffer->size - pos);
        memcpy(ptr, buffer->ptr + pos, len);
        pos += len;
        return len;
    };

    uint64 Write(const void* ptr, uint64 len) override
    {
        len = Min(len, buffer->size - pos);
        memcpy(buffer->ptr + pos, ptr, len);
        pos += len;
        return len;
    };

    bool CanSeek() const override { return true; }
    bool CanRead() const override { return true; }
    bool CanWrite() const override { return true; }
    uint64 Length() const override { return buffer->size; }

    uint64 Seek(int64 p, From from) override
    {
        switch (from)
        {
        case From::Current: p += pos; break;
        case From::End: p += buffer->size; break;
        }
        return pos = (uint64)Clamp<int64>(p, 0ll, buffer->size);
    }

    RCPtr<Buffer> Map() override { return buffer; }
};


struct FileStream : Stream
{
    HANDLE hf;
    uint64 size;
    bool canRead;
    bool canWrite;

    explicit FileStream(HANDLE h, bool cr, bool cw) : hf(h), canRead(cr), canWrite(cw)
    {
        LARGE_INTEGER lis;
        GetFileSizeEx(hf, &lis);
        size = lis.QuadPart;
    }

    ~FileStream() override
    {
        CloseHandle(hf);
    };

    uint64 Read(void* ptr, uint64 len) override
    {
        len = Min(len, 0xffffffffull);
        DWORD read = 0;
        ReadFile(hf, ptr, (DWORD)len, &read, nullptr);
        return read;
    };

    uint64 Write(const void* ptr, uint64 len) override
    {
        len = Min(len, 0xffffffffull);
        DWORD written = 0;
        WriteFile(hf, ptr, (DWORD)len, &written, nullptr);
        return written;
    };

    bool CanRead() const override { return canRead; }
    bool CanWrite() const override { return canWrite; }
    bool CanSeek() const override { return true; }
    uint64 Length() const override { return size; }

    uint64 Seek (int64 pos, From from) override
    { 
        LARGE_INTEGER lis = {}, lip = {};
        lis.QuadPart = pos;        
        SetFilePointerEx(hf, lis, &lip, (DWORD)from);
        return lip.QuadPart;
    }

    RCPtr<Buffer> Map() override
    {
        // TODO: proper memory mapping
        auto buf = Buffer::New(size);
        /*uint64 read = */ Read(buf->ptr, size);
        return buf;
    }
};

bool FileExists(const char* path)
{
    return !!PathFileExists(path);
}

Stream *OpenFile(const char* path, OpenFileMode mode)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    bool cr, cw;
    switch (mode)
    {
    case OpenFileMode::Read:
        h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        cr = true; cw = false;
        break;
    case OpenFileMode::Append:
        h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE)
            SetFilePointer(h, 0, 0, FILE_END);
        cw = true; cr = false;
        break;
    case OpenFileMode::Create:
        h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
        cw = true; cr = false;
        break;
    case OpenFileMode::RandomAccess:
        h = CreateFile(path, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_ALWAYS, 0, NULL);
        cw = true; cr = true;
        break;
    }
    if (h == INVALID_HANDLE_VALUE)
    {
        Fatal("could not open %s: %s\n", path, LastErrorString());
    }
    DPrintF("Opening %s\n", path);
    return new FileStream(h, cr, cw);
}


RCPtr<Buffer> LoadFile(const char* path)
{
    RCPtr<Buffer> buffer;
    Stream* s = OpenFile(path);
    if (s)
    {
        buffer = s->Map();
        delete s;
    }
    return buffer;
}

String ReadFileUTF8(const char* path)
{
    Stream* str = OpenFile(path);
    ASSERT(str);

    size_t len = str->Length();
    String ret;
    char *ptr = ret.Make((int)len);

    size_t offs = 0;
    size_t read;
    while (offs < len && (read = str->Read(ptr + offs, len - offs)))
        offs += read;

    ptr[offs] = 0;
    delete str;
    return ret;
}

void WriteFileUTF8(const String& text, const char* path)
{
    Stream* str = OpenFile(path, OpenFileMode::Create);
    ASSERT(str);

    size_t len = text.Length();
    size_t offs = 0;
    size_t written;
    const char* ptr = text;
    while (offs < len && (written = str->Write(ptr + offs, len - offs)))
        offs += written;

    delete str;
}

RCPtr<Buffer> LoadResource(int name, int type)
{
    HMODULE handle = ::GetModuleHandle(NULL);
    HRSRC rc = ::FindResource(handle, MAKEINTRESOURCE(name), MAKEINTRESOURCE(type));
    HGLOBAL rcData = ::LoadResource(handle, rc);
    DWORD size = ::SizeofResource(handle, rc);
    void *data = ::LockResource(rcData);
    return Buffer::FromMemory(data, size, false);
}

//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------

ThreadLock::ThreadLock()
{
    P = new CRITICAL_SECTION;
    InitializeCriticalSection((LPCRITICAL_SECTION)P);
}

ThreadLock::~ThreadLock()
{
    DeleteCriticalSection((LPCRITICAL_SECTION)P);
    delete P;
}

void ThreadLock::Lock()
{
    EnterCriticalSection((LPCRITICAL_SECTION)P);
}

void ThreadLock::Unlock()
{
    LeaveCriticalSection((LPCRITICAL_SECTION)P);
}

//----------------------------------------------------------------------------------------------

ThreadEvent::ThreadEvent(bool autoReset)
{
    P = CreateEvent(NULL, !autoReset, FALSE, NULL);
}

ThreadEvent::~ThreadEvent()
{
    CloseHandle(P);
}

void ThreadEvent::Fire()
{
    SetEvent(P);
}

void ThreadEvent::Reset()
{
    ResetEvent(P);
}

void ThreadEvent::Wait()
{
    WaitForSingleObject(P, INFINITE);
}

bool ThreadEvent::Wait(int timeoutMs)
{
    return !WaitForSingleObject(P, timeoutMs);
}

void* ThreadEvent::GetRawEvent() const
{
    return P;
}

//----------------------------------------------------------------------------------------------

struct Thread::Priv
{
    Priv() {}

    Func<void(Thread&)> Func;
    HANDLE Handle;
    DWORD ThreadID;

    static DWORD WINAPI Proxy(void *t)
    {
        auto thread = (Thread*)t;
        thread->P->Func(*thread);
        return 0;
    }
};

Thread::Thread(Func<void(Thread&)> threadFunc)
{   
    P = new Priv;
    P->Func = threadFunc;
    P->Handle = CreateThread(NULL, 0, Priv::Proxy, this, 0, &P->ThreadID);
}

Thread::~Thread()
{
    Terminate();
    WaitForSingleObject(P->Handle, INFINITE);
    CloseHandle(P->Handle);
    delete P;    
}

void Thread::Sleep(int ms)
{
    ::Sleep(ms);
}

//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------

bool IsFullscreen() 
{
    QUERY_USER_NOTIFICATION_STATE state;
    SHQueryUserNotificationState(&state);
    return (state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN);
}

void SetScrollLock(bool on)
{
    BYTE keyState[256];

    GetKeyboardState((LPBYTE)&keyState);

    if ((on && !(keyState[VK_SCROLL] & 1)) ||
        (!on && (keyState[VK_SCROLL] & 1)))
    {
        // Simulate a key press
        keybd_event(VK_SCROLL,
            0x45,
            KEYEVENTF_EXTENDEDKEY | 0,
            0);

        // Simulate a key release
        keybd_event(VK_SCROLL,
            0x45,
            KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP,
            0);
    }
}
//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------


/*
ScreenMode screenMode;


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hwnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CHAR:
        // quit on esc
        if (wParam == 27)
        {
            DestroyWindow(hWnd);
            break;
        }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CoInitialize(NULL);

    screenMode = { 1920, 1080, false };
    TrainSetMode(screenMode);
    
    MFStartup(MF_VERSION, MFSTARTUP_LITE);


    // Initialize global strings
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAINENGINE));
    wcex.hCursor = 0;// LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = "kbTrainEngine";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

    // Perform application initialization:
    hInst = hInstance; // Store instance handle in our global variable
    if (screenMode.fullscreen)
    {
        const POINT ptZero = { 0, 0 };
        HMONITOR mon = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        if (!GetMonitorInfo(mon, &mi)) return 1;

        hWnd = CreateWindow(wcex.lpszClassName, L"Träin Ängine", WS_POPUP|WS_VISIBLE,
            mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top, nullptr, nullptr, hInstance, nullptr);

        //ShowCursor(FALSE);
    }
    else
    {
        uint dpi = GetDpiForSystem();
        screenMode.width = screenMode.width * dpi / 96;
        screenMode.height = screenMode.height * dpi / 96;

        RECT wr = { 0, 0, (LONG)screenMode.width, (LONG)screenMode.height };    // set the size, but not the position
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);    // adjust the size

        hWnd = CreateWindow(wcex.lpszClassName, "Träin Ängine", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, wr.right-wr.left, wr.bottom-wr.top, nullptr, nullptr, hInstance, nullptr);
    }

    if (!hWnd)
        Fatal("Could not create window");

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    TrainInit();

    // Main message loop:
    MSG msg = {};
    bool done = false;
    while (!done)
    {
      
        TrainRender();

        while (!done && PeekMessage(&msg, NULL, 0, 0, 0))
        {
            if (GetMessage(&msg, nullptr, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
                done = true;
        }
    }

    TrainExit();

    ImGui_ImplWin32_Shutdown();

    MFShutdown();

    return (int)msg.wParam;
}
*/

