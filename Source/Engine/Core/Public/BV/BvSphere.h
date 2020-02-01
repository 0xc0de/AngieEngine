﻿/*

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

#include <Core/Public/Plane.h>
#include "BvAxisAlignedBox.h"

struct BvSphere {
    Float3 Center;
    float  Radius;

    BvSphere() = default;
    BvSphere( const float & _Radius );
    BvSphere( const Float3 & _Center, const float & _Radius );

    void operator/=( const float & _Scale );
    void operator*=( const float & _Scale );

    void operator+=( const Float3 & _Vec );
    void operator-=( const Float3 & _Vec );

    BvSphere operator+( const Float3 & _Vec ) const;
    BvSphere operator-( const Float3 & _Vec ) const;

    bool Compare( const BvSphere & _Other ) const;
    bool CompareEps( const BvSphere & _Other, const float & _Epsilon ) const;
    bool operator==( const BvSphere & _Other ) const;
    bool operator!=( const BvSphere & _Other ) const;

    void Clear();

    void AddPoint( const Float3 & _Point );
    void AddSphere( const BvSphere & _Sphere );

    void FromPointsAverage( const Float3 * _Points, int _NumPoints );
    void FromPoints( const Float3 * _Points, int _NumPoints );
    void FromPointsAroundCenter( const Float3 & _Center, const Float3 * _Points, int _NumPoints );
    void FromAxisAlignedBox( const BvAxisAlignedBox & _AxisAlignedBox );

    float Dist( const PlaneF & _Plane ) const;
    EPlaneSide SideOffset( const PlaneF & _Plane, const float & _Epsilon ) const;
    bool ContainsPoint( const Float3 & _Point ) const;
};

AN_FORCEINLINE BvSphere::BvSphere( const float & _Radius ) {
    Center.X = 0;
    Center.Y = 0;
    Center.Z = 0;
    Radius = _Radius;
}

AN_FORCEINLINE BvSphere::BvSphere( const Float3 & _Center, const float & _Radius ) {
    Center = _Center;
    Radius = _Radius;
}

AN_FORCEINLINE void BvSphere::Clear() {
    Center.X = Center.Y = Center.Z = Radius = 0;
}

AN_FORCEINLINE void BvSphere::operator/=( const float & _Scale ) {
    Radius /= _Scale;
}

AN_FORCEINLINE void BvSphere::operator*=( const float & _Scale ) {
    Radius *= _Scale;
}

AN_FORCEINLINE void BvSphere::operator+=( const Float3 & _Vec ) {
    Center += _Vec;
}

AN_FORCEINLINE void BvSphere::operator-=( const Float3 & _Vec ) {
    Center -= _Vec;
}

AN_FORCEINLINE BvSphere BvSphere::operator+( const Float3 & _Vec ) const {
    return BvSphere( Center + _Vec, Radius );
}

AN_FORCEINLINE BvSphere BvSphere::operator-( const Float3 & _Vec ) const {
    return BvSphere( Center - _Vec, Radius );
}

AN_FORCEINLINE bool BvSphere::Compare( const BvSphere & _Other ) const {
    return ( Center.Compare( _Other.Center ) && Math::Compare( Radius, _Other.Radius ) );
}

AN_FORCEINLINE bool BvSphere::CompareEps( const BvSphere & _Other, const float & _Epsilon ) const {
    return Center.CompareEps( _Other.Center, _Epsilon ) && Math::CompareEps( Radius, _Other.Radius, _Epsilon );
}

AN_FORCEINLINE bool BvSphere::operator==( const BvSphere & _Other ) const {
    return Compare( _Other );
}

AN_FORCEINLINE bool BvSphere::operator!=( const BvSphere & _Other ) const {
    return !Compare( _Other );
}

AN_FORCEINLINE void BvSphere::FromPointsAverage( const Float3 * _Points, int _NumPoints ) {
    if ( _NumPoints <= 0 ) {
        return;
    }

    Center = _Points[0];
    for ( int i = 1 ; i < _NumPoints; ++i ) {
        Center += _Points[i];
    }
    Center /= _NumPoints;

    Radius = 0;
    for ( int i = 0 ; i < _NumPoints ; ++i ) {
        const float DistSqr = Center.DistSqr( _Points[i] );
        if ( DistSqr > Radius ) {
            Radius = DistSqr;
        }
    }

    Radius = Math::Sqrt( Radius );
}

AN_FORCEINLINE void BvSphere::FromPoints( const Float3 * _Points, int _NumPoints ) {
    if ( _NumPoints <= 0 ) {
        return;
    }

    // build aabb

    Float3 Mins( _Points[0] ), Maxs( _Points[0] );

    for ( int i = 1 ; i < _NumPoints ; i++ ) {
        const Float3 & v = _Points[i];
        if ( v.X < Mins.X ) { Mins.X = v.X; }
        if ( v.X > Maxs.X ) { Maxs.X = v.X; }
        if ( v.Y < Mins.Y ) { Mins.Y = v.Y; }
        if ( v.Y > Maxs.Y ) { Maxs.Y = v.Y; }
        if ( v.Z < Mins.Z ) { Mins.Z = v.Z; }
        if ( v.Z > Maxs.Z ) { Maxs.Z = v.Z; }
    }

    // get center from aabb
    Center = ( Mins + Maxs ) * 0.5;

    // calc Radius
    Radius = 0;
    for ( int i = 0 ; i < _NumPoints ; ++i ) {
        const float DistSqr = Center.DistSqr( _Points[i] );
        if ( DistSqr > Radius ) {
            Radius = DistSqr;
        }
    }
    Radius = Math::Sqrt( Radius );
}

AN_FORCEINLINE void BvSphere::FromPointsAroundCenter( const Float3 & _Center, const Float3 * _Points, int _NumPoints ) {
    if ( _NumPoints <= 0 ) {
        return;
    }

    Center = _Center;

    // calc radius
    Radius = 0;
    for ( int i = 0 ; i < _NumPoints ; ++i ) {
        const float DistSqr = Center.DistSqr( _Points[i] );
        if ( DistSqr > Radius ) {
            Radius = DistSqr;
        }
    }
    Radius = Math::Sqrt( Radius );
}

AN_FORCEINLINE void BvSphere::FromAxisAlignedBox( const BvAxisAlignedBox & _AxisAlignedBox ) {
    Center = ( _AxisAlignedBox.Maxs + _AxisAlignedBox.Mins ) * 0.5f;
    Radius = Center.Dist( _AxisAlignedBox.Maxs );
}

AN_FORCEINLINE void BvSphere::AddPoint( const Float3 & _Point ) {
    if ( Center.Compare( Float3::Zero() ) && Radius == 0.0f ) {
        Center = _Point;
    } else {
        const Float3 CenterDiff = _Point - Center;
        const float LenSqr = CenterDiff.LengthSqr();
        if ( LenSqr > Radius * Radius ) {
            const float Len = Math::Sqrt( LenSqr );
            Center += CenterDiff * 0.5f * ( 1.0f - Radius / Len );
            Radius += 0.5f * ( Len - Radius );
        }
    }
}

AN_FORCEINLINE void BvSphere::AddSphere( const BvSphere & _Sphere ) {
    if ( Radius == 0.0f ) {
        *this = _Sphere;
    } else {
        const Float3 CenterDiff = Center - _Sphere.Center;
        const float LenSqr = CenterDiff.LengthSqr();
        const float RadiusDiff = Radius - _Sphere.Radius;

        if ( RadiusDiff * RadiusDiff >= LenSqr ) {
            if ( RadiusDiff < 0.0f ) {
                *this = _Sphere;
            }
        } else {
            constexpr float ZeroTolerance = 0.000001f;
            const float Len = Math::Sqrt( LenSqr );
            Center = Len > ZeroTolerance ? _Sphere.Center + CenterDiff * 0.5f * ( Len + RadiusDiff ) / Len : _Sphere.Center;
            Radius = ( Len + _Sphere.Radius + Radius ) * 0.5f;
        }
    }
}

AN_FORCEINLINE float BvSphere::Dist( const PlaneF & _Plane ) const {
    float Dist = _Plane.Dist( Center );
    if ( Dist > Radius ) {
        return Dist - Radius;
    }
    if ( Dist < -Radius ) {
        return Dist + Radius;
    }
    return 0;
}

AN_FORCEINLINE EPlaneSide BvSphere::SideOffset( const PlaneF & _Plane, const float & _Epsilon ) const {
    float Dist = _Plane.Dist( Center );
    if ( Dist > Radius + _Epsilon ) {
        return EPlaneSide::Front;
    }
    if ( Dist < -Radius - _Epsilon ) {
        return EPlaneSide::Back;
    }
    return EPlaneSide::On;
}

AN_FORCEINLINE bool BvSphere::ContainsPoint( const Float3 & _Point ) const {
    return Center.DistSqr( _Point ) <= Radius * Radius;
}

struct alignas(16) BvSphereSSE : BvSphere {};
