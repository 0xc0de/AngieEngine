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

#include "WWidget.h"

class ANGIE_API WButton : public WWidget {
    AN_CLASS( WButton, WWidget )

public:
    TWidgetEvent<> E_OnButtonClick;

    template< typename T, typename... TArgs >
    WButton & SetOnClick( T * _Object, void ( T::*_Method )(TArgs...) ) {
        E_OnButtonClick.Add( _Object, _Method );
        return *this;
    }

    bool IsPressed() const { return State == ST_PRESSED; }
    bool IsReleased() const { return State == ST_RELEASED; }

protected:
    WButton();
    ~WButton();

    void OnMouseButtonEvent( struct FMouseButtonEvent const & _Event, double _TimeStamp ) override;

    void OnDrawEvent( FCanvas & _Canvas ) override;

private:
    enum {
        ST_RELEASED = 0,
        ST_PRESSED = 1
    };

    int State;
};

class ANGIE_API WTextButton : public WButton {
    AN_CLASS( WTextButton, WButton )

public:
    WTextButton & SetText( const char * _Text );
    WTextButton & SetColor( FColor4 const & _Color );
    WTextButton & SetHoverColor( FColor4 const & _Color );
    WTextButton & SetPressedColor( FColor4 const & _Color );
    WTextButton & SetTextColor( FColor4 const & _Color );
    WTextButton & SetBorderColor( FColor4 const & _Color );
    WTextButton & SetRounding( float _Rounding );
    WTextButton & SetRoundingCorners( EDrawCornerFlags _RoundingCorners );
    WTextButton & SetBorderThickness( float _Thickness );

protected:
    WTextButton();
    ~WTextButton();

    void OnDrawEvent( FCanvas & _Canvas ) override;

private:
    FColor4 Color;
    FColor4 HoverColor;
    FColor4 PressedColor;
    FColor4 TextColor;
    FColor4 BorderColor;
    EDrawCornerFlags RoundingCorners;
    FString Text;
    float Rounding;
    float BorderThickness;
};

class ANGIE_API WImageButton : public WButton {
    AN_CLASS( WImageButton, WButton )

public:
    WImageButton & SetImage( FTexture * _Image );
    WImageButton & SetHoverImage( FTexture * _Image );
    WImageButton & SetPressedImage( FTexture * _Image );

protected:
    WImageButton();
    ~WImageButton();

    void OnDrawEvent( FCanvas & _Canvas ) override;

private:
    TRef<FTexture> Image;
    TRef<FTexture> HoverImage;
    TRef<FTexture> PressedImage;
};
