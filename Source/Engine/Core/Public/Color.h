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

#include "Float.h"

class FColor4 : public Float4 {
public:
    FColor4() = default;
    explicit constexpr FColor4( const float & _Value ) : Float4( _Value ) {}
    constexpr FColor4( const float & _X, const float & _Y, const float & _Z ) : Float4( _X, _Y, _Z, 1.0f ) {}
    constexpr FColor4( const float & _X, const float & _Y, const float & _Z, const float & _W ) : Float4( _X, _Y, _Z, _W ) {}
    FColor4( Float4 const & _Value ) : Float4( _Value ) {}
    FColor4( Float3 const & _Value ) : Float4( _Value.X, _Value.Y, _Value.Z, 1.0f ) {}

    void SwapRGB();

    void SetAlpha( float _Alpha );
    float GetAlpha() const { return W; }

    bool IsTransparent() const { return W < 0.0001f; }

    void SetByte( byte _Red, byte _Green, byte _Blue );
    void SetByte( byte _Red, byte _Green, byte _Blue, byte _Alpha );

    void GetByte( byte & _Red, byte & _Green, byte & _Blue ) const;
    void GetByte( byte & _Red, byte & _Green, byte & _Blue, byte & _Alpha ) const;

    void SetDWord( uint32_t _Color );
    uint32_t GetDWord() const;

    void SetUShort565( unsigned short _565 );
    unsigned short GetUShort565() const;

    void SetYCoCgAlpha( const byte _YCoCgAlpha[4] );
    void GetYCoCgAlpha( byte _YCoCgAlpha[4] ) const;

    void SetYCoCg( const byte _YCoCg[3] );
    void GetYCoCg( byte _YCoCg[3] ) const;

    void SetCoCg_Y( const byte _CoCg_Y[4] );
    void GetCoCg_Y( byte _CoCg_Y[4] ) const;

    void SetHSL( float _Hue, float _Saturation, float _Lightness );
    void GetHSL( float & _Hue, float & _Saturation, float & _Lightness ) const;

    void SetCMYK( float _Cyan, float _Magenta, float _Yellow, float _Key );
    void GetCMYK( float & _Cyan, float & _Magenta, float & _Yellow, float & _Key ) const;

    float GetLuminance() const;

    FColor4 ToLinear() const;
    FColor4 ToSRGB() const;

    Float3 const & GetRGB() const { return *(Float3 *)(this); }

    static FColor4 const & White() {
        static FColor4 color(1.0f);
        return color;
    }

    static FColor4 const & Black() {
        static FColor4 color(0.0f,0.0f,0.0f,1.0f);
        return color;
    }
};

//#define SRGB_GAMMA_APPROX

AN_FORCEINLINE float ConvertToRGB( const float & _sRGB ) {
#ifdef SRGB_GAMMA_APPROX
    return pow( _sRGB, 2.2f );
#else
    if ( _sRGB < 0.0f ) return 0.0f;
    if ( _sRGB > 1.0f ) return 1.0f;
    if ( _sRGB <= 0.04045 ) return _sRGB / 12.92f;
    return pow( ( _sRGB + 0.055f ) / 1.055f, 2.4f );
#endif
}

AN_FORCEINLINE float ConvertToSRGB( const float & _lRGB ) {
#ifdef SRGB_GAMMA_APPROX
    return pow( _lRGB, 1.0f / 2.2f );
#else
    if ( _lRGB < 0.0f ) return 0.0f;
    if ( _lRGB > 1.0f ) return 1.0f;
    if ( _lRGB <= 0.0031308 ) return _lRGB * 12.92f;
    return 1.055f * pow( _lRGB, 1.0f / 2.4f ) - 0.055f;
#endif
}

AN_FORCEINLINE FColor4 FColor4::ToLinear() const {
    return FColor4( ConvertToRGB( X ), ConvertToRGB( Y ), ConvertToRGB( Z ), W );
}

AN_FORCEINLINE FColor4 FColor4::ToSRGB() const {
    return FColor4( ConvertToSRGB( X ), ConvertToSRGB( Y ), ConvertToSRGB( Z ), W );
}

AN_FORCEINLINE void FColor4::SwapRGB() {
    FCore::SwapArgs( X, Z );
}

AN_FORCEINLINE void FColor4::SetAlpha( float _Alpha ) {
    W = FMath::Saturate( _Alpha );
}

AN_FORCEINLINE void FColor4::SetByte( byte _Red, byte _Green, byte _Blue ) {
    constexpr float scale = 1.0f / 255.0f;
    X = _Red * scale;
    Y = _Green * scale;
    Z = _Blue * scale;
}

AN_FORCEINLINE void FColor4::SetByte( byte _Red, byte _Green, byte _Blue, byte _Alpha ) {
    constexpr float scale = 1.0f / 255.0f;
    X = _Red * scale;
    Y = _Green * scale;
    Z = _Blue * scale;
    W = _Alpha * scale;
}

AN_FORCEINLINE void FColor4::GetByte( byte & _Red, byte & _Green, byte & _Blue ) const {
    _Red   = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    _Green = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    _Blue  = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );
}

AN_FORCEINLINE void FColor4::GetByte( byte & _Red, byte & _Green, byte & _Blue, byte & _Alpha ) const {
    _Red   = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    _Green = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    _Blue  = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );
    _Alpha = FMath::Clamp( FMath::ToIntFast( W * 255 ), 0, 255 );
}

AN_FORCEINLINE void FColor4::SetDWord( uint32_t _Color ) {
    const int r = _Color & 0xff;
    const int g = ( _Color >> 8 ) & 0xff;
    const int b = ( _Color >> 16 ) & 0xff;
    const int a = ( _Color >> 24 );

    constexpr float scale = 1.0f / 255.0f;
    X = r * scale;
    Y = g * scale;
    Z = b * scale;
    W = a * scale;
}

AN_FORCEINLINE uint32_t FColor4::GetDWord() const {
    const int r = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    const int g = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    const int b = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );
    const int a = FMath::Clamp( FMath::ToIntFast( W * 255 ), 0, 255 );

    return r | ( g << 8 ) | ( b << 16 ) | ( a << 24 );
}

AN_FORCEINLINE void FColor4::SetUShort565( unsigned short _565 ) {
    constexpr float scale = 1.0f / 255.0f;
    const int r = ( ( _565 >> 8 ) & ( ( ( 1 << ( 8 - 3 ) ) - 1 ) << 3 ) ) | ( ( _565 >> 13 ) & ((1<<3)-1) );
    const int g = ( ( _565 >> 3 ) & ( ( ( 1 << ( 8 - 2 ) ) - 1 ) << 2 ) ) | ( ( _565 >>  9 ) & ((1<<2)-1) );
    const int b = ( ( _565 << 3 ) & ( ( ( 1 << ( 8 - 3 ) ) - 1 ) << 3 ) ) | ( ( _565 >>  2 ) & ((1<<3)-1) );
    X = r * scale;
    Y = g * scale;
    Z = b * scale;
}

AN_FORCEINLINE unsigned short FColor4::GetUShort565() const {
    const int r = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    const int g = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    const int b = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );

    return ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 );
}

AN_FORCEINLINE void FColor4::SetYCoCgAlpha( const byte _YCoCgAlpha[4] ) {
    constexpr float scale = 1.0f / 255.0f;
    const int y  = _YCoCgAlpha[0];
    const int co = _YCoCgAlpha[1] - 128;
    const int cg = _YCoCgAlpha[2] - 128;
    X = FMath::Clamp( y + ( co - cg ), 0, 255 ) * scale;
    Y = FMath::Clamp( y + ( cg ), 0, 255 ) * scale;
    Z = FMath::Clamp( y + ( - co - cg ), 0, 255 ) * scale;
    W = _YCoCgAlpha[3] * scale;
}

AN_FORCEINLINE void FColor4::GetYCoCgAlpha( byte _YCoCgAlpha[4] ) const {
    const int r = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    const int g = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    const int b = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );
    const int a = FMath::Clamp( FMath::ToIntFast( W * 255 ), 0, 255 );

    _YCoCgAlpha[0] = FMath::Clamp( ( ( r + ( g << 1 ) +  b ) + 2 ) >> 2, 0, 255 );
    _YCoCgAlpha[1] = FMath::Clamp( ( ( ( ( r << 1 ) - ( b << 1 ) ) + 2 ) >> 2 ) + 128, 0, 255 );
    _YCoCgAlpha[2] = FMath::Clamp( ( ( ( -r + ( g << 1 ) - b ) + 2 ) >> 2 ) + 128, 0, 255 );
    _YCoCgAlpha[3] = a;
}

AN_FORCEINLINE void FColor4::SetYCoCg( const byte _YCoCg[3] ) {
    constexpr float scale = 1.0f / 255.0f;
    const int y  = _YCoCg[0];
    const int co = _YCoCg[1] - 128;
    const int cg = _YCoCg[2] - 128;
    X = FMath::Clamp( y + ( co - cg ), 0, 255 ) * scale;
    Y = FMath::Clamp( y + ( cg ), 0, 255 ) * scale;
    Z = FMath::Clamp( y + ( - co - cg ), 0, 255 ) * scale;
}

AN_FORCEINLINE void FColor4::GetYCoCg( byte _YCoCg[3] ) const {
    const int r = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    const int g = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    const int b = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );
    _YCoCg[0] = FMath::Clamp( ( ( r + ( g << 1 ) +  b ) + 2 ) >> 2, 0, 255 );
    _YCoCg[1] = FMath::Clamp( ( ( ( ( r << 1 ) - ( b << 1 ) ) + 2 ) >> 2 ) + 128, 0, 255 );
    _YCoCg[2] = FMath::Clamp( ( ( ( -r + ( g << 1 ) - b ) + 2 ) >> 2 ) + 128, 0, 255 );
}

AN_FORCEINLINE void FColor4::SetCoCg_Y( const byte _CoCg_Y[4] ) {
    constexpr float scale = 1.0f / 255.0f;
    const int y  = _CoCg_Y[3];
    const int co = _CoCg_Y[0] - 128;
    const int cg = _CoCg_Y[1] - 128;
    X = FMath::Clamp( y + ( co - cg ), 0, 255 ) * scale;
    Y = FMath::Clamp( y + ( cg ), 0, 255 ) * scale;
    Z = FMath::Clamp( y + ( - co - cg ), 0, 255 ) * scale;
}

AN_FORCEINLINE void FColor4::GetCoCg_Y( byte _CoCg_Y[4] ) const {
    const int r = FMath::Clamp( FMath::ToIntFast( X * 255 ), 0, 255 );
    const int g = FMath::Clamp( FMath::ToIntFast( Y * 255 ), 0, 255 );
    const int b = FMath::Clamp( FMath::ToIntFast( Z * 255 ), 0, 255 );

    _CoCg_Y[0] = FMath::Clamp( ( ( ( ( r << 1 ) - ( b << 1 ) ) + 2 ) >> 2 ) + 128, 0, 255 );
    _CoCg_Y[1] = FMath::Clamp( ( ( ( -r + ( g << 1) - b ) + 2 ) >> 2 ) + 128, 0, 255 );
    _CoCg_Y[2] = 0;
    _CoCg_Y[3] = FMath::Clamp( ( ( r + ( g << 1 ) +  b ) + 2 ) >> 2, 0, 255 );
}

AN_FORCEINLINE void FColor4::SetHSL( float _Hue, float _Saturation, float _Lightness ) {
    _Hue = FMath::Saturate( _Hue );
    _Saturation = FMath::Saturate( _Saturation );
    _Lightness = FMath::Saturate( _Lightness );

    const float max = _Lightness;
    const float min = ( 1.0f - _Saturation ) * _Lightness;

    const float f = max - min;

    if ( _Hue >= 0.0f && _Hue <= 1.0f / 6.0f ) {
        X = max;
        Y = FMath::Saturate( min + _Hue * f * 6.0f );
        Z = min;
        return;
    }

    if ( _Hue <= 1.0f / 3.0f ) {
        X = FMath::Saturate( max - ( _Hue - 1.0f / 6.0f ) * f * 6.0f );
        Y = max;
        Z = min;
        return;
    }

    if ( _Hue <= 0.5f ) {
        X = min;
        Y = max;
        Z = FMath::Saturate( min + ( _Hue - 1.0f / 3.0f ) * f * 6.0f );
        return;
    }

    if ( _Hue <= 2.0f / 3.0f ) {
        X = min;
        Y = FMath::Saturate( max - ( _Hue - 0.5f ) * f * 6.0f );
        Z = max;
        return;
    }

    if ( _Hue <= 5.0f / 6.0f ) {
        X = FMath::Saturate( min + ( _Hue - 2.0f / 3.0f ) * f * 6.0f );
        Y = min;
        Z = max;
        return;
    }

    if ( _Hue <= 1.0f ) {
        X = max;
        Y = min;
        Z = FMath::Saturate( max - ( _Hue - 5.0f / 6.0f ) * f * 6.0f );
        return;
    }

    X = Y = Z = 0.0f;
}

AN_FORCEINLINE void FColor4::GetHSL( float & _Hue, float & _Saturation, float & _Lightness ) const {
    float maxComponent, minComponent;

    const float r = FMath::Saturate(X) * 255.0f;
    const float g = FMath::Saturate(Y) * 255.0f;
    const float b = FMath::Saturate(Z) * 255.0f;

    FMath::MinMax( r, g, b, minComponent, maxComponent );

    const float dist = maxComponent - minComponent;

    const float f = ( dist == 0.0f ) ? 0.0f : 60.0f / dist;

    if ( maxComponent == r ) {
        // R 360
        if ( g < b ) {
            _Hue = ( 360.0f + f * ( g - b ) ) / 360.0f;
        } else {
            _Hue = (          f * ( g - b ) ) / 360.0f;
        }
    } else if ( maxComponent == g ) {
        // G 120 degrees
        _Hue = ( 120.0f + f * ( b - r ) ) / 360.0f;
    } else if ( maxComponent == b ) {
        // B 240 degrees
        _Hue = ( 240.0f + f * ( r - g ) ) / 360.0f;
    } else {
        _Hue = 0.0f;
    }

    _Hue = FMath::Saturate( _Hue );

    _Saturation = maxComponent == 0.0f ? 0.0f : dist / maxComponent;
    _Lightness = maxComponent / 255.0f;
}

AN_FORCEINLINE void FColor4::SetCMYK( float _Cyan, float _Magenta, float _Yellow, float _Key ) {
    const float scale = 1.0f - FMath::Saturate( _Key );
    X = ( 1.0f - FMath::Saturate( _Cyan ) ) * scale;
    Y = ( 1.0f - FMath::Saturate( _Magenta ) ) * scale;
    Z = ( 1.0f - FMath::Saturate( _Yellow ) ) * scale;
}

AN_FORCEINLINE void FColor4::GetCMYK( float & _Cyan, float & _Magenta, float & _Yellow, float & _Key ) const {
    const float r = FMath::Saturate( X );
    const float g = FMath::Saturate( Y );
    const float b = FMath::Saturate( Z );
    const float maxComponent = FMath::Max( r, g, b );
    const float scale = maxComponent > 0.0f ? 1.0f / maxComponent : 0.0f;

    _Cyan    = ( maxComponent - r ) * scale;
    _Magenta = ( maxComponent - g ) * scale;
    _Yellow  = ( maxComponent - b ) * scale;
    _Key     = 1.0f - maxComponent;
}

AN_FORCEINLINE float FColor4::GetLuminance() const {
    // assume color is in linear space
    return X * 0.2126f + Y * 0.7152f + Z * 0.0722f;
}
