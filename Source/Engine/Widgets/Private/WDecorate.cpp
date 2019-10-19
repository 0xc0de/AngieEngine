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

#include <Engine/Widgets/Public/WDecorate.h>
#include <Engine/Widgets/Public/WWidget.h>

AN_CLASS_META( WDecorate )

WDecorate::WDecorate() {

}

WDecorate::~WDecorate() {

}

void WDecorate::OnDrawEvent( FCanvas & _Canvas ) {

}


AN_CLASS_META( WTextDecorate )

WTextDecorate::WTextDecorate() {
    Color = FColor4::White();
    bWordWrap = false;
    HorizontalAlignment = WIDGET_ALIGNMENT_NONE;
    VerticalAlignment = WIDGET_ALIGNMENT_NONE;
}

WTextDecorate::~WTextDecorate() {
}

WTextDecorate & WTextDecorate::SetText( const char * _Text ) {
    Text = _Text;
    return *this;
}

WTextDecorate & WTextDecorate::SetFont( FFont * _Font ) {
    Font = _Font;
    return *this;
}

WTextDecorate & WTextDecorate::SetColor( FColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WTextDecorate & WTextDecorate::SetHorizontalAlignment( EWidgetAlignment _Alignment ) {
    HorizontalAlignment = _Alignment;
    return *this;
}

WTextDecorate & WTextDecorate::SetVerticalAlignment( EWidgetAlignment _Alignment ) {
    VerticalAlignment = _Alignment;
    return *this;
}

WTextDecorate & WTextDecorate::SetWordWrap( bool _WordWrap ) {
    bWordWrap = _WordWrap;
    return *this;
}

WTextDecorate & WTextDecorate::SetOffset( Float2 const & _Offset ) {
    Offset = _Offset;
    return *this;
}

FFont const * WTextDecorate::GetFont( FCanvas & _Canvas ) const {
    FFont const * font = Font;

    if ( !font ) {
        // back to default font
        font = _Canvas.GetDefaultFont();
    }

    return font;
}

void WTextDecorate::OnDrawEvent( FCanvas & _Canvas ) {
    Float2 ownerSize = GetOwner()->GetCurrentSize();
    float width = ownerSize.X;
    FFont const * font = GetFont( _Canvas );

    Float2 pos;
    Float2 size = font->CalcTextSizeA( font->GetFontSize(), ownerSize.X, bWordWrap ? ownerSize.X : 0.0f, Text.Begin(), Text.End() );

    if ( HorizontalAlignment == WIDGET_ALIGNMENT_LEFT ) {
        pos.X = 0;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_RIGHT ) {
        pos.X = ownerSize.X - size.X;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.X * 0.5f;
        pos.X = center - size.X * 0.5f;
    } else {
        pos.X = Offset.X;
    }

    if ( VerticalAlignment == WIDGET_ALIGNMENT_TOP ) {
        pos.Y = 0;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_BOTTOM ) {
        pos.Y = ownerSize.Y - size.Y;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.Y * 0.5f;
        pos.Y = center - size.Y * 0.5f;
    } else {
        pos.Y = Offset.Y;
    }

    // WIDGET_ALIGNMENT_STRETCH is not handled for text

    pos += GetOwner()->GetDesktopPosition();

    _Canvas.DrawTextUTF8( font, font->GetFontSize(), pos, Color, Text.Begin(), Text.End(), bWordWrap ? width : 0.0f );
}



AN_CLASS_META( WBorderDecorate )

WBorderDecorate::WBorderDecorate() {
    Color = FColor4::White();
    BgColor = FColor4::Black();
    bFillBackgrond = false;
    Rounding = 0;
    RoundingCorners = CORNER_ROUND_ALL;
    Thickness = 1;
}

WBorderDecorate::~WBorderDecorate() {
}

WBorderDecorate & WBorderDecorate::SetColor( FColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WBorderDecorate & WBorderDecorate::SetFillBackground( bool _FillBackgrond ) {
    bFillBackgrond = _FillBackgrond;
    return *this;
}

WBorderDecorate & WBorderDecorate::SetBackgroundColor( FColor4 const & _Color ) {
    BgColor = _Color;
    return *this;
}

WBorderDecorate & WBorderDecorate::SetThickness( float _Thickness ) {
    Thickness = _Thickness;
    return *this;
}

WBorderDecorate & WBorderDecorate::SetRounding( float _Rounding ) {
    Rounding = _Rounding;
    return *this;
}

WBorderDecorate & WBorderDecorate::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

void WBorderDecorate::OnDrawEvent( FCanvas & _Canvas ) {
    Float2 mins, maxs;

    GetOwner()->GetDesktopRect( mins, maxs, false );

    if ( bFillBackgrond ) {
        _Canvas.DrawRectFilled( mins, maxs, BgColor, Rounding, RoundingCorners );
    }

    _Canvas.DrawRect( mins, maxs, Color, Rounding, RoundingCorners, Thickness );
}


AN_CLASS_META( WImageDecorate )

WImageDecorate::WImageDecorate() {
    Color = FColor4::White();
    Rounding = 0;
    RoundingCorners = CORNER_ROUND_ALL;
    ColorBlending = COLOR_BLENDING_ALPHA;
    SamplerType = HUD_SAMPLER_TILED_LINEAR;
    bUseOriginalSize = false;
    HorizontalAlignment = WIDGET_ALIGNMENT_NONE;
    VerticalAlignment = WIDGET_ALIGNMENT_NONE;
    Size = Float2(32,32);
    UV0 = Float2(0,0);
    UV1 = Float2(1,1);
}

WImageDecorate::~WImageDecorate() {
}

WImageDecorate & WImageDecorate::SetColor( FColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WImageDecorate & WImageDecorate::SetRounding( float _Rounding ) {
    Rounding = _Rounding;
    return *this;
}

WImageDecorate & WImageDecorate::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

WImageDecorate & WImageDecorate::SetTexture( FTexture2D * _Texture ) {
    Texture = _Texture;
    return *this;
}

WImageDecorate & WImageDecorate::SetColorBlending( EColorBlending _Blending ) {
    ColorBlending = _Blending;
    return *this;
}

WImageDecorate & WImageDecorate::SetSamplerType( EHUDSamplerType _SamplerType ) {
    SamplerType = _SamplerType;
    return *this;
}

WImageDecorate & WImageDecorate::SetOffset( Float2 const & _Offset ) {
    Offset = _Offset;
    return *this;
}

WImageDecorate & WImageDecorate::SetSize( Float2 const & _Size ) {
    Size = _Size;
    return *this;
}

WImageDecorate & WImageDecorate::SetHorizontalAlignment( EWidgetAlignment _Alignment ) {
    HorizontalAlignment = _Alignment;
    return *this;
}

WImageDecorate & WImageDecorate::SetVerticalAlignment( EWidgetAlignment _Alignment ) {
    VerticalAlignment = _Alignment;
    return *this;
}

WImageDecorate & WImageDecorate::SetUseOriginalSize( bool _UseOriginalSize ) {
    bUseOriginalSize = _UseOriginalSize;
    return *this;
}

WImageDecorate & WImageDecorate::SetUVs( Float2 const & _UV0, Float2 const & _UV1 ) {
    UV0 = _UV0;
    UV1 = _UV1;
    return *this;
}

void WImageDecorate::OnDrawEvent( FCanvas & _Canvas ) {
    if ( !Texture ) {
        return;
    }

    Float2 ownerSize = GetOwner()->GetCurrentSize();

    Float2 pos;
    Float2 size = Size;

    if ( bUseOriginalSize ) {
        size.X = Texture->GetWidth();
        size.Y = Texture->GetHeight();
    }

    if ( HorizontalAlignment == WIDGET_ALIGNMENT_STRETCH ) {
        pos.X = 0;
        size.X = ownerSize.X;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_LEFT ) {
        pos.X = 0;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_RIGHT ) {
        pos.X = ownerSize.X - size.X;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.X * 0.5f;
        pos.X = center - size.X * 0.5f;
    } else {
        pos.X = Offset.X;
    }

    if ( VerticalAlignment == WIDGET_ALIGNMENT_STRETCH ) {
        pos.Y = 0;
        size.Y = ownerSize.Y;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_TOP ) {
        pos.Y = 0;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_BOTTOM ) {
        pos.Y = ownerSize.Y - size.Y;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.Y * 0.5f;
        pos.Y = center - size.Y * 0.5f;
    } else {
        pos.Y = Offset.Y;
    }

    pos += GetOwner()->GetDesktopPosition();

    _Canvas.DrawTextureRounded( Texture, pos.X, pos.Y, size.X, size.Y, UV0, UV1, Color, Rounding, RoundingCorners, ColorBlending, SamplerType );
}

#if 0
AN_CLASS_META( WViewportDecorate )

WViewportDecorate::WViewportDecorate() {
    Color = FColor4::White();
    Rounding = 0;
    RoundingCorners = CORNER_ROUND_ALL;
    ColorBlending = COLOR_BLENDING_DISABLED;
    HorizontalAlignment = WIDGET_ALIGNMENT_NONE;
    VerticalAlignment = WIDGET_ALIGNMENT_NONE;
    Size = Float2(32,32);
}

WViewportDecorate::~WViewportDecorate() {
}

WViewportDecorate & WViewportDecorate::SetColor( FColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetRounding( float _Rounding ) {
    Rounding = _Rounding;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetPlayerController( FPlayerController * _PlayerController ) {
    PlayerController = _PlayerController;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetColorBlending( EColorBlending _Blending ) {
    ColorBlending = _Blending;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetOffset( Float2 const & _Offset ) {
    Offset = _Offset;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetSize( Float2 const & _Size ) {
    Size = _Size;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetHorizontalAlignment( EWidgetAlignment _Alignment ) {
    HorizontalAlignment = _Alignment;
    return *this;
}

WViewportDecorate & WViewportDecorate::SetVerticalAlignment( EWidgetAlignment _Alignment ) {
    VerticalAlignment = _Alignment;
    return *this;
}

void WViewportDecorate::OnDrawEvent( FCanvas & _Canvas ) {
    if ( !PlayerController ) {
        return;
    }

    Float2 pos;

    Float2 ownerSize = GetOwner()->GetCurrentSize();

    Float2 size = Size;

    if ( HorizontalAlignment == WIDGET_ALIGNMENT_STRETCH ) {
        pos.X = 0;
        size.X = ownerSize.X;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_LEFT ) {
        pos.X = 0;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_RIGHT ) {
        pos.X = ownerSize.X - size.X;
    } else if ( HorizontalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.X * 0.5f;
        pos.X = center - size.X * 0.5f;
    } else {
        pos.X = Offset.X;
    }

    if ( VerticalAlignment == WIDGET_ALIGNMENT_STRETCH ) {
        pos.Y = 0;
        size.Y = ownerSize.Y;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_TOP ) {
        pos.Y = 0;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_BOTTOM ) {
        pos.Y = ownerSize.Y - size.Y;
    } else if ( VerticalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = ownerSize.Y * 0.5f;
        pos.Y = center - size.Y * 0.5f;
    } else {
        pos.Y = Offset.Y;
    }

    pos += GetOwner()->GetDesktopPosition();

    _Canvas.DrawViewport( PlayerController, pos.X, pos.Y, size.X, size.Y, Color, Rounding, RoundingCorners, ColorBlending );
}
#endif
