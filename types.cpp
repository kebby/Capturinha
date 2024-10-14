//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "types.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <windows.h>
#include <stringapiset.h>

char *String::Make(int len)
{
    void* mem = new uint8[sizeof(Node) + len];
    node = RCPtr<Node>(new (mem) Node);
    node->len = len;    
    node->str[len] = 0;
    return node->str;
}

void String::Make(const char* p, int len)
{
    if (!p || !p[0]) return;
    if (len<0) len = (int)strlen(p);
    char *ptr = Make(len);
    memcpy(ptr, p, len);
}

void String::Make(const wchar_t* p, int len)
{
    if (!p || !p[0]) return;
    int bytes = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, p, len, NULL, 0, NULL, NULL);
    char *ptr = Make(bytes);
    WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, p, len, ptr, bytes, NULL, NULL);
}


String String::Concat(const String &s1, const String &s2)
{
    if (!s1) return s2;
    if (!s2) return s1;
    auto& n1 = s1.node;
    auto& n2 = s2.node;
    size_t len = n1->len + n2->len;
    String str;
    char *ptr = str.Make((int)len);
    memcpy(ptr, n1->str, n1->len);
    memcpy(ptr + n1->len, n2->str, n2->len);
    return str;
}

String String::Repeat(char chr, int times)
{
    String str;
    str.Make(times);
    memset(str.node->str, chr, times);
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
    str.Make(buf, len);
    return str;
}

String String::Join(const Array<String> &strings, const String &separator)
{
    size_t count = strings.Count();
    if (!count) return String();
    if (count == 1) return strings[0];
    StringBuilder sb;
    for (int i = 0; i < count; i++)
    {
        if (i) sb += separator;
        sb += strings[i];
    }
    return sb.ToString();
}


int String::Compare(const String& s1, const String& s2, bool ignoreCase)
{  
    if (ignoreCase)
        return _stricmp(s1, s2);
    else
        return strcmp(s1, s2);
}

int String::Compare(const String& s1, const char *s2, bool ignoreCase)
{
    if (!s2) s2 = "";
    if (ignoreCase)
        return _stricmp(s1, s2);
    else
        return strcmp(s1, s2);
}

int String::CompareLen(const String& s1, const char* s2, bool ignoreCase)
{
    if (!s2) s2 = "";
    if (ignoreCase)
        return _strnicmp(s1, s2, s1.Length());
    else
        return strncmp(s1, s2, s1.Length());
}

String::WCharProxy String::ToWChar() const
{ 
    WCharProxy proxy;
    if (!node) return proxy;
    int len = MultiByteToWideChar(CP_UTF8, 0, node->str, (int)node->len, 0, 0);
    proxy.ptr = new wchar_t[len + 1];
    MultiByteToWideChar(CP_UTF8, 0, node->str, (int)node->len, proxy.ptr, len+1);
    proxy.ptr[len] = 0;
    return proxy;
}


String StringBuilder::ToString() const
{
    size_t len = 0;
    for (auto &s : strings)
        len += s.Length();

    String out;
    out.Make((int)len);
    size_t offs = 0;
    for (auto &s : strings)
    {
        len = s.Length();       
        memcpy(out.node->str + offs, s.node->str, len);
        offs += len;
    }
    out.node->str[offs] = 0;
    return out;
}


bool Scanner::If(const String& str)
{
    Skip();
    if (!String::CompareLen(str, ptr, true))
    {
        ptr += str.Length();
        return true;
    }
    return false;
}

bool Scanner::IfChar(char c)
{
    Skip();
    if (*ptr == c)
    {
        ptr++;
        return true;
    }
    return false;
}

bool Scanner::Char(char c)
{
    bool ret = IfChar(c);
    if (!ret)
        Error(String::PrintF("expected %c", c));
    return ret;
}

int64 Scanner::Decimal(int* digits)
{
    if (*ptr < '0' || *ptr > '9')
    {
        Error("Number expected");
        if (digits) *digits = 0;
        return 0;
    }
    int64 ret = 0;
    int dig = 0;
    while (*ptr >= '0' && *ptr <= '9')
    {
        if (ret || *ptr > '0') dig++;
        ret = 10 * ret + ((int64)*ptr - '0');
        ptr++;
    }
    if (digits) *digits = dig;
    return ret;
}

void Scanner::Error(const String& err)
{
    errors += String::PrintF("Error (%d,%d): %s", ln + 1, ptr - line, (const char*)err);
}

String Scanner::QuotedString()
{
    Skip();
    if (!Char('\"')) return String();

    String ret;
    bfill = 0;
    while (*ptr && *ptr != '\"' && *ptr != '\n' && *ptr != '\r')
    {
        if (*ptr == '\\')
        {
            ptr++;
            switch (*ptr)
            {
            case 0: ptr--; break;
            case 'r': AddChar('\r', ret); break;
            case 't': AddChar('\t', ret); break;
            case 'n': AddChar('\n', ret); break;
            case 'b': AddChar('\b', ret); break;
            case 'f': AddChar('\f', ret); break;
            case 'u': ASSERT0("IMPLEMENT ME");
            case '\\': case '"': case '/': AddChar(*ptr, ret); break;
            }
            ptr++;
        }
        else
            AddChar(*ptr++, ret);
    }
    if (!Char('\"')) return String();

    buffer[bfill] = 0;
    ret = ret + buffer;
    return ret;
}

void Scanner::Skip()
{
    while (*ptr && ((uint8)(*ptr) <= ' '))
    {
        if (*ptr == '\n')
        {
            ln++;
            line = ptr + 1;
        }
        ptr++;
    }
}

void Scanner::AddChar(char c, String& out)
{
    buffer[bfill++] = c;
    if (bfill == 1023)
    {
        buffer[bfill] = 0;
        out = out + buffer;
        bfill = 0;
    }
}
