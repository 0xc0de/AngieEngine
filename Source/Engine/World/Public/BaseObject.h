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

#include "Factory.h"
#include <Engine/Core/Public/Document.h>

/*

FBaseObject

Base object class.
Cares of reference counting, garbage collecting and little basic functionality.

*/
class ANGIE_API FBaseObject : public FDummy {
    AN_CLASS( FBaseObject, FDummy )

    friend class FGarbageCollector;

public:
    // Serialize object to document data
    virtual int Serialize( FDocument & _Doc );

    virtual void LoadObject( const char * _Path ) {}

    // Load attributes form document data
    void LoadAttributes( FDocument const & _Document, int _FieldsHead );

    // Add reference
    void AddRef();

    // Remove reference
    void RemoveRef();

    void AddToLoadList();

    void RemoveFromLoadList();

    bool IsInLoadList() const;

    virtual void SetName( FString const & _Name ) { Name = _Name; }

    FString const & GetName() const { return Name; }

    virtual const char * GetResourcePath() const { return ""; }

    // Get total existing objects
    static uint64_t GetTotalObjects() { return TotalObjects; }

    static void ReloadAll();

protected:
    FBaseObject();

    virtual ~FBaseObject();

    FString Name;

private:
    // Total existing objects
    static uint64_t TotalObjects;

    // Current refs count for this object
    int RefCount;

    // Used by garbage collector to add this object to remove list
    FBaseObject * NextPendingKillObject;
    FBaseObject * PrevPendingKillObject;

    // Load list
    FBaseObject * Next;
    FBaseObject * Prev;
    static FBaseObject * GlobalLoadList;
    static FBaseObject * GlobalLoadListTail;
};

/*

FGarbageCollector

Cares of garbage collecting and removing

*/
class FGarbageCollector final {
    AN_FORBID_COPY( FGarbageCollector )

public:
    // Initialize garbage collector
    static void Initialize();

    // Deinitialize garbage collector
    static void Deinitialize();

    // Add object to remove it at next DeallocateObjects() call
    static void AddObject( FBaseObject * _Object );
    static void RemoveObject( FBaseObject * _Object );

    // Deallocates all collected objects
    static void DeallocateObjects();

private:
    static FBaseObject * PendingKillObjects;
    static FBaseObject * PendingKillObjectsTail;
};

template< typename T >
class TRefHolder final {
public:
    T * Object = nullptr;

    ~TRefHolder() {
        if ( Object ) {
            Object->RemoveRef();
        }
    }

    operator T*() const {
        return Object;
    }

    T * operator->() {
        return Object;
    }

    T const * operator->() const {
        return Object;
    }

    void operator=( TRefHolder< T > const & _Ref ) {
        this->operator =( _Ref.Object );
    }

    void operator=( T * _Object ) {
        if ( Object == _Object ) {
            return;
        }
        if ( Object ) {
            Object->RemoveRef();
        }
        Object = _Object;
        if ( Object ) {
            Object->AddRef();
        }
    }
};
