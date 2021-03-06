/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2021 Alexander Samusev.

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

#include "Memory.h"

/**

TPodVector

Array for POD types

*/
template< typename T, int BASE_CAPACITY = 32, int GRANULARITY = 32, typename Allocator = AZoneAllocator >
class TPodVector final {
public:
    typedef T * Iterator;
    typedef const T * ConstIterator;

    enum { TYPE_SIZEOF = sizeof( T ) };

    TPodVector()
        : ArrayData( StaticData ), ArraySize( 0 ), ArrayCapacity( BASE_CAPACITY )
    {
        static_assert( BASE_CAPACITY > 0, "TPodVector: Invalid BASE_CAPACITY" );
        AN_ASSERT( IsAlignedPtr( StaticData, 16 ) );
    }

    TPodVector( int _Size )
        : ArraySize( _Size )
    {
        AN_ASSERT( _Size >= 0 );
        if ( _Size > BASE_CAPACITY ) {
            ArrayCapacity = _Size;
            ArrayData = (T *)Allocator::Inst().Alloc( TYPE_SIZEOF * ArrayCapacity );
        } else {
            ArrayCapacity = BASE_CAPACITY;
            ArrayData = StaticData;
        }
    }

    TPodVector( TPodVector const & _Array )
        : TPodVector( _Array.ArraySize )
    {
        Core::MemcpySSE( ArrayData, _Array.ArrayData, TYPE_SIZEOF * ArraySize );
    }    

    TPodVector( T const * _Data, int _Size )
        : TPodVector( _Size )
    {
        Core::Memcpy( ArrayData, _Data, TYPE_SIZEOF * ArraySize );
    }

    TPodVector( std::initializer_list< T > InitializerList )
        : TPodVector( InitializerList.size() )
    {
        std::copy( InitializerList.begin(), InitializerList.end(), ArrayData );
    }

    ~TPodVector()
    {
        if ( ArrayData != StaticData ) {
            Allocator::Inst().Free( ArrayData );
        }
    }

    void Clear()
    {
        ArraySize = 0;
    }

    void Free()
    {
        if ( ArrayData != StaticData ) {
            Allocator::Inst().Free( ArrayData );
            ArrayData = StaticData;
        }
        ArraySize = 0;
        ArrayCapacity = BASE_CAPACITY;
    }

    void ShrinkToFit()
    {
        if ( ArrayData == StaticData || ArrayCapacity == ArraySize ) {
            return;
        }

        if ( ArraySize <= BASE_CAPACITY ) {
            if ( ArraySize > 0 ) {
                Core::MemcpySSE( StaticData, ArrayData, TYPE_SIZEOF * ArraySize );
            }
            Allocator::Inst().Free( ArrayData );
            ArrayData = StaticData;
            ArrayCapacity = BASE_CAPACITY;
            return;
        }

        T * data = ( T * )Allocator::Inst().Alloc( TYPE_SIZEOF * ArraySize );
        Core::MemcpySSE( data, ArrayData, TYPE_SIZEOF * ArraySize );
        Allocator::Inst().Free( ArrayData );
        ArrayData = data;
        ArrayCapacity = ArraySize;
    }

    void Reserve( int _NewCapacity )
    {
        if ( _NewCapacity <= ArrayCapacity ) {
            return;
        }
        if ( ArrayData == StaticData ) {
            ArrayData = ( T * )Allocator::Inst().Alloc( TYPE_SIZEOF * _NewCapacity );
            Core::MemcpySSE( ArrayData, StaticData, TYPE_SIZEOF * ArraySize );
        } else {
            ArrayData = ( T * )Allocator::Inst().Realloc( ArrayData, TYPE_SIZEOF * _NewCapacity, true );
        }
        ArrayCapacity = _NewCapacity;
    }

    void ReserveInvalidate( int _NewCapacity )
    {
        if ( _NewCapacity <= ArrayCapacity ) {
            return;
        }
        if ( ArrayData == StaticData ) {
            ArrayData = ( T * )Allocator::Inst().Alloc( TYPE_SIZEOF * _NewCapacity );
        } else {
            ArrayData = ( T * )Allocator::Inst().Realloc( ArrayData, TYPE_SIZEOF * _NewCapacity, false );
        }
        ArrayCapacity = _NewCapacity;
    }

    void Resize( int _Size )
    {
        AN_ASSERT( _Size >= 0 );
        if ( _Size > ArrayCapacity ) {
            const int capacity = GrowCapacity( _Size );
            Reserve( capacity );
        }
        ArraySize = _Size;
    }

    void ResizeInvalidate( int _Size )
    {
        AN_ASSERT( _Size >= 0 );
        if ( _Size > ArrayCapacity ) {
            const int capacity = GrowCapacity( _Size );
            ReserveInvalidate( capacity );
        }
        ArraySize = _Size;
    }

    void Memset( const int _Value )
    {
        Core::MemsetSSE( ArrayData, _Value, TYPE_SIZEOF * ArraySize );
    }

    void ZeroMem()
    {
        Core::ZeroMemSSE( ArrayData, TYPE_SIZEOF * ArraySize );
    }

    /** Swap elements */
    void Swap( int _Index1, int _Index2 )
    {
        const T tmp = (*this)[ _Index1 ];
        (*this)[ _Index1 ] = (*this)[ _Index2 ];
        (*this)[ _Index2 ] = tmp;
    }

    void Reverse()
    {
        const int HalfArrayLength = ArraySize >> 1;
        const int ArrayLengthMinusOne = ArraySize - 1;
        T tmp;
        for ( int i = 0 ; i < HalfArrayLength ; i++ ) {
            tmp = ArrayData[ i ];
            ArrayData[ i ] = ArrayData[ ArrayLengthMinusOne - i ];
            ArrayData[ ArrayLengthMinusOne - i ] = tmp;
        }
    }

    /** Reverse elements order in range [ _FirstIndex ; _LastIndex ) */
    void Reverse( int _FirstIndex, int _LastIndex )
    {
        AN_ASSERT_( _FirstIndex >= 0 && _FirstIndex < ArraySize, "TPodVector::Reverse: array index is out of bounds" );
        AN_ASSERT_( _LastIndex >= 0 && _LastIndex <= ArraySize, "TPodVector::Reverse: array index is out of bounds" );
        AN_ASSERT_( _FirstIndex < _LastIndex, "TPodVector::Reverse: invalid order" );

        const int HalfRangeLength = ( _LastIndex - _FirstIndex ) >> 1;
        const int LastIndexMinusOne = _LastIndex - 1;

        T tmp;
        for ( int i = 0 ; i < HalfRangeLength ; i++ ) {
            tmp = ArrayData[ _FirstIndex + i ];
            ArrayData[ _FirstIndex + i ] = ArrayData[ LastIndexMinusOne - i ];
            ArrayData[ LastIndexMinusOne - i ] = tmp;
        }
    }

    void Insert( int _Index, const T & _Element )
    {
        if ( _Index == ArraySize ) {
            Append( _Element );
            return;
        }

        AN_ASSERT_( _Index >= 0 && _Index < ArraySize, "TPodVector::Insert: array index is out of bounds" );

        const int newLength = ArraySize + 1;
        const int capacity = GrowCapacity( newLength );

        if ( capacity > ArrayCapacity ) {
            T * data = ( T * )Allocator::Inst().Alloc( TYPE_SIZEOF * capacity );

            Core::MemcpySSE( data, ArrayData, TYPE_SIZEOF * _Index );
            data[ _Index ] = _Element;
            const int elementsToMove = ArraySize - _Index;
            Core::Memcpy( &data[ _Index + 1 ], &ArrayData[ _Index ], TYPE_SIZEOF * elementsToMove );

            if ( ArrayData != StaticData ) {
                Allocator::Inst().Free( ArrayData );
            }
            ArrayData = data;
            ArrayCapacity = capacity;
            ArraySize = newLength;
            return;
        }

        Core::Memmove( ArrayData + _Index + 1, ArrayData + _Index, TYPE_SIZEOF * ( ArraySize - _Index ) );
        ArrayData[ _Index ] = _Element;
        ArraySize = newLength;
    }

    void Append( T const & _Element )
    {
        Resize( ArraySize + 1 );
        ArrayData[ ArraySize - 1 ] = _Element;
    }

    void Append( TPodVector< T, BASE_CAPACITY, GRANULARITY, Allocator > const & _Array )
    {
        Append( _Array.ArrayData, _Array.ArraySize );
    }

    void Append( T const * _Data, int _Size )
    {
        int length = ArraySize;

        Resize( ArraySize + _Size );
        Core::Memcpy( &ArrayData[ length ], _Data, TYPE_SIZEOF * _Size );
    }

    T & Append()
    {
        Resize( ArraySize + 1 );
        return ArrayData[ ArraySize - 1 ];
    }

    void Remove( int _Index )
    {
        AN_ASSERT_( _Index >= 0 && _Index < ArraySize, "TPodVector::Remove: array index is out of bounds" );

        Core::Memmove( ArrayData + _Index, ArrayData + _Index + 1, TYPE_SIZEOF * ( ArraySize - _Index - 1 ) );

        ArraySize--;
    }

    void RemoveLast()
    {
        if ( ArraySize > 0 ) {
            ArraySize--;
        }
    }

    void RemoveDuplicates()
    {
        for ( int i = 0 ; i < ArraySize ; i++ ) {
            for ( int j = ArraySize-1 ; j > i ; j-- ) {

                if ( ArrayData[j] == ArrayData[i] ) {
                    // duplicate found

                    Core::Memmove( ArrayData + j, ArrayData + j + 1, TYPE_SIZEOF * ( ArraySize - j - 1 ) );
                    ArraySize--;
                }
            }
        }
    }

    /** An optimized removing of array element without memory moves. Just swaps last element with removed element. */
    void RemoveSwap( int _Index )
    {
        AN_ASSERT_( _Index >= 0 && _Index < ArraySize, "TPodVector::RemoveSwap: array index is out of bounds" );

        if ( ArraySize > 0 ) {
            ArrayData[ _Index ] = ArrayData[ ArraySize - 1 ];
            ArraySize--;
        }
    }

    /** Remove elements in range [ _FirstIndex ; _LastIndex ) */
    void Remove( int _FirstIndex, int _LastIndex )
    {
        AN_ASSERT_( _FirstIndex >= 0 && _FirstIndex < ArraySize, "TPodVector::Remove: array index is out of bounds" );
        AN_ASSERT_( _LastIndex >= 0 && _LastIndex <= ArraySize, "TPodVector::Remove: array index is out of bounds" );
        AN_ASSERT_( _FirstIndex < _LastIndex, "TPodVector::Remove: invalid order" );

        Core::Memmove( ArrayData + _FirstIndex, ArrayData + _LastIndex, TYPE_SIZEOF * ( ArraySize - _LastIndex ) );
        ArraySize -= _LastIndex - _FirstIndex;
    }

    bool IsEmpty() const
    {
        return ArraySize == 0;
    }

    T & operator[]( const int _Index )
    {
        AN_ASSERT_( _Index >= 0 && _Index < ArraySize, "TPodVector::operator[]" );
        return ArrayData[ _Index ];
    }

    T const & operator[]( const int _Index ) const
    {
        AN_ASSERT_( _Index >= 0 && _Index < ArraySize, "TPodVector::operator[]" );
        return ArrayData[ _Index ];
    }

    T & Last()
    {
        AN_ASSERT_( ArraySize > 0, "TPodVector::Last" );
        return ArrayData[ ArraySize - 1 ];
    }

    T const & Last() const
    {
        AN_ASSERT_( ArraySize > 0, "TPodVector::Last" );
        return ArrayData[ ArraySize - 1 ];
    }

    T & First()
    {
        AN_ASSERT_( ArraySize > 0, "TPodVector::First" );
        return ArrayData[ 0 ];
    }

    T const & First() const
    {
        AN_ASSERT_( ArraySize > 0, "TPodVector::First" );
        return ArrayData[ 0 ];
    }

    Iterator Begin()
    {
        return ArrayData;
    }

    ConstIterator Begin() const
    {
        return ArrayData;
    }

    Iterator End()
    {
        return ArrayData + ArraySize;
    }

    ConstIterator End() const
    {
        return ArrayData + ArraySize;
    }

    /** foreach compatibility */
    Iterator begin() { return Begin(); }

    /** foreach compatibility */
    ConstIterator begin() const { return Begin(); }

    /** foreach compatibility */
    Iterator end() { return End(); }

    /** foreach compatibility */
    ConstIterator end() const { return End(); }

    Iterator Erase( ConstIterator _Iterator )
    {
        AN_ASSERT_( _Iterator >= ArrayData && _Iterator < ArrayData + ArraySize, "TPodVector::Erase" );
        int Index = _Iterator - ArrayData;
        Core::Memmove( ArrayData + Index, ArrayData + Index + 1, TYPE_SIZEOF * ( ArraySize - Index - 1 ) );
        ArraySize--;
        return ArrayData + Index;
    }

    Iterator EraseSwap( ConstIterator _Iterator )
    {
        AN_ASSERT_( _Iterator >= ArrayData && _Iterator < ArrayData + ArraySize, "TPodVector::Erase" );
        int Index = _Iterator - ArrayData;
        ArrayData[ Index ] = ArrayData[ ArraySize - 1 ];
        ArraySize--;
        return ArrayData + Index;
    }

    Iterator Insert( ConstIterator _Iterator, const T & _Element )
    {
        AN_ASSERT_( _Iterator >= ArrayData && _Iterator <= ArrayData + ArraySize, "TPodVector::Insert" );
        int Index = _Iterator - ArrayData;
        Insert( Index, _Element );
        return ArrayData + Index;
    }

    ConstIterator Find( T const & _Element ) const
    {
        return Find( Begin(), End(), _Element );
    }

    ConstIterator Find( ConstIterator _Begin, ConstIterator _End, const T & _Element ) const
    {
        for ( auto It = _Begin ; It != _End ; It++ ) {
            if ( *It == _Element ) {
                return It;
            }
        }
        return End();
    }

    bool IsExist( T const & _Element ) const
    {
        for ( int i = 0 ; i < ArraySize ; i++ ) {
            if ( ArrayData[i] == _Element ) {
                return true;
            }
        }
        return false;
    }

    int IndexOf( T const & _Element ) const
    {
        for ( int i = 0 ; i < ArraySize ; i++ ) {
            if ( ArrayData[i] == _Element ) {
                return i;
            }
        }
        return -1;
    }

    T * ToPtr()
    {
        return ArrayData;
    }

    T const * ToPtr() const
    {
        return ArrayData;
    }

    int Size() const
    {
        return ArraySize;
    }

    int Capacity() const
    {
        return ArrayCapacity;
    }

    TPodVector & operator=( TPodVector const & _Array )
    {
        Set( _Array.ArrayData, _Array.ArraySize );
        return *this;
    }

    TPodVector & operator=( std::initializer_list< T > InitializerList )
    {
        ResizeInvalidate( InitializerList.size() );
        std::copy( InitializerList.begin(), InitializerList.end(), ArrayData );
        return *this;
    }

    void Set( T const * _Data, int _Size )
    {
        ResizeInvalidate( _Size );
        Core::Memcpy( ArrayData, _Data, TYPE_SIZEOF * _Size );
    }

    int GrowCapacity( int _Size ) const
    {
        const int mod = _Size % GRANULARITY;
        return mod ? _Size + GRANULARITY - mod : _Size;
    }

private:
    alignas( 16 ) T StaticData[BASE_CAPACITY];
    T * ArrayData;
    int ArraySize;
    int ArrayCapacity;

    static_assert( std::is_trivial< T >::value, "Expected POD type" );
    //static_assert( IsAligned( sizeof( T ), Alignment ), "Alignment check" );
};

template< typename T, typename Allocator = AZoneAllocator >
using TPodVectorLite = TPodVector< T, 1, 32, Allocator >;

template< typename T, int BASE_CAPACITY = 32, int GRANULARITY = 32 >
using TPodVectorHeap = TPodVector< T, BASE_CAPACITY, GRANULARITY, AHeapAllocator<16> >;
