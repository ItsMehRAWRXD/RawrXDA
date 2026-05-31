#pragma once
// RawrXD_Win32_Foundation.h
// Compatibility shim forwarding to RawrXD_Foundation.h
// Zero Qt dependencies - Pure Win32/C++20 foundation

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "RawrXD_Foundation.h"

namespace RawrXD {

    // Compatibility aliases
    template<typename T>
    using List = Vector<T>;

    // Variant was unique to Win32_Foundation, adding it here.
    // QVariant replacement (simplified)
    class Variant {
    public:
        enum Type { Type_Null, Type_Bool, Type_Int, Type_LongLong, Type_Double, Type_String, Type_ByteArray, Type_Pointer };
        
    private:
        Type type = Type_Null;
        union {
            bool b;
            int64_t i;
            double d;
            void* ptr;
        };
        RawrXD::String s;
        RawrXD::ByteArray ba;
        
    public:
        Variant() = default;
        Variant(bool v) : type(Type_Bool), b(v) {}
        Variant(int v) : type(Type_Int), i(v) {}
        Variant(int64_t v) : type(Type_LongLong), i(v) {}
        Variant(double v) : type(Type_Double), d(v) {}
        Variant(const RawrXD::String& v) : type(Type_String), s(v) {}
        Variant(const char* v) : type(Type_String), s(v) {}
        Variant(const RawrXD::ByteArray& v) : type(Type_ByteArray), ba(v) {}
        template<typename T> Variant(T* p) : type(Type_Pointer), ptr((void*)p) {}
        
        bool isNull() const { return type == Type_Null; }
        bool isValid() const { return type != Type_Null; }
        Type getType() const { return type; }
        
        bool toBool() const { return type == Type_Bool ? b : false; }
        int toInt() const { return type == Type_Int ? (int)i : (type == Type_LongLong ? (int)i : 0); }
        int64_t toLongLong() const { return type == Type_LongLong || type == Type_Int ? i : 0; }
        double toDouble() const { return type == Type_Double ? d : (type == Type_Int || type == Type_LongLong ? (double)i : 0.0); }
        RawrXD::String toString() const; 
        RawrXD::ByteArray toByteArray() const { return type == Type_ByteArray ? ba : RawrXD::ByteArray(); }
        template<typename T> T* toPointer() const { return type == Type_Pointer ? (T*)ptr : nullptr; }
    };

    inline RawrXD::String Variant::toString() const {
        if (type == Type_String) return s;
        if (type == Type_Int) return RawrXD::String::number((int)i);
        if (type == Type_Double) return RawrXD::String::number(d);
        if (type == Type_Bool) return b ? RawrXD::String(L"true") : RawrXD::String(L"false");
        return RawrXD::String();
    }
}
