/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2019 Alexander Samusev.

This file is part of the Angie Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include "String.h"

struct ATokenBuffer final {
    AN_FORBID_COPY( ATokenBuffer )

    using Allocator = AZoneAllocator;

    const char * GetBuffer() const { return Start; }
    bool InSitu() const { return bInSitu; }

    const char * Cur;
    int LineNumber;

    ATokenBuffer();
    ~ATokenBuffer();

    void Initialize( const char * _String, bool _InSitu );
    void Deinitialize();

private:
    char * Start;
    bool bInSitu;
};

enum ETokenType {
    TT_Unknown,
    TT_EOF,
    TT_Bracket,
    TT_Field,
    TT_String
};

struct SToken final {
    const char * Begin;
    const char * End;
    int Type;

    void FromString( const char * _Str );
    AString ToString() const;
    bool CompareToString( const char * _Str ) const;
    const char * NamedType() const;
};

struct SDocumentField;

struct SDocumentValue final {
    enum { T_String, T_Object };
    int Type;
    SToken Token;   // for T_String
    int FieldsHead; // for T_Object list of fields
    int FieldsTail; // for T_Object list of fields
    int Next;       // next value
    int Prev;       // prev value
};

struct SDocumentField final {
    SToken Name;
    int ValuesHead; // list of values
    int ValuesTail; // list of values
    int Next;       // next field
    int Prev;       // prev field
};

struct ADocumentProxyBuffer final {
    AN_FORBID_COPY( ADocumentProxyBuffer )

    using Allocator = AZoneAllocator;

    AString & NewString();
    AString & NewString( const char * _String );
    AString & NewString( AString const & _String );

    ADocumentProxyBuffer();
    ~ADocumentProxyBuffer();

private:
    struct AStringList {
        AStringList() {}
        AStringList( const char * _Str ) : Str( _Str ) {}
        AStringList( AString const & _Str ) : Str( _Str ) {}
        AString Str;
        AStringList * Next;
    };

    AStringList * StringList;
};

struct ANGIE_API ADocument final {
    AN_FORBID_COPY( ADocument )

    using Allocator = AZoneAllocator;

    ATokenBuffer Buffer;
    ADocumentProxyBuffer ProxyBuffer;

    bool bCompactStringConversion = false;

    int FieldsHead = -1;    // list of fields
    int FieldsTail = -1;    // list of fields

    SDocumentField * Fields = nullptr;
    SDocumentValue * Values = nullptr;

    ADocument();
    ~ADocument();

    int AllocateField();
    int AllocateValue();
    void Clear();

    void FromString( const char * _Script, bool _InSitu );
    AString ToString() const;

    AString ObjectToString( int _Object ) const;

    SDocumentField * FindField( int _FieldsHead, const char * _Name ) const;

    int CreateField( const char * _FieldName );
    int CreateStringValue( const char * _Value );
    int CreateObjectValue();
    void AddField( int _Field );
    void AddValueToField( int _FieldOrArray, int _Value );
    int CreateStringField( const char * _FieldName, const char * _FieldValue );
    void AddFieldToObject( int _Object, int _Field );
    int AddStringField( int _Object, const char * _FieldName, const char * _FieldValue );
    int AddArray( int _Object, const char * _ArrayName );

private:
    int FieldsMemReserved = 0;
    int FieldsCount = 0;
    int ValuesMemReserved = 0;
    int ValuesCount = 0;
};

void PrintDocument( ADocument const & _Doc );
