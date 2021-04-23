#pragma once

#include <math.h>
#include <new.h>

// basic types
// -------------------------------------------------------------------------------

typedef unsigned int uint;
typedef long long int64;
typedef unsigned long long uint64;
typedef short int16;
typedef unsigned short uint16;
typedef signed char int8;
typedef unsigned char uint8;

// debug assertions
// -------------------------------------------------------------------------------

[[noreturn]]
void OnAssert(const char* file, int line, const char* expr);

#define ASSERT(x) { if (!(x)) OnAssert(__FILE__, __LINE__, #x); }

#define ASSERT0(s) { OnAssert(__FILE__, __LINE__, s); }

// basic math
// -------------------------------------------------------------------------------

template<typename T> constexpr T Min(T a, T b) { return a < b ? a : b; }
template<typename T> constexpr T Max(T a, T b) { return a > b ? a : b; }
template<typename T> constexpr T Clamp(T v, T min, T max) { return Min(Max(v, min), max); }
template<typename T> constexpr T Lerp(T v, T a, T b) { return a * (1 - v) + b * v; }

template<typename T> constexpr T Smooth(T x) { return (3 * x - 2) * x * x; }
template<typename T> constexpr T Smoothstep(T v, T min, T max) { return Smooth((v - min) / (max - min)); }


// basic helper stuff 
// -------------------------------------------------------------------------------

constexpr int NumArgs() { return 0; }
template<typename a1, typename ... args> constexpr int NumArgs(a1, args... a) { return NumArgs(a...) + 1; }

template<typename T> void Delete(T*& ptr) { delete ptr; ptr = nullptr; }

// containers
// -------------------------------------------------------------------------------

template<typename T> class Array
{
private:
    T* mem = nullptr;
    size_t size = 0;
    size_t capacity = 0;

    void Construct(size_t at) { new(&mem[at]) T(); }
    void Construct(size_t at, const T& value) { new(&mem[at]) T(value); }
    void Construct(size_t at, T&& value) { new(&mem[at]) T(value); }
    template<class Tv> void Construct(size_t at, Tv value) { new(&mem[at]) T(value); }
    template<class Tv, class ... args> void Construct(size_t at, Tv v, args ...a) { Construct(at, v); Construct(at + 1, a...); }
    void Destruct(size_t i) { mem[i].T::~T(); }
    
    void Grow(size_t to)
    {
        if (to <= capacity) return;
        T* oldptr = mem;
        capacity = Max(2*capacity, to);
        if (!capacity) capacity = 1;
        mem = (T*)new uint8[capacity * sizeof(T)];
        for (size_t i = 0; i < size; i++)
        {
            T& elem = ((T*)oldptr)[i];
            Construct(i, (T&&)elem);
            elem.T::~T();
        }
        delete[] (uint8*)oldptr;
    }
    
    void PrepareInsert(size_t at, size_t count)
    {
        ASSERT(at <= size);
        Grow(at + count);
        for (size_t i = size-at; i --> 0;)
        {
            if (at + i + count >= size)
                Construct(at + i + count, (T&&)mem[at + i]);
            else
                mem[at + i + count] = (T&&)mem[at + i];
            Destruct(at + i);
        }
        size += count;
    }

public:
    Array() {}
    explicit Array(size_t cap) { Grow(cap); }
    template<class ... args> Array(args ...a) { PushTail(a...); }

    Array(const Array& a)
    {
        Grow(a.capacity);
        for (size_t i = 0; i < a.size; i++)
            PushTail(a[i]);
    }

    Array(Array&& a) : mem(a.mem),size(a.size), capacity(a.capacity)
    {
        a.size = a.capacity = 0;
        a.mem = nullptr;            
    }
    ~Array() { Clear(); delete (uint8*)mem; }

    Array& operator = (const Array &arr)
    {
        Clear();
        Grow(arr.capacity);
        for (size_t i = 0; i < arr.size; i++)
            PushTail(arr[i]);
        return *this;
    }

    
    void Clear() { SetSize(0); }

    void SetSize(size_t s)
    {
        Grow(s);
        while (size < s)
            Construct(size++);
        while (size > s)
            Destruct(--size);
    }
  
    template<class ... args> void Insert(size_t at, args ...a) { PrepareInsert(at, NumArgs(a...)); Construct(at, a...); }
    template<class ... args> void PushHead(args ...a) { Insert(0, a...); }
    template<class ... args> void PushTail(args ...a) { Insert(size, a...); }
    template<class Ta> Array& operator += (Ta arg) { Insert(size, arg); return *this; }

    T RemAtUnordered(size_t index)
    {
        T ret = Get(index);
        size--;
        if (index<size)
            mem[index] = (T&&)mem[size];
        Destruct(size);
        return ret;
    }

    T RemAt(size_t index)
    {
        T ret = Get(index);
        size--;
        while (index < size)
        {
            mem[index] = (T&&)mem[index + 1];
            index++;
        }
        Destruct(size);
        return ret;
    }

    T PopHead() { return RemAt(0); }
    T PopTail() { return RemAt(size-1); }

    template<class TPred> void RemIf(TPred pred)
    {
        int di = 0;
        for (int i = 0; i < size; i++)
            if (!pred(mem[i]))
            {
                if (di != i)
                    mem[di] = (T&&)mem[i];
                di++;
            }
        for (int i = di; i < size; i++)
            Destruct(i);
        size = di;
    }

    template<class TPred> void RemIfUnordered(TPred pred)
    {
        for (int i = 0; i < size; )
            if (pred(mem[i]))
            {
                size--;
                if (i != size)
                    mem[i] = (T&&)mem[size];
                Destruct(size);
            }
            else
                i++;
    }

    void Rem(const T& v) { RemIf([&](const T& x) { return x == v; }); }
    void RemUnordered(const T& v) { RemIfUnordered([&](const T& x) { return x == v; }); }

    const T& Get(size_t i) const { ASSERT(i < size); return ((T*)mem)[i]; }
    T& Get(size_t i) { ASSERT(i < size); return ((T*)mem)[i]; }

    T& operator[](size_t i) { return Get(i); }
    const T& operator[](size_t i) const { return Get(i); }
    size_t Count() const { return size; }
    bool operator ! () const { return !size; }

    bool operator == (const Array& arr) const
    {
        if (size != arr.size) return false;
        for (int i = 0; i < size; i++)
            if (mem[i] != arr.mem[i])
                return false;
        return true;
    }

    bool operator != (const Array& arr) const { return !(*this == arr); }
};

// for..in support
template<typename T> T* begin(Array<T> &arr) { return arr.Count() ? &arr[0] : nullptr; }
template<typename T> T* end(Array<T> &arr) { return arr.Count() ? (&arr[0])+arr.Count() : nullptr; }
template<typename T> T* begin(const Array<T>& arr) { return arr.Count() ? &arr[0] : nullptr; }
template<typename T> T* end(const Array<T>& arr) { return arr.Count() ? (&arr[0]) + arr.Count() : nullptr; }

template<typename TP> void DeleteAll(Array<TP*>& array) { for (TP* p : array) delete p; array.Clear(); }

// Atomics
//----------------------------------------------------------------------------------------------

uint AtomicInc(uint& x);
uint AtomicDec(uint& x);

// COM and reference counting
//----------------------------------------------------------------------------------------------

// base class for COM-Style ref counting
class RCObj
{
    uint RC = 1;
public:
    virtual ~RCObj() {}
    inline void AddRef() { AtomicInc(RC); }
    inline void Release() { if (!AtomicDec(RC)) delete this; }
};

// smart pointer for COM-style ref counted stuff. Works for COM objects and RCObj.
template <typename T> class RCPtr
{
    T* ptr = nullptr;
public:
    constexpr RCPtr() : ptr(nullptr) {}
    RCPtr(const RCPtr& p) : ptr(p.ptr) { if (ptr) ptr->AddRef(); }
    constexpr RCPtr(RCPtr&& p) : ptr(p.ptr) { p.ptr = nullptr; }
    constexpr RCPtr(T* p) { ptr = p; }

    // this works with COM or if your class implements it manually
    template <typename T2> RCPtr(const RCPtr<T2>& pp) : ptr(nullptr)
    {
        if (pp.IsValid()) pp->QueryInterface<T>(&ptr);
    }

    ~RCPtr() { Clear(); }

    RCPtr& operator = (const RCPtr& p) { Clear(); ptr = p.ptr; if (ptr) ptr->AddRef(); return *this; }
    RCPtr& operator = (RCPtr&& p) { Clear(); ptr = p.ptr; p.ptr = nullptr; return *this; }
    template <typename T2> RCPtr & operator =  (const RCPtr<T2>& pp)
    {
        Clear();
        if (pp.IsValid()) pp->QueryInterface<T>(&ptr);
        return *this;
    }
     
    void Clear() { if (ptr) { ptr->Release(); ptr = 0; } }
    constexpr bool IsValid() const { return ptr != nullptr; }
    constexpr bool operator !() const { return !IsValid();  }

    T* operator -> () const { ASSERT(ptr); return ptr; }
    T& Ref() const { ASSERT(ptr); return *ptr; }

    // casts
    constexpr operator T* () const { return ptr; }
    constexpr operator void** () { Clear(); return (void**)&ptr; }
    constexpr operator T** () { Clear(); return &ptr; }
};

// Functions
//----------------------------------------------------------------------------------------------


// Simple class that binds an object instance and a member ptr into a functor class 
// that you can call. Don't use directly, just call Bind()
template<typename ObjT, typename RetT, typename ... ArgT> class Binding
{
public:
    constexpr Binding(ObjT& obj, RetT(ObjT::* ptr)(ArgT...)) : Obj(obj), Ptr(ptr) { }
    constexpr Binding(const Binding& b) : Obj(b.Obj), Ptr(b.Ptr) { }
    Binding& operator=(const Binding& b) { Obj = b.Obj; Ptr = b.Ptr; return *this; }

    constexpr RetT operator()(ArgT... args) const { return (Obj.*Ptr)(args...); }

private:
    typedef RetT(ObjT::* PtrT)(ArgT...);
    ObjT& Obj;
    PtrT Ptr;
};

// Bind an object instance and a member function pointer into a functor object
template<typename ObjT, typename RetT, typename ... ArgT>
constexpr Binding<ObjT, RetT, ArgT...> Bind(ObjT& obj, RetT(ObjT::* ptr)(ArgT...))
{
    return Binding<ObjT, RetT, ArgT...>(obj, ptr);
}

// Bind an object instance and a member function pointer into a functor object
template<typename ObjT, typename RetT, typename ... ArgT>
constexpr Binding<ObjT, RetT, ArgT...> Bind(ObjT* obj, RetT(ObjT::* ptr)(ArgT...))
{
    return Binding<ObjT, RetT, ArgT...>(*obj, ptr);
}

// Proper function pointer that takes everything that you can call with ()
// (replaces std::function)
template<typename> class Func {};
template<typename RetT, typename ...ArgT> class Func<RetT(ArgT...)>
{
    struct ICallable : RCObj
    {
        virtual ~ICallable() {}
        virtual RetT operator()(ArgT ...) = 0;
    };

    // type erasure: An object that contains the thing to call and exports an interface to call it
    template <typename F> struct CallProxy : ICallable
    {
        F Obj;
        CallProxy(F obj) : Obj(obj) {}
        RetT operator()(ArgT... args) override { return Obj(args...); }
    };    

    RCPtr<ICallable> ptr;

public:
    constexpr Func() {};

    // constructors from other Funcs or callable stuff
    constexpr Func(const Func& f) : ptr(f.ptr) {}
    constexpr Func(Func &&f) : ptr(static_cast<RCPtr<ICallable>&&>(f.ptr)) {}
    template<typename F> Func(F func) : ptr(new CallProxy<F>(func)) {}

    // the same but as assignment
    Func& operator = (const Func& f) { ptr = f.ptr; return *this; }
    Func& operator = (Func&& f) { ptr = static_cast<RCPtr<ICallable>&&>(f.ptr); return *this; }
    template<typename F> Func& operator = (F func) { ptr = new CallProxy<F>(func); return *this; }

    // call
    RetT operator() (ArgT... args) const { return ptr.Ref()(args...);  }

    // helper stuff
    constexpr bool IsValid() const { return ptr.IsValid(); }
    constexpr bool operator!() const { return !IsValid(); }
    void Clear() { ptr.Clear(); }
};

// Buffers
// -------------------------------------------------------------------------------

class Buffer : public RCObj
{
public:
    uint8* const ptr;
    const uint64 size;

    virtual ~Buffer() {};

    static RCPtr<Buffer> New(uint64 size);
    static RCPtr<Buffer> FromMemory(void* ptr, uint64 size, bool transferOwnership);
    static RCPtr<Buffer> Part(const RCPtr<Buffer> buffer, uint64 offset, uint64 size);

protected:
    Buffer(uint8* p, uint64 s) : ptr(p), size(s) { }
};

// Strings
// -------------------------------------------------------------------------------

// very basic and very not optimized quasi-immutable string class
// I just want a better interface than C style
// let's totally rewrite this as soon as it becomes slow enough
class String
{
public:
    String() {};
    String(const char* p, int len=-1) { MakeNode(p, len); }
    String(const wchar_t* p, int len = -1) { MakeNode(p, len); }
    String(const String& s) { node = s.node; }
    String(String&& s) { node = static_cast<RCPtr<Node>&&>(s.node); }

    static String PrintF(const char* format, ...);
    static String Concat(const String& s1, const String& s2);

    String& operator = (const char* p) { MakeNode(p); return *this; }
    String& operator = (const wchar_t* p) { MakeNode(p); return *this; }
    String& operator = (const String& s) { node = s.node; return *this; }
    String& operator = (String&& s) { node = static_cast<RCPtr<Node>&&>(s.node); return *this; }

    size_t Length() const { return node.IsValid() ? node->len : 0; }
    operator const char* () const { return node.IsValid() ? node->str : ""; }

    static int Compare(const String& a, const String &b, bool ignoreCase = false);
    static int Compare(const String& a, const char *b, bool ignoreCase = false);
    template<typename Ts> int Compare(const Ts& s, bool ignoreCase = false) const { return Compare(*this, s, ignoreCase); }

    bool operator! () const { return !node; }
    String operator + (const String& s) const { return Concat(*this, s); }

    template<typename Ts> bool operator < (const Ts& s) const { return Compare(s) < 0; }
    template<typename Ts> bool operator <= (const Ts& s) const { return Compare(s) <= 0; }
    template<typename Ts> bool operator == (const Ts& s) const { return Compare(s) == 0; }
    template<typename Ts> bool operator >= (const Ts& s) const { return Compare(s) >= 0; }
    template<typename Ts> bool operator > (const Ts& s) const { return Compare(s) > 0; }
    template<typename Ts> bool operator != (const Ts& s) const { return Compare(s) != 0; }

    class WCharProxy
    {
        friend class String;
        WCharProxy() {}
        WCharProxy(const WCharProxy& p) = delete;
        wchar_t* ptr = nullptr;
    public:
        ~WCharProxy() { delete[] ptr; }
        WCharProxy(WCharProxy&& p) { ptr = p.ptr; p.ptr = nullptr; }
        operator const wchar_t* () const { return ptr ? ptr : L""; }
    } ToWChar() const;

private:

    struct Node : RCObj { size_t len = 0; char str[1] = {}; }; // variable size!
    RCPtr<Node> node;

    void MakeNode(const char* p, int len=-1);
    void MakeNode(const wchar_t* p, int len = -1);
};
