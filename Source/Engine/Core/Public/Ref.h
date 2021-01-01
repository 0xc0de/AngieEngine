/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2020 Alexander Samusev.

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

#include "Std.h"

struct SWeakRefCounter;

struct SZoneAllocator
{
    void * Allocate( std::size_t _SizeInBytes )
    {
        return GZoneMemory.Alloc( _SizeInBytes );
    }

    void Deallocate( void * _Bytes )
    {
        GZoneMemory.Free( _Bytes );
    }
};

template< typename TAllocator >
class TRefCounted
{
public:
    TRefCounted()
        : RefCount( 1 )
    {
    }

    virtual ~TRefCounted() {}

    /** Non-copyable pattern */
    TRefCounted( TRefCounted< TAllocator > const & ) = delete;

    /** Non-copyable pattern */
    TRefCounted & operator=( TRefCounted< TAllocator > const & ) = delete;

    /** Add reference */
    inline void AddRef()
    {
        ++RefCount;
    }

    /** Remove reference */
    inline void RemoveRef()
    {
        if ( --RefCount == 0 ) {
            delete this;
            return;
        }

        AN_ASSERT( RefCount > 0 );
    }

    int GetRefCount() const { return RefCount; }

    /** Set weakref counter. Used by TWeakRef */
    void SetWeakRefCounter( SWeakRefCounter * _RefCounter ) { WeakRefCounter = _RefCounter; }

    /** Get weakref counter. Used by TWeakRef */
    SWeakRefCounter * GetWeakRefCounter() { return WeakRefCounter; }

    void * operator new( size_t _SizeInBytes )
    {
        return TAllocator().Allocate( _SizeInBytes );
    }

    void operator delete( void * _Ptr )
    {
        TAllocator().Deallocate( _Ptr );
    }

private:
    int RefCount;
    SWeakRefCounter * WeakRefCounter = nullptr;
};

using ARefCounted = TRefCounted< SZoneAllocator >;


/**

TRef

Shared pointer

*/
template< typename T >
class TRef final {
public:
    using ReferencedType = T;

    TRef() : Object( nullptr ) {}

    TRef( TRef< T > const & _Ref )
        : Object( _Ref.Object )
    {
        if ( Object ) {
            Object->AddRef();
        }
    }

    explicit TRef( T * _Object )
        : Object( _Object )
    {
        if ( Object ) {
            Object->AddRef();
        }
    }

    ~TRef() {
        if ( Object ) {
            Object->RemoveRef();
        }
    }

    T * GetObject() { return Object; }

    T const * GetObject() const { return Object; }

//    operator bool() const {
//        return Object != nullptr;
//    }

    operator T*() const {
        return Object;
    }

    T & operator *() const {
        AN_ASSERT_( Object, "TRef" );
        return *Object;
    }

    T * operator->() {
        AN_ASSERT_( Object, "TRef" );
        return Object;
    }

    T const * operator->() const {
        AN_ASSERT_( Object, "TRef" );
        return Object;
    }

    void Reset() {
        if ( Object ) {
            Object->RemoveRef();
            Object = nullptr;
        }
    }

    TRef< T > & operator=( TRef< T > const & _Ref ) {
        this->operator =( _Ref.Object );
        return *this;
    }

    TRef< T > & operator=( T * _Object ) {
        if ( Object == _Object ) {
            return *this;
        }
        if ( Object ) {
            Object->RemoveRef();
        }
        Object = _Object;
        if ( Object ) {
            Object->AddRef();
        }
        return *this;
    }
private:
    T * Object;
};


/**

TWeakRef

Weak pointer

*/

struct SWeakRefCounter {
    void * Object;
    int RefCount;
};

class AWeakReference {
protected:
    AWeakReference() : WeakRefCounter( nullptr ) {}

//    void ResetWeakRef( ABaseObject * _Object );

//    void RemoveWeakRef();

    template< typename T >
    void ResetWeakRef( T * _Object ) {
        T * Cur = WeakRefCounter ? (T*)WeakRefCounter->Object : nullptr;

        if ( Cur == _Object ) {
            return;
        }

        RemoveWeakRef< T >();

        if ( !_Object ) {
            return;
        }

        WeakRefCounter = _Object->GetWeakRefCounter();
        if ( !WeakRefCounter ) {
            WeakRefCounter = AllocateWeakRefCounter();
            WeakRefCounter->Object = _Object;
            WeakRefCounter->RefCount = 1;
            _Object->SetWeakRefCounter( WeakRefCounter );
        } else {
            WeakRefCounter->RefCount++;
        }
    }

    template< typename T >
    void RemoveWeakRef() {
        if ( WeakRefCounter ) {
            if ( --WeakRefCounter->RefCount == 0 ) {
                if ( WeakRefCounter->Object ) {
                    ((T*)WeakRefCounter->Object)->SetWeakRefCounter( nullptr );
                }
                DeallocateWeakRefCounter( WeakRefCounter );
            }
            WeakRefCounter = nullptr;
        }
    }

    SWeakRefCounter * WeakRefCounter;

private:
    SWeakRefCounter * AllocateWeakRefCounter();

    void DeallocateWeakRefCounter( SWeakRefCounter * _Counter );
};

template< typename T >
class TWeakRef final : public AWeakReference {
public:
    TWeakRef() {}

    TWeakRef( TWeakRef< T > const & _Ref ) {
        ResetWeakRef( const_cast< T * >( _Ref.GetObject() ) );
    }

    TWeakRef( TRef< T > const & _Ref ) {
        ResetWeakRef( const_cast< T * >( _Ref.GetObject() ) );
    }

    explicit TWeakRef( T * _Object ) {
        ResetWeakRef( _Object );
    }

    ~TWeakRef() {
        RemoveWeakRef< T >();
    }

    TRef< T > ToStrongRef() const {
        return TRef< T >( const_cast< T * >( GetObject() ) );
    }

    T * GetObject() {
        return WeakRefCounter ? static_cast< T * >( WeakRefCounter->Object ) : nullptr;
    }

    T const * GetObject() const {
        return WeakRefCounter ? static_cast< T * >( WeakRefCounter->Object ) : nullptr;
    }

//    operator bool() const {
//        return !IsExpired();
//    }

    operator T*() const {
        return const_cast< T * >( GetObject() );
    }

    T & operator *() const {
        AN_ASSERT_( !IsExpired(), "TWeakRef" );
        return *GetObject();
    }

    T * operator->() {
        AN_ASSERT_( !IsExpired(), "TWeakRef" );
        return GetObject();
    }

    T const * operator->() const {
        AN_ASSERT_( !IsExpired(), "TWeakRef" );
        return GetObject();
    }

    bool IsExpired() const {
        return !WeakRefCounter || static_cast< T * >( WeakRefCounter->Object ) == nullptr;
    }

    void Reset() {
        RemoveWeakRef< T >();
    }

    void operator=( T * _Object ) {
        ResetWeakRef( _Object );
    }

    void operator=( TRef< T > const & _Ref ) {
        ResetWeakRef( const_cast< T * >( _Ref.GetObject() ) );
    }

    void operator=( TWeakRef< T > const & _Ref ) {
        ResetWeakRef( const_cast< T * >( _Ref.GetObject() ) );
    }
};

template< typename T >
AN_FORCEINLINE bool operator == ( TRef< T > const & _Ref, TRef< T > const & _Ref2 ) { return _Ref.GetObject() == _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator != ( TRef< T > const & _Ref, TRef< T > const & _Ref2 ) { return _Ref.GetObject() != _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator == ( TRef< T > const & _Ref, TWeakRef< T > const & _Ref2 ) { return _Ref.GetObject() == _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator != ( TRef< T > const & _Ref, TWeakRef< T > const & _Ref2 ) { return _Ref.GetObject() != _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator == ( TWeakRef< T > const & _Ref, TRef< T > const & _Ref2 ) { return _Ref.GetObject() == _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator != ( TWeakRef< T > const & _Ref, TRef< T > const & _Ref2 ) { return _Ref.GetObject() != _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator == ( TWeakRef< T > const & _Ref, TWeakRef< T > const & _Ref2 ) { return _Ref.GetObject() == _Ref2.GetObject(); }

template< typename T >
AN_FORCEINLINE bool operator != ( TWeakRef< T > const & _Ref, TWeakRef< T > const & _Ref2 ) { return _Ref.GetObject() != _Ref2.GetObject(); }

template< typename T, typename... Args >
inline TRef< T > MakeRef( Args && ... args )
{
    // create object (refcount=1)
    T * x = new T( StdForward< Args >( args )... );
    // handle reference (refcount=2)
    TRef< T > ref( x );
    // keep refcount=1
    x->RemoveRef();
    return ref;
}

template< typename T >
AN_FORCEINLINE void CheckedDelete( T * Ptr )
{
    using type_must_be_complete = char[sizeof(T) ? 1 : -1];
    (void) sizeof( type_must_be_complete );
    delete Ptr;
}

template< typename T >
class TUniqueRef
{
public:
    TUniqueRef()
        : Object(nullptr)
    {
    }

    explicit TUniqueRef( T * InPtr )
        : Object( InPtr )
    {
    }

    TUniqueRef( TUniqueRef< T > const & ) = delete;
    TUniqueRef & operator=( TUniqueRef< T > const & ) = delete;

    TUniqueRef( TUniqueRef && Up )
        : Object( Up.Detach() )
    {

    }

    ~TUniqueRef()
    {
        Reset();
    }

    TUniqueRef & operator=( TUniqueRef && Up )
    {
        Reset( Up.Detach() );
        return *this;
    }

    T * operator->() const
    {
        AN_ASSERT( Object );
        return Object;
    }

    T & operator*() const
    {
        AN_ASSERT( Object );
        return *Object;
    }

    template< typename U >
    bool operator==( TUniqueRef< U > const & Rhs )
    {
        return Object == Rhs.Object;
    }

    template< typename U >
    bool operator!=( TUniqueRef< U > const & Rhs )
    {
        return Object != Rhs.Object;
    }

    operator bool() const
    {
        return Object != nullptr;
    }

//    void Swap( TUniqueRef & Up )
//    {
//        StdSwap( Object, Up.Object );
//    }

    T * Detach()
    {
        T * ptr = Object;
        Object = nullptr;
        return ptr;
    }

    T * GetObject() const
    {
        return Object;
    }

    void Reset( T * InPtr = nullptr )
    {
        CheckedDelete( Object );
        Object = InPtr;
    }

private:
    T * Object;
};

template< typename T, typename ... Args >
TUniqueRef< T > MakeUnique( Args && ... args )
{
    return TUniqueRef< T >( new T( StdForward< Args >( args )... ) );
}
