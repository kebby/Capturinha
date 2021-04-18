#include "types.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <windows.h>
#include <stringapiset.h>

void String::MakeNode(const char* p, int len)
{
    if (!p || !p[0]) return;
    if (len<0) len = (int)strlen(p);
    node.Clear();
    void* mem = new uint8[sizeof(Node) + len];
    node = RCPtr<Node>(new (mem) Node);
    node->len = len;
    strcpy_s(node->str, len + 1, p);
}

void String::MakeNode(const wchar_t* p, int len)
{
    if (!p || !p[0]) return;
    int bytes = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, p, len, NULL, 0, NULL, NULL);
    void* mem = new uint8[sizeof(Node) + bytes];
    node = RCPtr<Node>(new (mem) Node);
    node->len = bytes;
    WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, p, len, node->str, bytes, NULL, NULL);
}

String String::Concat(const String &s1, const String &s2)
{
    if (!s1) return s2;
    if (!s2) return s1;
    auto& n1 = s1.node;
    auto& n2 = s2.node;
    String str;
    size_t len = n1->len + n2->len;
    void* mem = new uint8[sizeof(Node) + len];
    str.node = RCPtr<Node>(new (mem) Node);
    str.node->len = len;
    strcpy_s(str.node->str, n1->len + 1, n1->str);
    strcpy_s(str.node->str + n1->len, n2->len + 1, n2->str);
    return str;
}

String String::PrintF(const char* format, ...)
{
    constexpr size_t size = 4096;
    thread_local char buf[size];

    va_list args;
    va_start(args, format);
    int len = vsnprintf_s(buf, size, format, args);
    if (len < 0) len = 0;
    va_end(args);
    buf[len] = 0;

    String str;
    str.MakeNode(buf, len);
    return str;
}

int String::Compare(const String& s1, const String& s2, bool ignoreCase)
{  
    if (ignoreCase)
        return _stricmp(s1, s2);
    else
        return strcmp(s1, s2);
}