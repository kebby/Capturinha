//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"

// Usage: JSON_DEFINE_ENUM(MyEnum, ("My", "Enum", "Values"))
#define JSON_DEFINE_ENUM(type, strings) template<> struct JsonEnumDef<type> { \
    static const Array<String> &GetValues() { static const Array<String> strs##strings; return strs; } \
    JsonEnumDef(const type & r) : ref(r) { } \
    const type & ref; \
};

#define JSON_BEGIN() template<class TV> void _VisitJson(TV &visitor) const { bool comma = false;
#define JSON_VALUE(v) visitor.Member(#v, v, comma); comma = true; 
#define JSON_VALUE_NAME(v, name) visitor.Member(name, v, comma); comma = true;
#define JSON_ENUM(v)visitor.Member(#v, GetEnumDef(v), comma); comma = true; 
#define JSON_ENUM_NAME(v, name) visitor.Member(name, GetEnumDef(v), comma); comma = true;
#define JSON_END() }

template<typename T> struct JsonEnumDef;
template<typename T> JsonEnumDef<T> GetEnumDef(const T& value) { return JsonEnumDef<T>(value); }

class Json
{
public:
    template<typename T> static String Serialize(const T &value, bool pretty = false)
    {
        StringBuilder sb;
        sb.SetPrettyPrint(pretty);
        Write(sb, value);
        return sb.ToString();
    }

    template<typename T> static bool Deserialize(const char *json, T &into)
    {
        Scanner scan(json);
        Read(scan, into);       
        return !!scan;
    }

    template<typename T> static bool Deserialize(const char* json, T &into, Array<String> &errors)
    {      
        Scanner scan(json);
        Read(scan, into);
        errors = scan.Errors();
        return !!scan;
    }


private:

    //------------------------------------------------------------------
    // reading
  
    static void ReadJsonNumber(Scanner& scan, int64& major, int64& minor, int& minorDigits, int& exp, bool &sign)
    {
        major = 0; minor = 0; minorDigits = 0; exp = 0;
        sign = scan.IfChar('-');
        major = scan.Decimal();
        if (scan.IfChar('.'))
            minor = scan.Decimal(&minorDigits);
        if (scan.IfChar('e') || scan.IfChar('E'))
        {
            bool sign2 = false;
            scan.IfChar('+');
            if (scan.IfChar('-'))
                sign2 = true;
            exp = (int)scan.Decimal();
            if (sign2)
                exp = -exp;
        }
    }

    static void Read(Scanner& scan, double& value)
    {
        int64 major, minor;
        int mdigits, exp;
        bool sign;
        ReadJsonNumber(scan, major, minor, mdigits, exp, sign);
        value = ((double)major + (double)minor * pow(10, -mdigits)) * pow(10, exp) * (sign ? -1 : 1);
    }

    static void Read(Scanner& scan, int64& value)
    {
        int64 major, minor;
        int mdigits, exp;
        bool sign;
        ReadJsonNumber(scan, major, minor, mdigits, exp, sign);
        if (exp)
            value = (int64)(((double)major + (double)minor * pow(10, -mdigits)) * pow(10, exp) * (sign ? -1 : 1));
        else
            value = major * (sign ? -1 : 1);
    }

    static void CheckRange(Scanner &scan, int64& v, int64 min, int64 max)
    {
        if (v < min) 
        {
            v = min;
            scan.Error(String::PrintF("Number too small (min: %lld)", min));
        }
        else if (v > max)
        {
            v = max;
            scan.Error(String::PrintF("Number too large (mas: %lld)", max));
        }
    }

    static void Read(Scanner& scan, float& value) { double v; Read(scan, v); value = (float)v; }
    static void Read(Scanner& scan, int8& value) { int64 v; Read(scan, v); CheckRange(scan, v, -0x80, 0x7f); value = (int8)v; }
    static void Read(Scanner& scan, uint8& value) { int64 v; Read(scan, v); CheckRange(scan, v, 0, 0xff); value = (uint8)v; }
    static void Read(Scanner& scan, int16& value) { int64 v; Read(scan, v); CheckRange(scan, v, -0x8000, 0x7fff); value = (int16)v; }
    static void Read(Scanner& scan, uint16& value) { int64 v; Read(scan, v); CheckRange(scan, v, 0, 0xffff); value = (uint16)v; }
    static void Read(Scanner& scan, int& value) { int64 v; Read(scan, v); CheckRange(scan, v, -0x80000000ll, 0x7fffffffll); value = (int)v; }
    static void Read(Scanner& scan, uint& value) { int64 v; Read(scan, v); CheckRange(scan, v, 0, 0xffffffffll); value = (uint)v; }

    template<class T> static void Read(Scanner& scan, JsonEnumDef<T>& def)
    {
        String value = scan.QuotedString();
        auto strs = def.GetValues();
        for (int i = 0; i < strs.Count(); i++)
            if (!value.Compare(strs[i], true))
            {
                const_cast<T&>(def.ref) = (T)i;
                return;
            }
        scan.Error(String::PrintF("Unknown value %s (expected: %s)", (const char*)value, (const char*)String::Join(strs, ", ")));
    }

    template<class T> static void Read(Scanner& scan, Array<T>& array)
    {
        if (!scan.Char('[')) return;

        array.Clear();
        while (!scan.IfChar(']'))
        {
            T value;
            Read(scan, value);
            array.PushTail(value);
            scan.IfChar(',');
        }
    }

    template<class T> struct ReadVisitor
    {
        ReadVisitor(Scanner &s, String n) : name(n), scan(s) {}

        template<typename T> void Member(String mn, const T& value, bool)
        {
            if (!found && !name.Compare(mn, true))
            {
                Read(scan, const_cast<T&>(value));
                found = true;
            }
        }

        String name;
        Scanner& scan;
        bool found = false;
    };

    template<class T> static void Read(Scanner& scan, T& object)
    {
        if (!scan.Char('{')) return;

        while (!scan.IfChar('}'))
        {
            String name = scan.QuotedString();
            if (!scan.Char(':')) return;

            ReadVisitor<T> visitor(scan, name);
            object._VisitJson(visitor);
            if (!visitor.found)
            {
                scan.Error(String::PrintF("Unknown key: %s", (const char*)name));
                return;
            }
            scan.IfChar(',');
        }
    }

    static void Read(Scanner& scan, bool& str) 
    { 
        if (scan.If("true")) str = true;
        else if (scan.If("false")) str = false;
        else scan.Error("Expected true or false");
    }

    static void Read(Scanner& scan, String& str) { str = scan.If("null") ? String() : scan.QuotedString(); }

    //------------------------------------------------------------------
    // writing
  
    static void Write(StringBuilder& sb, const double& value) { sb += String::PrintF("%f", value); }
    static void Write(StringBuilder& sb, const int64& value) { sb += String::PrintF("%lld", value); }
    static void Write(StringBuilder& sb, const uint64& value) { sb += String::PrintF("%llu", value); }
    static void Write(StringBuilder& sb, const bool& value) { sb += value ? "true" : "false"; }

    static void Write(StringBuilder& sb, const int8& value) { Write(sb, (int64)value); }
    static void Write(StringBuilder& sb, const uint8& value) { Write(sb, (uint64)value); }
    static void Write(StringBuilder& sb, const int16& value) { Write(sb, (int64)value); }
    static void Write(StringBuilder& sb, const uint16& value) { Write(sb, (uint64)value); }
    static void Write(StringBuilder& sb, const int& value) { Write(sb, (int64)value); }
    static void Write(StringBuilder& sb, const uint& value) { Write(sb, (uint64)value); }
    static void Write(StringBuilder& sb, const float& value) { Write(sb, (double)value); }

    static void Write(StringBuilder& sb, const char *value) 
    { 
        if (value)
        {
            char buffer[1024];
            int bfill = 0;

            auto addChar = [&](char c)
            {
                buffer[bfill++] = c;
                if (bfill == 1023)
                {
                    buffer[bfill] = 0;
                    sb += buffer;
                    bfill = 0;
                }
            };

            sb += "\"";
            while (*value)
            {
                switch (*value)
                {
                case '\\': case '\"': addChar('\\'); addChar(*value); break;
                case '\r': addChar('\\'); addChar('r'); break;
                case '\n': addChar('\\'); addChar('n'); break;
                case '\t': addChar('\\'); addChar('t'); break;
                default: if ((uint)*value >= ' ') addChar(*value); break;
                }
                value++;
            }

            buffer[bfill] = 0;
            sb += buffer;
            sb += "\"";
        }
        else
            sb += "null";
    }

    static void Write(StringBuilder& sb, const String& value) { Write(sb, (const char*)value); }

    template<class T> static void Write(StringBuilder& sb, const JsonEnumDef<T>& value) 
    {
        Write(sb, JsonEnumDef<T>::GetValues()[(size_t)value.ref]);
    }

    template<class T> static void Write(StringBuilder& sb, const Array<T> &array)
    {
        sb += "["; 
        sb.PrettyNewline(2);
        bool comma = false;
        for (auto &vv : array)
        {
            if (comma) 
            {
                sb += ","; 
                sb.PrettyNewline();
            }
            Write(sb, vv);
            comma = true;          
        }
        sb.PrettyNewline(-2);
        sb += "]";
    }

    struct WriteVisitor
    {
        WriteVisitor(StringBuilder& into) : sb(into) {}
        StringBuilder& sb;

        template<typename T> void Member(String name, const T& value, bool comma)
        {
            if (comma) {
                sb += ","; 
                sb.PrettyNewline();
            }
            sb.Append("\"", name, "\":");
            sb.PrettySpace();
            Write(sb, value);
        }
    };

    template<typename T> static void Write(StringBuilder& sb, const T& object)
    {
        sb += "{"; 
        sb.PrettyNewline(2);
        WriteVisitor visitor(sb);
        object._VisitJson(visitor);
        sb.PrettyNewline(-2);
        sb += "}";
    }

};