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

#include <Core/Public/Alloc.h>
#include <Core/Public/Hash.h>
#include <Core/Public/HashFunc.h>
#include <Core/Public/String.h>
#include <Core/Public/CoreMath.h>
#include <Core/Public/PodArray.h>
#include <Core/Public/Ref.h>

class AClassMeta;
class AAttributeMeta;
//class APrecacheMeta;
class ADummy;

class ANGIE_API AObjectFactory
{
    AN_FORBID_COPY( AObjectFactory )

    friend class AClassMeta;
    friend void InitializeFactories();
    friend void DeinitializeFactories();

public:
    AObjectFactory( const char * _Tag );
    ~AObjectFactory();

    const char * GetTag() const { return Tag; }

    ADummy * CreateInstance( const char * _ClassName ) const;
    ADummy * CreateInstance( uint64_t _ClassId ) const;

    template< typename T >
    T * CreateInstance() const { return static_cast< T * >( CreateInstance( T::ClassId() ) ); }

    AClassMeta const * GetClassList() const;

    AClassMeta const * FindClass( const char * _ClassName ) const;

    AClassMeta const * LookupClass( const char * _ClassName ) const;
    AClassMeta const * LookupClass( uint64_t _ClassId ) const;

    uint64_t FactoryClassCount() const { return NumClasses; }

    static AObjectFactory const * Factories() { return FactoryList; }
    AObjectFactory const * Next() const { return NextFactory; }

private:
    const char * Tag;
    AClassMeta * Classes;
    mutable AClassMeta ** IdTable;
    mutable THash<> NameTable;
    uint64_t NumClasses;
    AObjectFactory * NextFactory;
    static AObjectFactory * FactoryList;
};

class ANGIE_API AClassMeta
{
    AN_FORBID_COPY( AClassMeta )

    friend class AObjectFactory;
    friend class AAttributeMeta;
    //friend class APrecacheMeta;

public:
    const uint64_t ClassId;

    const char * GetName() const { return ClassName; }
    uint64_t GetId() const { return ClassId; }
    AClassMeta const * SuperClass() const { return pSuperClass; }
    AClassMeta const * Next() const { return pNext; }
    AObjectFactory const * Factory() const { return pFactory; }
    AAttributeMeta const * GetAttribList() const { return AttributesHead; }
    //APrecacheMeta const * GetPrecacheList() const { return PrecacheHead; }

    bool IsSubclassOf( AClassMeta const & _Superclass ) const
    {
        for ( AClassMeta const * meta = this ; meta ; meta = meta->SuperClass() ) {
            if ( meta->GetId() == _Superclass.GetId() ) {
                return true;
            }
        }
        return false;
    }

    template< typename _Superclass >
    bool IsSubclassOf() const
    {
        return IsSubclassOf( _Superclass::ClassMeta() );
    }

    // TODO: class flags?

    virtual ADummy * CreateInstance() const = 0;
    virtual void DestroyInstance( ADummy * _Object ) const = 0;

    static void CloneAttributes( ADummy const * _Template, ADummy * _Destination );

    static AObjectFactory & DummyFactory() { static AObjectFactory ObjectFactory( "Dummy factory" ); return ObjectFactory; }

    // Utilites
    AAttributeMeta const * FindAttribute( const char * _Name, bool _Recursive ) const;
    void GetAttributes( TPodArray< AAttributeMeta const * > & _Attributes, bool _Recursive = true ) const;

protected:
    AClassMeta( AObjectFactory & _Factory, const char * _ClassName, AClassMeta const * _SuperClassMeta )
        : ClassId( _Factory.NumClasses + 1 ), ClassName( _ClassName )
    {
        AN_ASSERT_( _Factory.FindClass( _ClassName ) == NULL, "Class already defined" );
        pNext = _Factory.Classes;
        pSuperClass = _SuperClassMeta;
        AttributesHead = nullptr;
        AttributesTail = nullptr;
        //PrecacheHead = nullptr;
        //PrecacheTail = nullptr;
        _Factory.Classes = this;
        _Factory.NumClasses++;
        pFactory = &_Factory;
    }

private:
    static void CloneAttributes_r( AClassMeta const * Meta, ADummy const * _Template, ADummy * _Destination );

    const char * ClassName;
    AClassMeta * pNext;
    AClassMeta const * pSuperClass;
    AObjectFactory const * pFactory;
    AAttributeMeta const * AttributesHead;
    AAttributeMeta const * AttributesTail;
    //APrecacheMeta const * PrecacheHead;
    //APrecacheMeta const * PrecacheTail;
};

AN_FORCEINLINE ADummy * AObjectFactory::CreateInstance( const char * _ClassName ) const
{
    AClassMeta const * classMeta = LookupClass( _ClassName );
    return classMeta ? classMeta->CreateInstance() : nullptr;
}

AN_FORCEINLINE ADummy * AObjectFactory::CreateInstance( uint64_t _ClassId ) const
{
    AClassMeta const * classMeta = LookupClass( _ClassId );
    return classMeta ? classMeta->CreateInstance() : nullptr;
}

AN_FORCEINLINE AClassMeta const * AObjectFactory::GetClassList() const
{
    return Classes;
}

class ANGIE_API AAttributeMeta
{
    AN_FORBID_COPY( AAttributeMeta )

public:
    const char * GetName() const { return Name.c_str(); }
    int GetNameHash() const { return NameHash; }
    uint32_t GetFlags() const { return Flags; }

    AAttributeMeta const * Next() const { return pNext; }
    AAttributeMeta const * Prev() const { return pPrev; }

    AAttributeMeta( AClassMeta const & _ClassMeta, const char * _Name, /*EAttributeType _Type, */uint32_t _Flags )
        : Name( std::string( _ClassMeta.GetName() ) + "." +  _Name )
        , NameHash( Core::Hash( Name.c_str(), Name.length() ) )
        , Flags( _Flags )
    {
        AClassMeta & classMeta = const_cast< AClassMeta & >( _ClassMeta );
        pNext = nullptr;
        pPrev = classMeta.AttributesTail;
        if ( pPrev ) {
            const_cast< AAttributeMeta * >( pPrev )->pNext = this;
        } else {
            classMeta.AttributesHead = this;
        }
        classMeta.AttributesTail = this;
    }

    // TODO: Min/Max range for integer or float attributes, support for enums

    void SetValue( ADummy * _Object, AString const & _Value ) const {
        FromString( _Object, _Value );
    }

    void GetValue( ADummy * _Object, AString & _Value ) const {
        ToString( _Object, _Value );
    }

    void CopyValue( ADummy const * _Src, ADummy * _Dst ) const {
        Copy( _Src, _Dst );
    }

    //void SetSubmember( ADummy * _Object, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes ) const {
    //    SetSubmemberCB( _Object, AttributeHash, Attributes );
    //}

private:
    std::string Name;
    int NameHash;
    AAttributeMeta const * pNext;
    AAttributeMeta const * pPrev;
    uint32_t Flags;

protected:
    TStdFunction< void( ADummy *, AString const & ) > FromString;
    TStdFunction< void( ADummy *, AString & ) > ToString;
    TStdFunction< void( ADummy const *, ADummy * ) > Copy;
    //TStdFunction< void( ADummy *, THash<> const &, TStdVector< std::pair< AString, AString > > const & ) > SetSubmemberCB;
};

#if 0
class ANGIE_API APrecacheMeta
{
    AN_FORBID_COPY( APrecacheMeta )

public:
    AClassMeta const & GetResourceClassMeta() const { return ResourceClassMeta; }
    const char * GetResourcePath() const { return Path; }
    int GetResourceHash() const { return Hash; }

    APrecacheMeta const * Next() const { return pNext; }
    APrecacheMeta const * Prev() const { return pPrev; }

    APrecacheMeta( AClassMeta const & _ClassMeta, AClassMeta const & _ResourceClassMeta, const char * _Path )
        : ResourceClassMeta( _ResourceClassMeta )
        , Path( _Path )
        , Hash( Core::HashCase( _Path, Core::Strlen( _Path ) ) )
    {
        AClassMeta & classMeta = const_cast< AClassMeta & >( _ClassMeta );
        pNext = nullptr;
        pPrev = classMeta.PrecacheTail;
        if ( pPrev ) {
            const_cast< APrecacheMeta * >( pPrev )->pNext = this;
        } else {
            classMeta.PrecacheHead = this;
        }
        classMeta.PrecacheTail = this;
    }

private:
    AClassMeta const & ResourceClassMeta;
    const char * Path;
    int Hash;
    APrecacheMeta const * pNext;
    APrecacheMeta const * pPrev;
};
#endif

template< typename AttributeType >
void SetAttributeFromString( AttributeType & Attribute, AString const & String );

//template< typename AttributeType >
//void SetAttributeSubmember( AttributeType & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes );

AN_FORCEINLINE void SetAttributeFromString( uint8_t & Attribute, AString const & String )
{
    Attribute = Math::ToInt< uint8_t >( String );
}

AN_FORCEINLINE void SetAttributeFromString( bool & Attribute, AString const & String )
{
    Attribute = !!Math::ToInt< uint8_t >( String );
}

AN_FORCEINLINE void SetAttributeFromString( int32_t & Attribute, AString const & String )
{
    Attribute = Math::ToInt< int32_t >( String );
}

AN_FORCEINLINE void SetAttributeFromString( float & Attribute, AString const & String )
{
    uint32_t i = Math::ToInt< uint32_t >( String );
    Attribute = *(float *)&i;
}

AN_FORCEINLINE void SetAttributeFromString( Float2 & Attribute, AString const & String )
{
    uint32_t tmp[2];
    sscanf( String.CStr(), "%d %d", &tmp[0], &tmp[1] );
    for ( int i = 0 ; i < 2 ; i++ ) {
        Attribute[i] = *(float *)&tmp[i];
    }
}

AN_FORCEINLINE void SetAttributeFromString( Float3 & Attribute, AString const & String )
{
    uint32_t tmp[3];
    sscanf( String.CStr(), "%d %d %d", &tmp[0], &tmp[1], &tmp[2] );
    for ( int i = 0 ; i < 3 ; i++ ) {
        Attribute[i] = *(float *)&tmp[i];
    }
}

AN_FORCEINLINE void SetAttributeFromString( Float4 & Attribute, AString const & String )
{
    uint32_t tmp[4];
    sscanf( String.CStr(), "%d %d %d %d", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
    for ( int i = 0 ; i < 4 ; i++ ) {
        Attribute[i] = *(float *)&tmp[i];
    }
}

AN_FORCEINLINE void SetAttributeFromString( Quat & Attribute, AString const & String )
{
    uint32_t tmp[4];
    sscanf( String.CStr(), "%d %d %d %d", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
    for ( int i = 0 ; i < 4 ; i++ ) {
        Attribute[i] = *(float *)&tmp[i];
    }
}

AN_FORCEINLINE void SetAttributeFromString( AString & Attribute, AString const & String )
{
    Attribute = String;
}

//AN_FORCEINLINE void SetAttributeSubmember( uint8_t & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( bool & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( int32_t & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( float & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( Float2 & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( Float3 & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( Float4 & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( Quat & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}
//AN_FORCEINLINE void SetAttributeSubmember( AString & Attribute, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
//{}

template< typename AttributeType >
void SetAttributeToString( AttributeType const & Attribute, AString & String );

AN_FORCEINLINE void SetAttributeToString( uint8_t const & Attribute, AString & String )
{
    String = Math::ToString( Attribute );
}

AN_FORCEINLINE void SetAttributeToString( bool const & Attribute, AString & String )
{
    String = Math::ToString( Attribute );
}

AN_FORCEINLINE void SetAttributeToString( int32_t const & Attribute, AString & String )
{
    String = Math::ToString( Attribute );
}

AN_FORCEINLINE void SetAttributeToString( float const & Attribute, AString & String )
{
    String = Math::ToString( *((uint32_t *)&Attribute) );
}

AN_FORCEINLINE void SetAttributeToString( Float2 const & Attribute, AString & String )
{
    String = Math::ToString( *((uint32_t *)&Attribute.X) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Y) );;
}

AN_FORCEINLINE void SetAttributeToString( Float3 const & Attribute, AString & String )
{
    String = Math::ToString( *((uint32_t *)&Attribute.X) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Y) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Z) );
}

AN_FORCEINLINE void SetAttributeToString( Float4 const & Attribute, AString & String )
{
    String = Math::ToString( *((uint32_t *)&Attribute.X) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Y) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Z) ) + " " + Math::ToString( *((uint32_t *)&Attribute.W) );
}

AN_FORCEINLINE void SetAttributeToString( Quat const & Attribute, AString & String )
{
    String = Math::ToString( *((uint32_t *)&Attribute.X) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Y) ) + " " + Math::ToString( *((uint32_t *)&Attribute.Z) ) + " " + Math::ToString( *((uint32_t *)&Attribute.W) );
}

AN_FORCEINLINE void SetAttributeToString( AString const & Attribute, AString & String )
{
    String = Attribute;
}

template< typename ObjectType >
class TAttributeMeta : public AAttributeMeta
{
    AN_FORBID_COPY( TAttributeMeta )

public:
    template< typename AttributeType >
    TAttributeMeta( AClassMeta const & _ClassMeta, const char * _Name,
                    void(ObjectType::*_Setter)( AttributeType ),
                    AttributeType(ObjectType::*_Getter)() const,
                    int _Flags )
        : AAttributeMeta( _ClassMeta, _Name, /*GetAttributeType< AttributeType >(),*/ _Flags )
    {
//        Setter = [_Setter]( ADummy * _Object, const void * _DataPtr ) {
//            ObjectType * object = static_cast< ObjectType * >( _Object );
//            (*object.*_Setter)( *(( AttributeType const * )_DataPtr) );
//        };
//        Getter = [_Getter]( ADummy * _Object, void * _DataPtr ) {
//            ObjectType * object = static_cast< ObjectType * >( _Object );
//            AttributeType Value = (*object.*_Getter)();
//            Core::Memcpy( ( AttributeType * )_DataPtr, &Value, sizeof( Value ) );
//        };

        FromString = [_Setter]( ADummy * _Object, AString const & _Value )
        {
            AttributeType attribute;

            SetAttributeFromString( attribute, _Value );

            (*static_cast< ObjectType * >( _Object ).*_Setter)( attribute );
        };

        ToString = [_Getter]( ADummy * _Object, AString & _Value )
        {
            SetAttributeToString( (*static_cast< ObjectType * >(_Object).*_Getter)(), _Value );
        };

        Copy = [_Setter,_Getter]( ADummy const * _Src, ADummy * _Dst )
        {
            (*static_cast< ObjectType * >( _Dst ).*_Setter)( (*static_cast< ObjectType const * >( _Src ).*_Getter)() );
        };

//        SetSubmemberCB = [_Setter]( ADummy * _Object, AString const & _Value )
//        {
//            AttributeType attribute;

//            SetAttributeFromString( attribute, _Value );

//            (*static_cast< ObjectType * >( _Object ).*_Setter)( attribute );
//        };
    }

    template< typename AttributeType >
    TAttributeMeta( AClassMeta const & _ClassMeta, const char * _Name, AttributeType * _AttribPointer, int _Flags )
        : AAttributeMeta( _ClassMeta, _Name, /*GetAttributeType< AttributeType >(),*/ _Flags )
    {
        FromString = [_AttribPointer]( ADummy * _Object, AString const & _Value )
        {
            SetAttributeFromString( *(AttributeType *)((byte *)static_cast< ObjectType * >(_Object) + (size_t)_AttribPointer), _Value );
        };

        ToString = [_AttribPointer]( ADummy * _Object, AString & _Value )
        {
            SetAttributeToString( *(AttributeType *)((byte *)static_cast< ObjectType * >(_Object) + (size_t)_AttribPointer), _Value );
        };

        Copy = [_AttribPointer]( ADummy const * _Src, ADummy * _Dst )
        {
            *(AttributeType *)((byte *)static_cast<ObjectType *>(_Dst) + (size_t)_AttribPointer) = *(AttributeType const *)((byte const *)static_cast<ObjectType const *>(_Src) + (size_t)_AttribPointer);
        };

        //SetSubmemberCB = [_AttribPointer]( ADummy * _Object, THash<> const & AttributeHash, TStdVector< std::pair< AString, AString > > const & Attributes )
        //{
        //    SetAttributeSubmember( *(AttributeType *)((byte *)static_cast< ObjectType * >(_Object) + (size_t)_AttribPointer), AttributeHash, Attributes );
        //};
    }
};

#define _AN_GENERATED_CLASS_BODY() \
public: \
    static AClassMeta const & ClassMeta() { \
        static ThisClassMeta __Meta; \
        return __Meta; \
    }\
    static AClassMeta const * SuperClass() { \
        return ClassMeta().SuperClass(); \
    } \
    static const char * ClassName() { \
        return ClassMeta().GetName(); \
    } \
    static uint64_t ClassId() { \
        return ClassMeta().GetId(); \
    } \
    virtual AClassMeta const & FinalClassMeta() const { return ClassMeta(); } \
    virtual const char * FinalClassName() const { return ClassName(); } \
    virtual uint64_t FinalClassId() const { return ClassId(); }


#define AN_CLASS( _Class, _SuperClass ) \
    AN_FACTORY_CLASS( AClassMeta::DummyFactory(), _Class, _SuperClass )

#define AN_FACTORY_CLASS( _Factory, _Class, _SuperClass ) \
    AN_FACTORY_CLASS_A( _Factory, _Class, _SuperClass, ADummy::Allocator )

#define AN_FACTORY_CLASS_A( _Factory, _Class, _SuperClass, _Allocator ) \
    AN_FORBID_COPY( _Class ) \
    friend class ADummy; \
public:\
    typedef _SuperClass Super; \
    typedef _Class ThisClass; \
    typedef _Allocator Allocator; \
    class ThisClassMeta : public AClassMeta { \
    public: \
        ThisClassMeta() : AClassMeta( _Factory, AN_STRINGIFY( _Class ), &Super::ClassMeta() ) { \
            RegisterAttributes(); \
        } \
        ADummy * CreateInstance() const override { \
            return NewObject< ThisClass >(); \
        } \
        void DestroyInstance( ADummy * _Object ) const override { \
            _Object->~ADummy(); \
            _Allocator::Inst().Free( _Object ); \
        } \
    private: \
        void RegisterAttributes(); \
    }; \
    _AN_GENERATED_CLASS_BODY() \
private:



#define AN_BEGIN_CLASS_META( _Class ) \
AClassMeta const & _Class##__Meta = _Class::ClassMeta(); \
void _Class::ThisClassMeta::RegisterAttributes() {

#define AN_END_CLASS_META() \
    }

#define AN_CLASS_META( _Class ) AN_BEGIN_CLASS_META( _Class ) AN_END_CLASS_META()

#define AN_ATTRIBUTE( _Name, _Setter, _Getter, _Flags ) \
    static TAttributeMeta< ThisClass > const _Name##Meta( *this, AN_STRINGIFY( _Name ), &ThisClass::_Setter, &ThisClass::_Getter, _Flags );

#define AN_ATTRIBUTE_( _Name, _Flags ) \
    static TAttributeMeta< ThisClass > const _Name##Meta( *this, AN_STRINGIFY( _Name ), (&(( ThisClass * )0)->_Name), _Flags );

//#define AN_PRECACHE( _ResourceClass, _ResourceName, _Path ) static APrecacheMeta const _ResourceName##Precache( *this, _ResourceClass::ClassMeta(), _Path );

/* Attribute flags */
#define AF_DEFAULT              0
#define AF_NON_SERIALIZABLE     1

/*

ADummy

Base factory object class.
Needs to resolve class meta data.

*/
class ANGIE_API ADummy
{
    AN_FORBID_COPY( ADummy )

public:
    typedef ADummy ThisClass;
    typedef AZoneAllocator Allocator;
    class ThisClassMeta : public AClassMeta
    {
    public:
        ThisClassMeta() : AClassMeta( AClassMeta::DummyFactory(), "ADummy", nullptr )
        {
        }
        ADummy * CreateInstance() const override
        {
            return NewObject< ThisClass >();
        }
        void DestroyInstance( ADummy * _Object ) const override
        {
            _Object->~ADummy();
            Allocator::Inst().Free( _Object );
        }
    private:
        void RegisterAttributes();
    };
    template< typename T >
    static T * NewObject()
    {
        void * data = T::Allocator::Inst().ClearedAlloc( sizeof( T ) );
        ADummy * object = new (data) T;
        return static_cast< T * >( object );
    }
    virtual ~ADummy() {}
protected:
    ADummy() {}
    _AN_GENERATED_CLASS_BODY()
};

template< typename T > T * CreateInstanceOf()
{
    return static_cast< T * >( T::ClassMeta().CreateInstance() );
}

template< typename T >
T * Upcast( ADummy * _Object )
{
    if ( _Object && _Object->FinalClassMeta().IsSubclassOf< T >() ) {
        return static_cast< T * >( _Object );
    }
    return nullptr;
}

void InitializeFactories();
void DeinitializeFactories();
