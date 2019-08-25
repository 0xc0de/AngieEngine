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

#include <Engine/Widgets/Public/WWindow.h>

AN_CLASS_META( WWindow )

WWindow::WWindow() {
    CaptionHeight = 30;
    TextColor = FColor4::White();
    TextHorizontalAlignment = WIDGET_ALIGNMENT_CENTER;
    TextVerticalAlignment = WIDGET_ALIGNMENT_CENTER;
    CaptionColor = FColor4::Black();
    BorderColor = FColor4::White();
    RoundingCorners = CORNER_ROUND_ALL;
    BorderRounding = 8;
    BorderThickness = 2;
    bWindowBorder = true;
    bCaptionBorder = true;
    BgColor = FColor4( 0 ); // transparent by default
    UpdateDragShape();
    UpdateMargin();
}

WWindow::~WWindow() {
}

WWindow & WWindow::SetCaptionText( const char * _CaptionText ) {
    CaptionText = _CaptionText;
    return *this;
}

WWindow & WWindow::SetCaptionHeight( float _CaptionHeight ) {
    CaptionHeight = _CaptionHeight;
    UpdateDragShape();
    UpdateMargin();
    return *this;
}

WWindow & WWindow::SetCaptionFont( FFontAtlas * _Atlas, int _FontId ) {
    FontAtlas = _Atlas;
    FontId = _FontId;
    return *this;
}

WWindow & WWindow::SetTextColor( FColor4 const & _Color ) {
    TextColor = _Color;
    return *this;
}

WWindow & WWindow::SetTextHorizontalAlignment( EWidgetAlignment _Alignment ) {
    TextHorizontalAlignment = _Alignment;
    return *this;
}

WWindow & WWindow::SetTextVerticalAlignment( EWidgetAlignment _Alignment ) {
    TextVerticalAlignment = _Alignment;
    return *this;
}

WWindow & WWindow::SetWordWrap( bool _WordWrap ) {
    bWordWrap = _WordWrap;
    return *this;
}

WWindow & WWindow::SetTextOffset( Float2 const & _Offset ) {
    TextOffset = _Offset;
    return *this;
}

WWindow & WWindow::SetCaptionColor( FColor4 const & _Color ) {
    CaptionColor = _Color;
    return *this;
}

WWindow & WWindow::SetBorderColor( FColor4 const & _Color ) {
    BorderColor = _Color;
    return *this;
}

WWindow & WWindow::SetBorderThickness( float _Thickness ) {
    BorderThickness = _Thickness;
    UpdateMargin();
    return *this;
}

WWindow & WWindow::SetBackgroundColor( FColor4 const & _Color ) {
    BgColor = _Color;
    return *this;
}

WWindow & WWindow::SetRounding( float _Rounding ) {
    BorderRounding = _Rounding;
    return *this;
}

WWindow & WWindow::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

void WWindow::UpdateDragShape() {
    Float2 vertices[4];

    float w = GetCurrentSize().X;

    vertices[0] = Float2(0,0);
    vertices[1] = Float2(w,0);
    vertices[2] = Float2(w,CaptionHeight);
    vertices[3] = Float2(0,CaptionHeight);

    SetDragShape( vertices, 4 );
}

void WWindow::UpdateMargin() {
    Float4 margin;
    margin.X = BorderThickness;
    margin.Y = CaptionHeight;
    margin.Z = BorderThickness;
    margin.W = BorderThickness;
    SetMargin( margin );
}

void WWindow::OnTransformDirty() {
    Super::OnTransformDirty();

    UpdateDragShape();
}

Float2 WWindow::GetTextPositionWithAlignment( FCanvas & _Canvas ) const {
    Float2 pos;

    const float width = GetCurrentSize().X;
    const float height = CaptionHeight;

    FFont const * font = GetFont( _Canvas );
    Float2 size = font->CalcTextSizeA( font->FontSize, width, bWordWrap ? width : 0.0f, CaptionText.Begin(), CaptionText.End() );

    if ( TextHorizontalAlignment == WIDGET_ALIGNMENT_LEFT ) {
        pos.X = 0;
    } else if ( TextHorizontalAlignment == WIDGET_ALIGNMENT_RIGHT ) {
        pos.X = width - size.X;
    } else if ( TextHorizontalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = width * 0.5f;
        pos.X = center - size.X * 0.5f;
    } else {
        pos.X = TextOffset.X;
    }

    if ( TextVerticalAlignment == WIDGET_ALIGNMENT_TOP ) {
        pos.Y = 0;
    } else if ( TextVerticalAlignment == WIDGET_ALIGNMENT_BOTTOM ) {
        pos.Y = height - size.Y;
    } else if ( TextVerticalAlignment == WIDGET_ALIGNMENT_CENTER ) {
        float center = height * 0.5f;
        pos.Y = center - size.Y * 0.5f;
    } else {
        pos.Y = TextOffset.Y;
    }

    // WIDGET_ALIGNMENT_STRETCH is not handled for text

    return pos;
}

FFont const * WWindow::GetFont( FCanvas & _Canvas ) const {
    FFont const * font = nullptr;
    if ( FontAtlas ) {
        font = FontAtlas->GetFont( FontId );
    }

    if ( !font ) {
        // back to default font
        font = _Canvas.GetDefaultFont();
    }

    return font;
}

void WWindow::OnDrawEvent( FCanvas & _Canvas ) {
    Float2 mins, maxs;

    GetDesktopRect( mins, maxs, false );

    if ( !BgColor.IsTransparent() ) {

        FWidgetShape const & windowShape = GetShape();

        int bgCorners = 0;
        if ( RoundingCorners & CORNER_ROUND_BOTTOM_LEFT ) {
            bgCorners |= CORNER_ROUND_BOTTOM_LEFT;
        }
        if ( RoundingCorners & CORNER_ROUND_BOTTOM_RIGHT ) {
            bgCorners |= CORNER_ROUND_BOTTOM_RIGHT;
        }

        if ( windowShape.IsEmpty() ) {

            _Canvas.DrawRectFilled( mins + Float2( 0, CaptionHeight ), maxs, BgColor, BorderRounding, bgCorners );

        } else {

            // TODO: Draw triangulated concave polygon

            _Canvas.DrawRectFilled( mins + Float2( 0, CaptionHeight ), maxs, BgColor, BorderRounding, bgCorners );
        }
    }

    // Draw border
    if ( bWindowBorder ) {

        FWidgetShape const & windowShape = GetShape();

        if ( windowShape.IsEmpty() ) {
            _Canvas.DrawRect( mins, maxs, BorderColor, BorderRounding, RoundingCorners, BorderThickness );
        } else {
            _Canvas.DrawPolyline( windowShape.ToPtr(), windowShape.Size(), BorderColor, false, BorderThickness );
        }
    }

    // Draw caption
    if ( CaptionHeight > 0.0f ) {
        float width = GetCurrentSize().X;
        Float2 captionSize = Float2( width, CaptionHeight );

        int captionCorners = 0;
        if ( RoundingCorners & CORNER_ROUND_TOP_LEFT ) {
            captionCorners |= CORNER_ROUND_TOP_LEFT;
        }
        if ( RoundingCorners & CORNER_ROUND_TOP_RIGHT ) {
            captionCorners |= CORNER_ROUND_TOP_RIGHT;
        }

        // Draw caption
        _Canvas.DrawRectFilled( mins, mins + captionSize, CaptionColor, BorderRounding, captionCorners );

        // Draw caption border
        if ( bCaptionBorder ) {
            _Canvas.DrawRect( mins, mins + captionSize, BorderColor, BorderRounding, captionCorners, BorderThickness );
        }

        // Draw caption text
        if ( !CaptionText.IsEmpty() ) {
            FFont const * font = GetFont( _Canvas );
            _Canvas.PushClipRect( mins, mins + captionSize, true );
            _Canvas.DrawTextUTF8( font, font->FontSize, mins + GetTextPositionWithAlignment( _Canvas ), TextColor, CaptionText.Begin(), CaptionText.End(), bWordWrap ? width : 0.0f );
            _Canvas.PopClipRect();
        }
    }
}
