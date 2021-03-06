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

#include <World/Public/Widgets/WButton.h>
#include <Runtime/Public/Runtime.h>

AN_CLASS_META( WButton )

WButton::WButton() {
    State = ST_RELEASED;
    bToggleButton = false;
}

WButton::~WButton() {
}

void WButton::OnMouseButtonEvent( SMouseButtonEvent const & _Event, double _TimeStamp ) {
    if ( _Event.Button != 0 ) {
        return;
    }

    if ( bToggleButton ) {
        if ( _Event.Action == IA_PRESS ) {
            if ( State == ST_PRESSED ) {
                State = ST_RELEASED;
            }
            else {
                State = ST_PRESSED;
            }
        }
        else if ( _Event.Action == IA_RELEASE ) {
            if ( IsHoveredByCursor() ) {
                // Keep new state and dispatch event
                E_OnButtonClick.Dispatch( this );
            }
            else {
                // Switch to previous state
                if ( State == ST_PRESSED ) {
                    State = ST_RELEASED;
                }
                else {
                    State = ST_PRESSED;
                }
            }
        }
    }
    else {
        if ( _Event.Action == IA_PRESS ) {
            State = ST_PRESSED;
        }
        else if ( _Event.Action == IA_RELEASE ) {
            if ( State == ST_PRESSED && IsHoveredByCursor() ) {

                State = ST_RELEASED;

                E_OnButtonClick.Dispatch( this );
            }
            else {
                State = ST_RELEASED;
            }
        }
    }
}

enum
{
    DRAW_DISABLED,
    DRAW_SIMPLE,
    DRAW_HOVERED,
    DRAW_PRESSED
};

int WButton::GetDrawState() const
{
    if ( IsDisabled() ) {
        return DRAW_DISABLED;
    }
    else if ( IsToggleButton() ) {
        if ( IsPressed() ) {
            return DRAW_PRESSED;
        }
        else if ( IsHoveredByCursor() ) {
            return DRAW_HOVERED;
        }
        else {
            return DRAW_SIMPLE;
        }
    }
    else if ( IsHoveredByCursor() ) {
        if ( IsPressed() ) {
            return DRAW_PRESSED;
        } else {
            return DRAW_HOVERED;
        }
    }
    return DRAW_SIMPLE;
}

void WButton::OnDrawEvent( ACanvas & _Canvas ) {
    AColor4 bgColor;

    switch ( GetDrawState() ) {
    case DRAW_DISABLED:
        bgColor = AColor4(0.4f,0.4f,0.4f);
        break;
    case DRAW_SIMPLE:
        bgColor = AColor4(0.4f,0.4f,0.4f);
        break;
    case DRAW_HOVERED:
        bgColor = AColor4( 0.5f,0.5f,0.5f,1 );
        break;
    case DRAW_PRESSED:
        bgColor = AColor4( 0.6f,0.6f,0.6f,1 );
        break;
    }

    Float2 mins, maxs;

    GetDesktopRect( mins, maxs, true );

    _Canvas.DrawRectFilled( mins, maxs, bgColor );
}


AN_CLASS_META( WTextButton )

WTextButton::WTextButton() {
    Color = AColor4(0.4f,0.4f,0.4f);
    HoverColor = AColor4( 0.5f,0.5f,0.5f,1 );
    PressedColor = AColor4( 0.6f,0.6f,0.6f,1 );
    TextColor = AColor4::White();
    BorderColor = Float4(0,0,0,0.5f);
    Rounding = 8;
    RoundingCorners = CORNER_ROUND_ALL;
    BorderThickness = 1;
    TextAlign = WIDGET_BUTTON_TEXT_ALIGN_CENTER;
    Font = ACanvas::GetDefaultFont();
}

WTextButton::~WTextButton() {
}

WTextButton & WTextButton::SetText( AStringView _Text ) {
    Text = _Text;
    return *this;
}

WTextButton & WTextButton::SetColor( AColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WTextButton & WTextButton::SetHoverColor( AColor4 const & _Color ) {
    HoverColor = _Color;
    return *this;
}

WTextButton & WTextButton::SetPressedColor( AColor4 const & _Color ) {
    PressedColor = _Color;
    return *this;
}

WTextButton & WTextButton::SetTextColor( AColor4 const & _Color ) {
    TextColor = _Color;
    return *this;
}

WTextButton & WTextButton::SetBorderColor( AColor4 const & _Color ) {
    BorderColor = _Color;
    return *this;
}

WTextButton & WTextButton::SetRounding( float _Rounding ) {
    Rounding = _Rounding;
    return *this;
}

WTextButton & WTextButton::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

WTextButton & WTextButton::SetBorderThickness( float _BorderThickness ) {
    BorderThickness = _BorderThickness;
    return *this;
}

WTextButton & WTextButton::SetTextAlign( EWidgetButtonTextAlign _TextAlign ) {
    TextAlign = _TextAlign;
    return *this;
}

WTextButton & WTextButton::SetFont( AFont * _Font ) {
    Font = _Font ? _Font : ACanvas::GetDefaultFont();
    return *this;
}

void WTextButton::OnDrawEvent( ACanvas & _Canvas ) {
    AColor4 bgColor;

    switch ( GetDrawState() ) {
    case DRAW_DISABLED:
        bgColor = Color;
        break;
    case DRAW_SIMPLE:
        bgColor = Color;
        break;
    case DRAW_HOVERED:
        bgColor = HoverColor;
        break;
    case DRAW_PRESSED:
        bgColor = PressedColor;
        break;
    }

    Float2 mins, maxs;

    GetDesktopRect( mins, maxs, true );

    float width = GetAvailableWidth();
    float height = GetAvailableHeight();

    _Canvas.DrawRectFilled( mins, maxs, bgColor, Rounding, RoundingCorners );
    if ( BorderThickness > 0.0f ) {
        _Canvas.DrawRect( mins, maxs, BorderColor, Rounding, RoundingCorners, BorderThickness );
    }

    Float2 size = Font->CalcTextSizeA( Font->GetFontSize(), width, 0, Text.Begin(), Text.End() );

    Float2 pos = mins;
    pos.Y += (height - size.Y) * 0.5f;

    switch ( TextAlign )
    {
        case WIDGET_BUTTON_TEXT_ALIGN_CENTER:
        {
            pos.X += ( width - size.X ) * 0.5f;
            break;
        }
        case WIDGET_BUTTON_TEXT_ALIGN_LEFT:
        {
            break;
        }
        case WIDGET_BUTTON_TEXT_ALIGN_RIGHT:
        {
            pos.X += width - size.X;
            break;
        }
    }

    _Canvas.PushFont( Font );
    _Canvas.DrawTextUTF8( pos, TextColor, Text.Begin(), Text.End() );
    _Canvas.PopFont();
}


AN_CLASS_META( WImageButton )

WImageButton::WImageButton() {
}

WImageButton::~WImageButton() {
}

WImageButton & WImageButton::SetImage( ATexture * _Image ) {
    Image = _Image;
    return *this;
}

WImageButton & WImageButton::SetHoverImage( ATexture * _Image ) {
    HoverImage = _Image;
    return *this;
}

WImageButton & WImageButton::SetPressedImage( ATexture * _Image ) {
    PressedImage = _Image;
    return *this;
}

void WImageButton::OnDrawEvent( ACanvas & _Canvas ) {
    ATexture * bgImage = nullptr;

    switch ( GetDrawState() ) {
    case DRAW_DISABLED:
        bgImage = Image;
        break;
    case DRAW_SIMPLE:
        bgImage = Image;
        break;
    case DRAW_HOVERED:
        bgImage = HoverImage;
        break;
    case DRAW_PRESSED:
        bgImage = PressedImage;
        break;
    }

    if ( bgImage ) {
        Float2 mins, maxs;

        GetDesktopRect( mins, maxs, true );

        _Canvas.DrawTexture( bgImage, mins.X, mins.Y, maxs.X-mins.X, maxs.Y-mins.Y );
    }
}
