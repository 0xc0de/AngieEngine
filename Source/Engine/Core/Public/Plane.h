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

#include "Float.h"

enum class EPlaneSide {
    Back = -1,
    Front = 1,
    On = 0,
    Cross = 2
};

struct PlaneD;

// Plane equalation: Normal.X * X + Normal.Y * Y + Normal.Z * Z + D = 0
struct PlaneF /*final*/ {
    Float3  Normal;
    float   D;

    PlaneF() = default;
    PlaneF( Float3 const & _P0, Float3 const & _P1, Float3 const & _P2 );
    PlaneF( float const & _A, float const & _B, float const & _C, const float & _D );
    PlaneF( Float3 const & _Normal, float const & _Dist );
    PlaneF( Float3 const & _Normal, Float3 const & _PointOnPlane );
    explicit PlaneF( PlaneD const & _Plane );

    float * ToPtr() {
        return &Normal.X;
    }

    float const * ToPtr() const {
        return &Normal.X;
    }

    PlaneF operator-() const;
    PlaneF & operator=( const PlaneF & p );

    bool operator==( PlaneF const & _Other ) const { return Compare( _Other ); }
    bool operator!=( PlaneF const & _Other ) const { return !Compare( _Other ); }

    bool Compare( PlaneF const & _Other ) const {
        return ToVec4() == _Other.ToVec4();
    }

    bool CompareEps( const PlaneF & _Other, float const & _NormalEpsilon, float const & _DistEpsilon ) const {
        return Bool4( Math::Dist( Normal.X, _Other.Normal.X ) < _NormalEpsilon,
                      Math::Dist( Normal.Y, _Other.Normal.Y ) < _NormalEpsilon,
                      Math::Dist( Normal.Z, _Other.Normal.Z ) < _NormalEpsilon,
                      Math::Dist( D, _Other.D ) < _DistEpsilon ).All();
    }

    void Clear();

    float Dist() const;

    void SetDist( const float & _Dist );

    int AxialType() const;

    int PositiveAxialType() const;

    int SignBits() const;

    void FromPoints( Float3 const & _P0, Float3 const & _P1, Float3 const & _P2 );
    void FromPoints( Float3 const _Points[3] );

    float Dist( Float3 const & _Point ) const;

    EPlaneSide SideOffset( Float3 const & _Point, float const & _Epsilon ) const;

    void NormalizeSelf() {
        const float NormalLength = Normal.Length();
        if ( NormalLength != 0.0f ) {
            const float OneOverLength = 1.0f / NormalLength;
            Normal *= OneOverLength;
            D *= OneOverLength;
        }
    }

    PlaneF Normalized() const {
        const float NormalLength = Normal.Length();
        if ( NormalLength != 0.0f ) {
            const float OneOverLength = 1.0f / NormalLength;
            return PlaneF( Normal * OneOverLength, D * OneOverLength );
        }
        return *this;
    }

    PlaneF Snap( const float & _NormalEpsilon, const float & _DistEpsilon ) const {
        PlaneF SnapPlane( Normal.SnapNormal( _NormalEpsilon ), D );
        const float SnapD = Math::Round( D );
        if ( Math::Abs( D - SnapD ) < _DistEpsilon ) {
            SnapPlane.D = SnapD;
        }
        return SnapPlane;
    }

    //Float3 ProjectPoint( const Float3 & _Point ) const;

    Float4 & ToVec4() { return *reinterpret_cast< Float4 * >( this ); }
    Float4 const & ToVec4() const { return *reinterpret_cast< const Float4 * >( this ); }

    // String conversions
    AString ToString( int _Precision = - 1 ) const {
        return AString( "( " ) + Math::ToString( Normal.X, _Precision ) + " " + Math::ToString( Normal.Y, _Precision ) + " " + Math::ToString( Normal.Z, _Precision ) + " " + Math::ToString( D, _Precision ) + " )";
    }

    AString ToHexString( bool _LeadingZeros = false, bool _Prefix = false ) const {
        return AString( "( " ) + Math::ToHexString( Normal.X, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( Normal.Y, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( Normal.Z, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( D, _LeadingZeros, _Prefix ) + " )";
    }

    // Byte serialization
    void Write( IStreamBase & _Stream ) const {
        Normal.Write( _Stream );
        _Stream.WriteFloat( D );
    }

    void Read( IStreamBase & _Stream ) {
        Normal.Read( _Stream );
        D = _Stream.ReadFloat();
    }
};

AN_FORCEINLINE PlaneF::PlaneF( Float3 const & _P0, Float3 const & _P1, Float3 const & _P2 ) {
    FromPoints( _P0, _P1, _P2 );
}

AN_FORCEINLINE PlaneF::PlaneF( float const & _A, float const & _B, float const & _C, float const & _D )
    : Normal( _A, _B, _C ), D( _D )
{
}

AN_FORCEINLINE PlaneF::PlaneF( Float3 const & _Normal, float const & _Dist )
    : Normal( _Normal ), D( -_Dist )
{
}

AN_FORCEINLINE PlaneF::PlaneF( Float3 const & _Normal, Float3 const & _PointOnPlane )
    : Normal( _Normal ), D( -Math::Dot( _PointOnPlane, _Normal ) )
{
}

AN_FORCEINLINE void PlaneF::Clear() {
    Normal.X = Normal.Y = Normal.Z = D = 0;
}

AN_FORCEINLINE float PlaneF::Dist() const {
    return -D;
}

AN_FORCEINLINE void PlaneF::SetDist( const float & _Dist ) {
    D = -_Dist;
}

AN_FORCEINLINE int PlaneF::AxialType() const {
    return Normal.NormalAxialType();
}

AN_FORCEINLINE int PlaneF::PositiveAxialType() const {
    return Normal.NormalPositiveAxialType();
}

AN_FORCEINLINE int PlaneF::SignBits() const {
    return Normal.SignBits();
}

AN_FORCEINLINE PlaneF & PlaneF::operator=( PlaneF const & _Other ) {
    Normal = _Other.Normal;
    D = _Other.D;
    return *this;
}

AN_FORCEINLINE PlaneF PlaneF::operator-() const {
    return PlaneF( -Normal, D );
}

AN_FORCEINLINE void PlaneF::FromPoints( Float3 const & _P0, Float3 const & _P1, Float3 const & _P2 ) {
    Normal = Math::Cross( _P0 - _P1, _P2 - _P1 ).Normalized();
    D = -Math::Dot( Normal, _P1 );
}

AN_FORCEINLINE void PlaneF::FromPoints( Float3 const _Points[3] ) {
    FromPoints( _Points[0], _Points[1], _Points[2] );
}

AN_FORCEINLINE float PlaneF::Dist( Float3 const & _Point ) const {
    return Math::Dot( _Point, Normal ) + D;
}

AN_FORCEINLINE EPlaneSide PlaneF::SideOffset( Float3 const & _Point, float const & _Epsilon ) const {
    const float Distance = Dist( _Point );
    if ( Distance > _Epsilon ) {
        return EPlaneSide::Front;
    }
    if ( Distance < -_Epsilon ) {
        return EPlaneSide::Back;
    }
    return EPlaneSide::On;
}

//// Assumes Plane.D == 0
//AN_FORCEINLINE Float3 PlaneF::ProjectPoint( Float3 const & _Point ) const {
//    const float OneOverLengthSqr = 1.0f / Normal.LengthSqr();
//    const Float3 N = Normal * OneOverLengthSqr;
//    return _Point - ( Math::Dot( Normal, _Point ) * OneOverLengthSqr ) * N;
//}

//namespace Math {

//// Assumes Plane.D == 0
//AN_FORCEINLINE Float3 ProjectPointOnPlane( Float3 const & _Point, Float3 const & _Normal ) {
//    const float OneOverLengthSqr = 1.0f / _Normal.LengthSqr();
//    const Float3 N = _Normal * OneOverLengthSqr;
//    return _Point - ( Math::Dot( _Normal, _Point ) * OneOverLengthSqr ) * N;
//}

//}


struct PlaneD /*final*/ {
    Double3 Normal;
    double  D;

    PlaneD() = default;
    PlaneD( Double3 const & _P0, Double3 const & _P1, Double3 const & _P2 );
    PlaneD( double const & _A, double const & _B, double const & _C, double const & _D );
    PlaneD( Double3 const & _Normal, double const  & _Dist );
    PlaneD( Double3 const & _Normal, Double3 const & _PointOnPlane );
    explicit PlaneD( PlaneF const & _Plane );

    double * ToPtr() {
        return &Normal.X;
    }

    double const * ToPtr() const {
        return &Normal.X;
    }

    PlaneD operator-() const;
    PlaneD & operator=( PlaneD const & p );

    bool operator==( PlaneD const & _Other ) const { return Compare( _Other ); }
    bool operator!=( PlaneD const & _Other ) const { return !Compare( _Other ); }

    bool Compare( PlaneD const & _Other ) const {
        return ToVec4() == _Other.ToVec4();
    }

    bool CompareEps( PlaneD const & _Other, double const & _NormalEpsilon, double const & _DistEpsilon ) const {
        return Bool4( Math::Dist( Normal.X, _Other.Normal.X ) < _NormalEpsilon,
                      Math::Dist( Normal.Y, _Other.Normal.Y ) < _NormalEpsilon,
                      Math::Dist( Normal.Z, _Other.Normal.Z ) < _NormalEpsilon,
                      Math::Dist( D, _Other.D ) < _DistEpsilon ).All();
    }

    void Clear();

    double Dist() const;

    void SetDist( const double & _Dist );

    int AxialType() const;

    int PositiveAxialType() const;

    int SignBits() const;

    void FromPoints( Double3 const & _P1, Double3 const & _P2, Double3 const & _P3 );
    void FromPoints( Double3 const _Points[3] );

    double Dist( Double3 const & _Point ) const;

    EPlaneSide SideOffset( Double3 const & _Point, double const & _Epsilon ) const;

    void NormalizeSelf() {
        const double NormalLength = Normal.Length();
        if ( NormalLength != 0.0f ) {
            const double OneOverLength = 1.0f / NormalLength;
            Normal *= OneOverLength;
            D *= OneOverLength;
        }
    }

    PlaneD Normalized() const {
        const double NormalLength = Normal.Length();
        if ( NormalLength != 0.0f ) {
            const double OneOverLength = 1.0f / NormalLength;
            return PlaneD( Normal * OneOverLength, D * OneOverLength );
        }
        return *this;
    }

    PlaneD Snap( const double & _NormalEpsilon, const double & _DistEpsilon ) const {
        PlaneD SnapPlane( Normal.SnapNormal( _NormalEpsilon ), D );
        const double SnapD = Math::Round( D );
        if ( Math::Abs( D - SnapD ) < _DistEpsilon ) {
            SnapPlane.D = SnapD;
        }
        return SnapPlane;
    }

    //Double3 ProjectPoint( const Double3 & _Point ) const;

    Double4 & ToVec4() { return *reinterpret_cast< Double4 * >( this ); }
    Double4 const & ToVec4() const { return *reinterpret_cast< const Double4 * >( this ); }

    // String conversions
    AString ToString( int _Precision = - 1 ) const {
        return AString( "( " ) + Math::ToString( Normal.X, _Precision ) + " " + Math::ToString( Normal.Y, _Precision ) + " " + Math::ToString( Normal.Z, _Precision ) + " " + Math::ToString( D, _Precision ) + " )";
    }

    AString ToHexString( bool _LeadingZeros = false, bool _Prefix = false ) const {
        return AString( "( " ) + Math::ToHexString( Normal.X, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( Normal.Y, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( Normal.Z, _LeadingZeros, _Prefix ) + " " + Math::ToHexString( D, _LeadingZeros, _Prefix ) + " )";
    }
};

AN_FORCEINLINE PlaneD::PlaneD( Double3 const & _P0, Double3 const & _P1, Double3 const & _P2 ) {
    FromPoints( _P0, _P1, _P2 );
}

AN_FORCEINLINE PlaneD::PlaneD( double const & _A, double const & _B, double const & _C, double const & _D )
    : Normal( _A, _B, _C ), D( _D )
{
}

AN_FORCEINLINE PlaneD::PlaneD( Double3 const & _Normal, double const & _Dist )
    : Normal( _Normal ), D( -_Dist )
{
}

AN_FORCEINLINE PlaneD::PlaneD( Double3 const & _Normal, Double3 const & _PointOnPlane )
    : Normal( _Normal ), D( -Math::Dot( _PointOnPlane, _Normal ) )
{
}

AN_FORCEINLINE void PlaneD::Clear() {
    Normal.X = Normal.Y = Normal.Z = D = 0;
}

AN_FORCEINLINE double PlaneD::Dist() const {
    return -D;
}

AN_FORCEINLINE void PlaneD::SetDist( const double & _Dist ) {
    D = -_Dist;
}

AN_FORCEINLINE int PlaneD::AxialType() const {
    return Normal.NormalAxialType();
}

AN_FORCEINLINE int PlaneD::PositiveAxialType() const {
    return Normal.NormalPositiveAxialType();
}

AN_FORCEINLINE int PlaneD::SignBits() const {
    return Normal.SignBits();
}

AN_FORCEINLINE PlaneD & PlaneD::operator=( PlaneD const & _Other ) {
    Normal = _Other.Normal;
    D = _Other.D;
    return *this;
}

AN_FORCEINLINE PlaneD PlaneD::operator-() const {
    return PlaneD( -Normal, D );
}

AN_FORCEINLINE void PlaneD::FromPoints( Double3 const & _P0, Double3 const & _P1, Double3 const & _P2 ) {
    Normal = Math::Cross( _P0 - _P1, _P2 - _P1 ).Normalized();
    D = -Math::Dot( Normal, _P1 );
}

AN_FORCEINLINE void PlaneD::FromPoints( Double3 const _Points[3] ) {
    FromPoints( _Points[0], _Points[1], _Points[2] );
}

AN_FORCEINLINE double PlaneD::Dist( Double3 const & _Point ) const {
    return Math::Dot( _Point, Normal ) + D;
}

AN_FORCEINLINE EPlaneSide PlaneD::SideOffset( Double3 const & _Point, double const & _Epsilon ) const {
    const double Distance = Dist( _Point );
    if ( Distance > _Epsilon ) {
        return EPlaneSide::Front;
    }
    if ( Distance < -_Epsilon ) {
        return EPlaneSide::Back;
    }
    return EPlaneSide::On;
}

AN_FORCEINLINE PlaneF::PlaneF( PlaneD const & _Plane ) : Normal( _Plane.Normal ), D( _Plane.D ) {}
AN_FORCEINLINE PlaneD::PlaneD( PlaneF const & _Plane ) : Normal( _Plane.Normal ), D( _Plane.D ) {}
