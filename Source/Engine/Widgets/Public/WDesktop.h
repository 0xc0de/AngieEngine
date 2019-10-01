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

class WWidget;

class ANGIE_API WMenuPopup : public FBaseObject {
    AN_CLASS( WMenuPopup, FBaseObject )

    friend class WDesktop;

public:

    // Add child widget
    WMenuPopup & AddWidget( WWidget * _Widget ) {
        Self->AddWidget( _Widget );
//        Widgets.Append( _Widget );
//        _Widget->AddRef();
        Float2 sz = _Widget->GetCurrentSize();
        Float4 const & margin = Self->GetMargin();
        CurWidth = FMath::Max( CurWidth, sz.X );
        CurHeight += sz.Y;
        Self->SetSize( CurWidth + margin.X + margin.Z, CurHeight + margin.Y + margin.W );
        return *this;
    }

    // Helper. Add child widget
    WMenuPopup & operator[]( WWidget & _Widget ) {
        return AddWidget( &_Widget );
    }

    // Helper. Add child decorate
    WMenuPopup & operator[]( WDecorate & _Decorate ) {
        Self->AddDecorate( &_Decorate );
        return *this;
    }

    void SelectFirstItem();
    void SelectLastItem();
    void SelectNextItem();
    void SelectPrevItem();
    void SelectNextSubMenu();
    void SelectPrevSubMenu();

protected:
    WMenuPopup();
    ~WMenuPopup();

private:
    TRef< WWidget > Self;
    float CurWidth;
    float CurHeight;

//    TPodArray< WWidget * > Widgets;
};

class ANGIE_API WDesktop : public FBaseObject {
    AN_CLASS( WDesktop, FBaseObject )

    friend class WWidget;

public:
    template< typename T >
    T * AddWidget() {
        T * w = NewObject< T >();
        AddWidget( w );
        return w;
    }

    WDesktop & AddWidget( WWidget * _Widget );

    WDesktop & RemoveWidget( WWidget * _Widget );

    WDesktop & RemoveWidgets();

    WDesktop & SetSize( float _Width, float _Height );

    WDesktop & SetSize( Float2 const & _Size );

    float GetWidth() const;

    float GetHeight() const;

    WDesktop & SetCursorPosition( Float2 const & _Position ) { CursorPosition = _Position; return *this; }

    Float2 const & GetCursorPosition() const { return CursorPosition; }

    WDesktop & SetCursorVisible( bool _Visible );

    bool IsCursorVisible() const { return bCursorVisible; }

    WDesktop & SetDrawBackground( bool _DrawBackground ) { bDrawBackground = _DrawBackground; return *this; }

    void OpenPopupMenu( WMenuPopup * _PopupMenu );

    void ClosePopupMenu();

    void CancelDragging();

    void SetFocusWidget( WWidget * _Focus );

    // Возвращает окно, у которого установлен фокус
    WWidget * GetFocusWidget() { return FocusWidget; }

    // Возвращает окно, которое в данный момент перетаскиваем
    WWidget * GetDraggingWidget() { return DraggingWidget; }

    // Возвращает окно, которое находится под точкой (только для видимых окон).
    // Если точка одновременно находится над несколькими окнами, то возвращается указатель на окно верхнего уровня.
    WWidget * GetWidgetUnderCursor( Float2 const & _Position );

    // Рекурсивный обход всех окон интерфейса с вызовом callback - функции для каждого обходимого окна.
    // Если _TopFirst = true, то обход окон идет от верхнего уровня, до нижнего. В противном случае - наоборот.
    // Callback - функция принимает два аргумента: текущее окно и дополнительный аргумент _Arg, передаваемый в функцию ForEachWidget
    template< typename T >
    void ForEachWidget( bool _TopFirst, T * _Object, bool (T::*_Method)( WWidget * _Widget, void * _Arg ), void * _Arg ) {
        ForEachWidget( _TopFirst, { _Object, _Method }, _Arg );
    }

    void ForEachWidget( bool _TopFirst, TCallback< bool( WWidget * _Widget, void * _Arg ) > const & _Callback, void * _Arg );

    // Создает событие KeyEvent
    // Если клавиша - это кнопка мыши, то устанавливается фокус у окна под курсором.
    // Далее срабатывает событиеKeyPressEvent или KeyReleaseEvent у окна, которое
    // в данный момент в фокусе.
    void GenerateKeyEvents( struct FKeyEvent const & _Event, double _TimeStamp );

    // Создает события MouseButtonEvent и OnDblClickEvent
    // Устанавливает фокус у окна под курсором. Далее срабатывает событие
    // MouseButtonEvent у окна, которое в данный момент в фокусе.
    void GenerateMouseButtonEvents( struct FMouseButtonEvent const & _Event, double _TimeStamp );

    // Создает событие MouseWheelEvent
    void GenerateMouseWheelEvents( struct FMouseWheelEvent const & _Event, double _TimeStamp );

    // Инициирует событие перемещаения мыши в окне, над которым в данный момент находится курсор.
    // Перемещает курсор на указанную дельта. Осуществляет перемещение текущего захваченного окна.
    void GenerateMouseMoveEvents( struct FMouseMoveEvent const & _Event, double _TimeStamp );

    // Создает событие CharEvent у окна, которое
    // в данный момент в фокусе.
    void GenerateCharEvents( struct FCharEvent const & _Event, double _TimeStamp );

    // Создает событие DrawEvent для всех видимых окон графического интерфейса.
    // События вызываются начиная с нижних уровней до высоких.
    void GenerateDrawEvents( FCanvas & _Canvas );

protected:
    WDesktop();
    ~WDesktop();

    virtual void OnDrawBackground( FCanvas & _Canvas );

    virtual void OnDrawCursor( FCanvas & _Canvas );

private:
    WWidget * GetWidgetUnderCursor_r( WWidget * _Widget, Float2 const & _ClipMins, Float2 const & _ClipMaxs, Float2 const & _Position );

    bool ForEachWidget_r( bool _TopFirst, WWidget * _Widget, TCallback< bool( WWidget * _Widget, void * _Arg ) > const & _Callback, void * _Arg );

    WWidget * GetExclusive();
    bool HandleDraggingWidget();

    TRef< WWidget > Root;
    TRef< WMenuPopup > Popup;
    WWidget * FocusWidget;
    TRef< WWidget > DraggingWidget;
    TRef< WWidget > MouseClickWidget;
    TRef< WWidget > MouseFocusWidget;
    uint64_t MouseClickTime;
    Float2 MouseClickPos;
    Float2 DraggingCursor;
    Float2 DraggingWidgetPos;
    Float2 CursorPosition;
    bool bCursorVisible;
    bool bDrawBackground;
};