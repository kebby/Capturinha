//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

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
template<typename T> constexpr T Lerp(float v, T a, T b) { return a * (1 - v) + b * v; }
template<typename T> constexpr T Lerp(double v, T a, T b) { return a * (1 - v) + b * v; }

template<typename T> constexpr T Smooth(T x) { return (3 * x - 2) * x * x; }
constexpr float Smoothstep(float v, float min, float max) { return Smooth(Clamp((v - min) / (max - min), 0.f, 1.f)); }
constexpr double Smoothstep(double v, double min, double max) { return Smooth(Clamp((v - min) / (max - min), 0., 1.)); }


// basic helper stuff 
// -------------------------------------------------------------------------------

template<typename T> void Delete(T*& ptr) { delete ptr; ptr = nullptr; }

// containers
// -------------------------------------------------------------------------------

template<typename T> class Span
{
protected:
    T* mem;
    size_t size;

public:
    constexpr Span() : mem(nullptr), size(0) {}
    constexpr Span(const Span& sp) : mem(sp.mem), size(sp.size) {}
    constexpr Span(T* ptr, size_t len) : mem(ptr), size(len) {}
    constexpr Span(T* begin, T* end) : mem(begin), size(end - begin) {}
    constexpr Span(T& ref) : mem(&ref), size(1) {}

    template<size_t len> constexpr Span(T(&arr)[len]) : mem(arr), size(len) {}

    constexpr Span operator=(const Span& sp) { mem = sp.mem; size = sp.size; return *this; }
    template<size_t len> Span operator=(T(&arr)[len]) { mem = arr; size = len; }

    constexpr T& Get(size_t i) const { ASSERT(i < size); return ((T*)mem)[i]; }
    constexpr T& operator[](size_t i) const { return Get(i); }

    constexpr size_t Len() const { return size; }
    constexpr bool operator ! () const { return !size; }
    constexpr T* Ptr() const { return mem; }

    template<typename Tto> constexpr Span<Tto> Cast() const
    {
        size_t bsize = size * sizeof(T);
        ASSERT(bsize % sizeof(Tto) == 0);
        return Span<Tto>((Tto*)mem, bsize / sizeof(Tto));
    }

    constexpr const Span Slice(size_t start) const {
        ASSERT(start <= size);
        return Span(mem + start, size - start);
    }

    constexpr const Span Slice(size_t start, size_t len) const {
        ASSERT(start + len <= size);
        return Span(mem + start, len);
    }

    constexpr bool operator == (const Span& span) const
    {
        if (size != span.size) return false;
        for (int i = 0; i < size; i++)
            if (mem[i] != span.mem[i])
                return false;
        return true;
    }

    constexpr bool operator != (const Span& span) const { return !(*this == span); }

    template<typename TPred> constexpr ptrdiff_t IndexOf(TPred pred, const size_t start = 0) const
    {
        for (size_t i = start; i < size; i++)
            if (pred(mem[i]))
                return i;
        return -1;
    }

    constexpr ptrdiff_t IndexOf(const T& value, const size_t start = 0) const
    {
        return IndexOf([&](const T& x) { return x == value; }, start);
    }

    constexpr void CopyTo(const Span& dest) const
    {
        ASSERT(dest.size >= size);
        if (dest.mem == mem)
            return;

        if (dest.mem < mem || dest.mem >= mem + size)
            for (size_t i = 0; i < size; i++)
                dest.mem[i] = mem[i];
        else
            for (size_t i = size; i-- > 0;)
                dest.mem[i] = mem[i];
    }

    constexpr void Fill(const T& value) const
    {
        for (size_t i = 0; i < size; i++)
            mem[i] = value;
    }

    constexpr void Clear() const { Fill({}); }
};

template<typename T> constexpr T* begin(const Span<T>& span) { return span.Len() ? span.Ptr() : nullptr; }
template<typename T> constexpr T* end(const Span<T>& span) { return span.Len() ? span.Ptr() + span.Len() : nullptr; }

template<typename T> T Read(Span<uint8>& span, bool advance = true)
{
    ASSERT(span.Len() >= sizeof(T));
    T ret = *(T*)span.Ptr();
    if (advance) span = span.Slice(sizeof(T));
    return ret;
}

template<typename T> void Write(Span<uint8>& span, const T& value, bool advance = true)
{
    ASSERT(span.Len() >= sizeof(T));
    *(T*)span.Ptr() = value;
    if (advance) span = span.Slice(sizeof(T));
}


template<class T> class ReadOnlySpan : public Span<const T>
{
public:
    constexpr ReadOnlySpan() : Span<const T>() {}
    constexpr ReadOnlySpan(const Span<const T>& sp) : Span<const T>(sp) {}
    constexpr ReadOnlySpan(const Span<T>& sp) : Span<const T>(sp.Ptr(), sp.Len()) {}
    constexpr ReadOnlySpan(const T* ptr, size_t len) : Span<const T>(ptr, len) {}
    constexpr ReadOnlySpan(const T* begin, const T* end) : Span<const T>(begin, end) {}
    constexpr ReadOnlySpan(T& ref) : Span<const T>(ref) {}

    template<size_t len> constexpr ReadOnlySpan(const T(&arr)[len]) : Span<const T>(arr, len) {};

    constexpr ReadOnlySpan operator=(const Span<T>& sp) { Span<const T>::mem = sp.Ptr(); Span<const T>::size = sp.Len(); return *this; }

    template<typename Tto> constexpr ReadOnlySpan<Tto> Cast() const
    {
        size_t bsize = Span<const T>::size * sizeof(T);
        ASSERT(bsize % sizeof(Tto) == 0);
        return ReadOnlySpan<Tto>((const Tto*)Span<const T>::mem, bsize / sizeof(Tto));
    }
};


template<typename T> T Read(ReadOnlySpan<uint8>& span, bool advance = true)
{
    ASSERT(span.Len() >= sizeof(T));
    T ret = *(T*)span.Ptr();
    if (advance) span = span.Slice(sizeof(T));
    return ret;
}

template<typename T, typename TA> class ArrayBase : public Span<T>
{
protected:
    ArrayBase(T* m = nullptr, size_t s = 0) : Span<T>(m, s) {}

    void Construct(size_t at) { new(&this->mem[at]) T(); }
    template<typename Tv> void Construct(size_t at, Tv value) { new(&this->mem[at]) T(value); }
    template<typename Tv, typename ... args> void Construct(size_t at, Tv v, args ...a) { Construct(at, v); Construct(at + 1, a...); }
    void Construct(size_t at, const Span<T>& span) { for (const T& v : span) Construct(at++, v); }
    void Construct(size_t at, const ReadOnlySpan<T>& span) { for (const T& v : span) Construct(at++, v); }
    void Destruct(size_t at) { this->mem[at].T::~T(); }

    void PrepareInsert(size_t at, size_t count)
    {
        ASSERT(at <= this->size);
        ((TA*)this)->Grow(at + count);
        for (size_t i = this->size - at; i-- > 0;)
        {
            if (at + i + count >= this->size)
                Construct(at + i + count, (T&&)this->mem[at + i]);
            else
                this->mem[at + i + count] = (T&&)this->mem[at + i];
            Destruct(at + i);
        }
        this->size += count;
    }

public:
    ArrayBase& operator = (const ReadOnlySpan<T> &arr)
    {
        Clear();
        ((TA*)this)->Grow(arr.size);
        PushTail(arr);
        return *this;
    }

    ArrayBase& operator = (const Span<T> &arr)
    {
        Clear();
        ((TA*)this)->Grow(arr.size);
        PushTail(arr);
        return *this;
    }


    void Clear() 
    { 
        while (this->size) 
            Destruct(--this->size); 
    }

    void SetSize(size_t s)
    {
        ((TA*)this)->Grow(s);
        while (this->size < s)
            Construct(this->size++);
        while (this->size > s)
            Destruct(--this->size);
    }

    template<typename ... args> void Insert(size_t at, args ...a) { PrepareInsert(at, sizeof...(a)); Construct(at, a...); }
    void Insert(size_t at, const Span<T>&span) { PrepareInsert(at, span.Len()); Construct(at, span); }
    void Insert(size_t at, const ReadOnlySpan<T>& span) { PrepareInsert(at, span.Len()); Construct(at, span); }
    template<typename ... args> void PushHead(args ...a) { Insert(0, a...); }
    void PushHead(const Span<T>& span) { Insert(0, span); }
    void PushHead(const ReadOnlySpan<T>& span) { Insert(0, span); }
    template<typename ... args> void PushTail(args ...a) { Insert(this->size, a...); }
    void PushTail(const Span<T>& span) { Insert(this->size, span); }
    void PushTail(const ReadOnlySpan<T>& span) { Insert(this->size, span); }
    template<typename Ta> ArrayBase& operator += (Ta arg) { Insert(this->size, arg); return *this; }
    ArrayBase& operator += (const Span<T>& span) { Insert(this->size, span); return *this; }
    ArrayBase& operator += (const ReadOnlySpan<T>& span) { Insert(this->size, span); return *this; }

    T RemAtUnordered(size_t index)
    {
        T ret = this->Get(index);
        this->size--;
        if (index < this->size)
            this->mem[index] = (T&&)this->mem[this->size];
        Destruct(this->size);
        return ret;
    }

    T RemAt(size_t index)
    {
        T ret = this->Get(index);
        this->size--;
        while (index < this->size)
        {
            this->mem[index] = (T&&)this->mem[index + 1];
            index++;
        }
        Destruct(this->size);
        return ret;
    }

    T PopHead() { return RemAt(0); }
    T PopTail() { return RemAt(this->size - 1); }

    template<typename TPred> void RemIf(TPred pred)
    {
        size_t di = 0;
        for (int i = 0; i < this->size; i++)
            if (!pred(this->mem[i]))
            {
                if (di != i)
                    this->mem[di] = (T&&)this->mem[i];
                di++;
            }
        while (this->size > di)
            Destruct(--this->size);
    }

    template<typename TPred> void RemIfUnordered(TPred pred)
    {
        for (size_t i = 0; i < this->size; )
            if (pred(this->mem[i]))
            {
                this->size--;
                if (i != this->size)
                    this->mem[i] = (T&&)this->mem[this->size];
                Destruct(this->size);
            }
            else
                i++;
    }

    void Rem(const T& v) { RemIf([&](const T& x) { return x == v; }); }
    void RemUnordered(const T& v) { RemIfUnordered([&](const T& x) { return x == v; }); }
};

template<typename TP, typename Ta> void DeleteAll(ArrayBase<TP*, Ta>& array) { for (TP* p : array) delete p; array.Clear(); }

template<typename T> class Array : public ArrayBase<T, Array<T>>
{
    typedef ArrayBase<T, Array<T>> TBase;
    size_t capacity = 0;

public:
    Array() { }

    explicit Array(size_t Capacity) { Grow(Capacity); }
    template<typename ... args> explicit Array(args ...a) { this->PushTail(a...); }

    Array(const Array& a) { Grow(a.Len()); this->PushTail((Span<T>)a); }

    Array(Array&& a) : TBase(a.mem, a.size, a.capacity)
    {
        a.size = a.capacity = 0;
        a.mem = nullptr;
    }

    Array(const Span<T>& a) { Grow(a.Len()); this->PushTail(a); }
    Array(const ReadOnlySpan<T>& a) { Grow(a.Len()); this->PushTail(a); }

    ~Array() 
    { 
        this->Clear(); 
        delete[](uint8*)this->mem; 
    }

    void Grow(size_t to)
    {
        if (to <= this->capacity) return;
        T* oldptr = this->mem;
        this->capacity = Max(2 * this->capacity, to);
        if (!this->capacity) this->capacity = 1;
        ArrayBase<T, Array<T>>::mem = (T*)new uint8[this->capacity * sizeof(T)];
        for (size_t i = 0; i < this->size; i++)
        {
            T& elem = ((T*)oldptr)[i];
            this->Construct(i, (T&&)elem);
            elem.T::~T();
        }
        if (oldptr) delete[](uint8*)oldptr;
    }

    Array& operator= (const Array& arr)
    {
        this->Clear();
        Grow(arr.Len());
        this->PushTail((Span<T>)arr);
        return *this;
    }

    Array& operator= (Array&& arr)
    {
        this->Clear();
        this->mem = arr.mem;
        this->size = arr.size;
        this->capacity = arr.capacity;
        arr.mem = nullptr;
        arr.size = 0;
        arr.capacity = 0;
        return *this;
    }

};



template<typename T, int Capacity> class StaticArray : public ArrayBase<T, StaticArray<T, Capacity>>
{
    typedef ArrayBase<T, StaticArray<T, Capacity>> TBase;
    friend TBase;

    uint8 staticMem[Capacity * sizeof(T)];

    void Grow(size_t to) { ASSERT(to <= Capacity); }

public:

    enum { capacity = Capacity };

    StaticArray() : TBase((T*)staticMem, 0) {}
    template<typename ... args> StaticArray(args ...a) : TBase((T*)staticMem, 0) { this->PushTail(a...); }
    StaticArray(const StaticArray& a) : TBase((T*)staticMem, 0) { this->PushTail((Span<T>)a); }
    StaticArray(const Span<T>& a) : TBase((T*)staticMem, 0) { this->PushTail(a); }
    StaticArray(const ReadOnlySpan<T>& a) : TBase((T*)staticMem, 0) { this->PushTail(a); }

    ~StaticArray() { this->Clear(); }

    StaticArray operator=(const StaticArray& a)
    {
        this->Clear();
        this->PushTail((Span<T>)a);
    }


};


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
        if (pp.IsValid()) pp->QueryInterface(__uuidof(T), (void**)&ptr);
    }

    ~RCPtr() { Clear(); }

    RCPtr& operator = (const RCPtr& p) { Clear(); ptr = p.ptr; if (ptr) ptr->AddRef(); return *this; }
    RCPtr& operator = (RCPtr&& p) noexcept { Clear(); ptr = p.ptr; p.ptr = nullptr; return *this; }
    template <typename T2> RCPtr& operator =(const RCPtr<T2>& pp)
    {
        Clear();
        if (pp.IsValid()) pp->QueryInterface(__uuidof(T), (void**)&ptr);
        return *this;
    }

    void Clear() { if (ptr) { ptr->Release(); ptr = 0; } }
    constexpr bool IsValid() const { return ptr != nullptr; }
    constexpr bool operator !() const { return !IsValid(); }

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
    constexpr Func(Func&& f) : ptr(static_cast<RCPtr<ICallable>&&>(f.ptr)) {}
    template<typename F> Func(F func) : ptr(new CallProxy<F>(func)) {}

    // the same but as assignment
    Func& operator = (const Func& f) { ptr = f.ptr; return *this; }
    Func& operator = (Func&& f) { ptr = static_cast<RCPtr<ICallable>&&>(f.ptr); return *this; }
    template<typename F> Func& operator = (F func) { ptr = new CallProxy<F>(func); return *this; }

    // call
    RetT operator() (ArgT... args) const { return ptr.Ref()(args...); }

    // helper stuff
    constexpr bool IsValid() const { return ptr.IsValid(); }
    constexpr bool operator!() const { return !IsValid(); }
    void Clear() { ptr.Clear(); }
};

// Buffers
// -------------------------------------------------------------------------------

class Buffer : public RCObj, public Span<uint8>
{
    Buffer() = delete;
    Buffer(const Buffer&) = delete;
    Buffer& operator = (const Buffer&) = delete;

public:
    ~Buffer() { delete[] mem; };
    Buffer(Buffer&& b) noexcept : Span<uint8>(b.mem, b.size) { b.mem = nullptr; b.size = 0; }
    Buffer(size_t size) : Span<uint8>(new uint8[size], size) { }
    Buffer(const void* ptr, size_t size);
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
    String(const char* p) { Make(p); }
    String(const wchar_t* p) { Make(p); }
    String(const String& s) { node = s.node; }
    String(String&& s) noexcept { node = static_cast<RCPtr<Node>&&>(s.node); }
    String(ReadOnlySpan<char> str) { Make(str.Ptr(), str.Len()); }
    String(ReadOnlySpan<wchar_t> str) { Make(str.Ptr(), str.Len()); }

    static String PrintF(const char* format, ...);
    static String Concat(const String& s1, const String& s2);
    static String Repeat(char chr, int count);
    static String Join(const ReadOnlySpan<String>& strings, const String& separator);

    String& operator = (const char* p) { Make(p); return *this; }
    String& operator = (const wchar_t* p) { Make(p); return *this; }
    String& operator = (const String& s) { node = s.node; return *this; }
    String& operator = (String&& s) noexcept { node = static_cast<RCPtr<Node>&&>(s.node); return *this; }
    String& operator = (ReadOnlySpan<char> str) { Make(str.Ptr(), str.Len()); return *this; }
    String& operator = (ReadOnlySpan<wchar_t> str) { Make(str.Ptr(), str.Len()); return *this; }

    size_t Length() const { return node.IsValid() ? node->len : 0; }
    operator const char* () const { return node.IsValid() ? node->str : ""; }

    static int Compare(const String& a, const String& b, bool ignoreCase = false);
    static int Compare(const String& a, const char* b, bool ignoreCase = false);
    static int CompareLen(const String& a, const char* b, bool ignoreCase = false);
    template<typename Ts> int Compare(const Ts& s, bool ignoreCase = false) const { return Compare(*this, s, ignoreCase); }

    bool operator! () const { return !node; }
    String operator + (const String& s) const { return Concat(*this, s); }
    String operator += (const String& s) { return *this = Concat(*this, s); }

    template<typename Ts> bool operator < (const Ts& s) const { return Compare(s) < 0; }
    template<typename Ts> bool operator <= (const Ts& s) const { return Compare(s) <= 0; }
    template<typename Ts> bool operator == (const Ts& s) const { return Compare(s) == 0; }
    template<typename Ts> bool operator >= (const Ts& s) const { return Compare(s) >= 0; }
    template<typename Ts> bool operator > (const Ts& s) const { return Compare(s) > 0; }
    template<typename Ts> bool operator != (const Ts& s) const { return Compare(s) != 0; }

    operator ReadOnlySpan<char>() const { return node.IsValid() ? ReadOnlySpan<char>(node->str, node->len) : ReadOnlySpan<char>(); }

    // wrapper to UTF-16 string
    // Beware object lifetimes (using ToWChar() as function argument is fine)
    class WCharProxy
    {
        friend class String;
        WCharProxy() {}
        WCharProxy(const WCharProxy& p) = delete;
        wchar_t* ptr = nullptr;
    public:
        ~WCharProxy() { delete[] ptr; }
        WCharProxy(WCharProxy&& p) noexcept { ptr = p.ptr; p.ptr = nullptr; }
        operator const wchar_t* () const { return ptr ? ptr : L""; }
    } ToWChar() const;

    char* Make(size_t len); // HERE BE DRAGONS, you need to fill the string aftewards

private:
    friend class StringBuilder;

    struct Node : RCObj { size_t len = 0; char str[1] = {}; }; // variable size!
    RCPtr<Node> node;

    void Make(const char* p, size_t len = -1);
    void Make(const wchar_t* p, size_t len = -1);
};

// StringBuilder class, just append strings and get the result at the end
// Supports optional pretty printing for human readable formats
class StringBuilder
{
public:

    void Clear() { strings.Clear(); }

    void Append(const String& str) { if (firstInLine) CheckIndent(); if (str.Length()) strings.PushTail(str); }
    template<class ... args> void Append(const String& str, args ...a) { Append(str); Append(a...); }
    void operator += (const String& str) { Append(str); }

    String ToString() const;

    // pretty printing
    void SetPrettyPrint(bool p) { pretty = p; indent = 0; firstInLine = p; }
    void PrettySpace() { if (pretty) Append(" "); }
    void PrettyNewline(int ind = 0) { if (pretty) { Append("\n"); indent = Max(0, indent + ind); firstInLine = true; } }

private:
    Array<String> strings;
    bool pretty = false;
    int indent = 0;
    bool firstInLine = true;

    void CheckIndent() { strings += String::Repeat(' ', indent); firstInLine = false; }
};

// Scans text and returns symbols, strings, or numbers
// There's an error list you can use to relay errors back to the user
class Scanner
{
public:
    Scanner(const char* text) : ptr(text), line(text) {}

    bool If(const String& str);
    bool IfChar(char c);
    bool Char(char c);
    int64 Decimal(int* digits = nullptr);
    String QuotedString();

    bool operator!() const { return errors.Len() > 0; }

    void Error(const String& err);
    const Array<String>& Errors() const { return errors; }

private:
    const char* ptr;
    const char* line;
    int ln = 0;
    Array<String> errors;
    char buffer[1024] = {};
    int bfill = 0;

    void Skip();
    void AddChar(char c, String& out);
};
