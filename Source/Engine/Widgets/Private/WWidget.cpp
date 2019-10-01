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

#include <Engine/Widgets/Public/WWidget.h>
#include <Engine/Widgets/Public/WDesktop.h>

AN_CLASS_META( WWidget )

WWidget::WWidget() {
    Size = Float2( 32, 32 );
    Visibility = WIDGET_VISIBILITY_VISIBLE;
    ColumnsCount = RowsCount = 1;
    bTransformDirty = true;
    bLayoutDirty = true;
}

WWidget::~WWidget() {
    RemoveDecorates();

    AN_Assert( !bFocus );

#if 0
    // TODO: this must be optimized:
    while ( !Childs.IsEmpty() ) {
        Childs[0]->Unparent();
    }
#else
    for ( WWidget * child : Childs ) {
        child->Parent = nullptr;
        child->MarkTransformDirty(); // FIXME: mark anyway?
        child->RemoveRef();
    }
#endif
}

WWidget & WWidget::SetParent( WWidget * _Parent ) {
    if ( IsRoot() ) {
        return *this;
    }

    if ( Parent == _Parent ) {
        return *this;
    }

    Unparent();

    if ( !_Parent ) {
        return *this;
    }

    Parent = _Parent;

    AddRef();
    _Parent->Childs.Insert( 0, this );

    BringOnTop( false );

    Parent->LayoutSlots.Append( this );
    Parent->MarkVHLayoutDirty();

    MarkTransformDirty();

    return *this;
}

void WWidget::LostFocus_r( WDesktop * _Desktop ) {
    if ( bFocus ) {
        AN_Assert( _Desktop->GetFocusWidget() == this );
        _Desktop->SetFocusWidget( nullptr );
        return;
    }

    for ( WWidget * child : Childs ) {
        child->LostFocus_r( _Desktop );
    }
}

WWidget & WWidget::Unparent() {
    if ( IsRoot() ) {
        return *this;
    }

    if ( !Parent ) {
        return *this;
    }

    LostFocus_r( GetDesktop() );

    Parent->Childs.Remove( Parent->Childs.IndexOf( this ) );
    Parent->LayoutSlots.Remove( Parent->LayoutSlots.IndexOf( this ) );
    Parent->MarkVHLayoutDirty();
    Parent = nullptr;

    MarkTransformDirty();
    RemoveRef();

    return *this;
}

void WWidget::RemoveWidgets() {
    while ( !Childs.IsEmpty() ) {
        Childs.Last()->Unparent();
    }
}

WDesktop * WWidget::GetDesktop() {
    WWidget * root = GetRoot();
    if ( root ) {
        return root->Desktop;
    }
    return nullptr;
}

WWidget * WWidget::GetRoot() {
    if ( IsRoot() ) {
        return this;
    }
    if ( Parent ) {
        return Parent->GetRoot();
    }
    return nullptr;
}

WWidget & WWidget::SetStyle( EWidgetStyle _Style ) {
    bool bBringOnTop = false;

    int style = _Style;

    // Оба стиля не могут существовать одновременно
    if ( style & WIDGET_STYLE_FOREGROUND ) {
        style &= ~WIDGET_STYLE_BACKGROUND;
    }

    if ( ( style & WIDGET_STYLE_FOREGROUND ) && !( Style & WIDGET_STYLE_FOREGROUND ) ) {
        bBringOnTop = true;
    }

    Style = (EWidgetStyle)style;

    if ( bBringOnTop ) {
        BringOnTop();
    }

    return *this;
}

WWidget & WWidget::SetStyle( int _Style ) {
    return SetStyle( (EWidgetStyle)_Style );
}

WWidget & WWidget::SetFocus() {
    WDesktop * desktop = GetDesktop();
    if ( !desktop ) {
        return *this;
    }

    desktop->SetFocusWidget( this );

    return *this;
}

bool WWidget::IsFocus() const {
    return bFocus;
}

WWidget & WWidget::SetPosition( float _X, float _Y ) {
    return SetPosition( Float2( _X, _Y ) );
}

WWidget & WWidget::SetPosition( Float2 const & _Position ) {
    Position = ( _Position + 0.5f ).Floor();
    MarkTransformDirty();
    return *this;
}

WWidget & WWidget::SetDektopPosition( float _X, float _Y ) {
    return SetDektopPosition( Float2( _X, _Y ) );
}

WWidget & WWidget::SetDektopPosition( Float2 const & _Position ) {
    Float2 position = _Position;
    if ( Parent ) {
        position -= Parent->GetDesktopPosition() + Parent->Margin.Shuffle2< sXY >();
    }
    return SetPosition( position );
}

WWidget & WWidget::SetSize( float _Width, float _Height ) {
    return SetSize( Float2( _Width, _Height ) );
}

WWidget & WWidget::SetSize( Float2 const & _Size ) {

    Float2 sz = ( _Size + 0.5f ).Floor();

    if ( sz.X < 0.0f ) {
        sz.X = 0.0f;
    }
    if ( sz.Y < 0.0f ) {
        sz.Y = 0.0f;
    }

    Size = sz;
    MarkTransformDirty();

    return *this;
}

WWidget & WWidget::SetMinSize( float _Width, float _Height ) {
    return SetMinSize( Float2( _Width, _Height ) );
}

WWidget & WWidget::SetMinSize( Float2 const & _Size ) {
    MinSize = _Size;

    if ( MinSize.X < 0.0f ) {
        MinSize.X = 0.0f;
    }
    if ( MinSize.Y < 0.0f ) {
        MinSize.Y = 0.0f;
    }

    MarkTransformDirty();

    return *this;
}

WWidget & WWidget::SetMaxSize( float _Width, float _Height ) {
    return SetMaxSize( Float2( _Width, _Height ) );
}

WWidget & WWidget::SetMaxSize( Float2 const & _Size ) {
    MaxSize = _Size;

    if ( MaxSize.X < 0.0f ) {
        MaxSize.X = 0.0f;
    }
    if ( MaxSize.Y < 0.0f ) {
        MaxSize.Y = 0.0f;
    }

    MarkTransformDirty();

    return *this;
}

WWidget & WWidget::SetShape( Float2 const * _Vertices, int _NumVertices ) {
    if ( _NumVertices > 0 ) {
        Shape.ResizeInvalidate( _NumVertices );
        memcpy( Shape.ToPtr(), _Vertices, _NumVertices * sizeof( Shape[0] ) );
    } else {
        Shape.Clear();
    }

    return *this;
}

WWidget & WWidget::SetDragShape( Float2 const * _Vertices, int _NumVertices ) {
    if ( _NumVertices > 0 ) {
        DragShape.ResizeInvalidate( _NumVertices );
        memcpy( DragShape.ToPtr(), _Vertices, _NumVertices * sizeof( DragShape[0] ) );
    } else {
        DragShape.Clear();
    }

    return *this;
}

WWidget & WWidget::SetMargin( float _Left, float _Top, float _Right, float _Bottom ) {
    return SetMargin( Float4( _Left, _Top, _Right, _Bottom ) );
}

WWidget & WWidget::SetMargin( Float4 const & _Margin ) {
    Margin = _Margin;
    if ( Margin.X < 0.0f ) {
        Margin.X = 0.0f;
    }
    if ( Margin.Y < 0.0f ) {
        Margin.Y = 0.0f;
    }
    if ( Margin.Z < 0.0f ) {
        Margin.Z = 0.0f;
    }
    if ( Margin.W < 0.0f ) {
        Margin.W = 0.0f;
    }
    MarkTransformDirtyChilds();
    return *this;
}

WWidget & WWidget::SetHorizontalAlignment( EWidgetAlignment _Alignment ) {
    HorizontalAlignment = _Alignment;
    MarkTransformDirty();
    return *this;
}

WWidget & WWidget::SetVerticalAlignment( EWidgetAlignment _Alignment ) {
    VerticalAlignment = _Alignment;
    MarkTransformDirty();
    return *this;
}

WWidget & WWidget::SetLayout( EWidgetLayout _Layout ) {
    if ( Layout != _Layout ) {
        Layout = _Layout;
        bLayoutDirty = true;
        MarkTransformDirtyChilds();
    }
    return *this;
}

WWidget & WWidget::SetGridOffset( int _Column, int _Row ) {
    Column = _Column;
    Row = _Row;
    MarkTransformDirty();
    return *this;
}

WWidget & WWidget::AddDecorate( WDecorate * _Decorate ) {
    if ( _Decorate ) {
        if ( _Decorate->Owner ) {
            _Decorate->Owner->RemoveDecorate( _Decorate );
        }
        Decorates.Append( _Decorate );
        _Decorate->Owner = this;
        _Decorate->AddRef();
    }
    return *this;
}

WWidget & WWidget::RemoveDecorate( WDecorate * _Decorate ) {

    int index = Decorates.IndexOf( _Decorate );
    if ( index != -1 ) {
        Decorates.Remove( index );
        _Decorate->Owner = nullptr;
        _Decorate->RemoveRef();
    }

    return *this;
}

WWidget & WWidget::RemoveDecorates() {
    for ( WDecorate * d : Decorates ) {
        d->Owner = nullptr;
        d->RemoveRef();
    }
    Decorates.Clear();
    return *this;
}

Float2 const & WWidget::GetPosition() const {
    return Position;
}

AN_FORCEINLINE void ClampWidgetSize( Float2 & _InOut, Float2 const & _Min, Float2 const & _Max ) {
    if ( _InOut.X < _Min.X ) {
        _InOut.X = _Min.X;
    }
    if ( _InOut.Y < _Min.Y ) {
        _InOut.Y = _Min.Y;
    }
    if ( _Max.X > 0.0f ) {
        if ( _InOut.X > _Max.X ) {
            _InOut.X = _Max.X;
        }
    }
    if ( _Max.Y > 0.0f ) {
        if ( _InOut.Y > _Max.Y ) {
            _InOut.Y = _Max.Y;
        }
    }
}

void WWidget::UpdateTransformIfDirty() {
    if ( bTransformDirty ) {
        UpdateTransform();
    }
}

// Размер окна выбирается таким образом, чтобы на нем поместились все дочерние окна, при этом дочернее окно
// не должно иметь следующие выравнивания: WIDGET_ALIGNMENT_RIGHT, WIDGET_ALIGNMENT_BOTTOM, WIDGET_ALIGNMENT_CENTER, WIDGET_ALIGNMENT_STRETCH.
// Если установлен лэйаут WIDGET_LAYOUT_IMAGE, то размер окна устанавливается равным ImageSize.
// Если установлен лэйаут WIDGET_LAYOUT_GRID, то размер окна устанавливается равным размеру сетки.

//bool WWidget::CanAutoWidth() {
//    if ( Layout == WIDGET_LAYOUT_IMAGE || Layout == WIDGET_LAYOUT_GRID ) {
//        return;
//    }

//    WIDGET_ALIGNMENT_RIGHT, WIDGET_ALIGNMENT_BOTTOM, WIDGET_ALIGNMENT_CENTER, WIDGET_ALIGNMENT_STRETCH.
//}

static void ApplyHorizontalAlignment( EWidgetAlignment _HorizontalAlignment, Float2 const & _AvailSize, Float2 & _Size, Float2 & _Pos ) {
    switch ( _HorizontalAlignment ) {
    case WIDGET_ALIGNMENT_STRETCH:
        _Pos.X = 0;
        _Size.X = _AvailSize.X;
        break;
    case WIDGET_ALIGNMENT_LEFT:
        _Pos.X = 0;
        break;
    case WIDGET_ALIGNMENT_RIGHT:
        _Pos.X = _AvailSize.X - _Size.X;
        break;
    case WIDGET_ALIGNMENT_CENTER: {
        float center = _AvailSize.X * 0.5f;
        _Pos.X = center - _Size.X * 0.5f;
        break;
        }
    default:
        break;
    }
}

static void ApplyVerticalAlignment( EWidgetAlignment _VerticalAlignment, Float2 const & _AvailSize, Float2 & _Size, Float2 & _Pos ) {
    switch ( _VerticalAlignment ) {
    case WIDGET_ALIGNMENT_STRETCH:
        _Pos.Y = 0;
        _Size.Y = _AvailSize.Y;
        break;
    case WIDGET_ALIGNMENT_TOP:
        _Pos.Y = 0;
        break;
    case WIDGET_ALIGNMENT_BOTTOM:
        _Pos.Y = _AvailSize.Y - _Size.Y;
        break;
    case WIDGET_ALIGNMENT_CENTER: {
        float center = _AvailSize.Y * 0.5f;
        _Pos.Y = center - _Size.Y * 0.5f;
        break;
        }
    default:
        break;
    }
}

void WWidget::UpdateTransform() {
    bTransformDirty = false;

    if ( !Parent ) {
        ActualPosition = Position;
        ActualSize = Size;
        ClampWidgetSize( ActualSize, MinSize, MaxSize );
        return;
    }

    Float2 curPos = Position;
    Float2 curSize = Size;

    ClampWidgetSize( curSize, MinSize, MaxSize );

    Float2 availSize;
    Float2 localOffset(0.0f);

    switch ( Parent->Layout ) {
    case WIDGET_LAYOUT_EXPLICIT:
    case WIDGET_LAYOUT_HORIZONTAL:
    case WIDGET_LAYOUT_HORIZONTAL_WRAP:
    case WIDGET_LAYOUT_VERTICAL:
    case WIDGET_LAYOUT_VERTICAL_WRAP:
    case WIDGET_LAYOUT_IMAGE:
    case WIDGET_LAYOUT_CUSTOM:
        availSize = Parent->GetAvailableSize();
        break;
    case WIDGET_LAYOUT_GRID: {
        Float2 cellMins, cellMaxs;
        Parent->GetCellRect( Column, Row, cellMins, cellMaxs );
        availSize = cellMaxs - cellMins;
        localOffset = cellMins;
        break;
        }
    default:
        AN_Assert( 0 );
    }

    if ( !IsMaximized() ) {
        switch ( Parent->Layout ) {
        case WIDGET_LAYOUT_EXPLICIT:
        case WIDGET_LAYOUT_GRID:
            ApplyHorizontalAlignment( HorizontalAlignment, availSize, curSize, curPos );
            ApplyVerticalAlignment( VerticalAlignment, availSize, curSize, curPos );
            break;

        case WIDGET_LAYOUT_IMAGE: {
            Float2 const & imageSize = Parent->GetImageSize();
            Float2 scale = availSize / imageSize;

            curPos = ( curPos * scale + 0.5f ).Floor();
            curSize = ( curSize * scale + 0.5f ).Floor();

            ApplyHorizontalAlignment( HorizontalAlignment, availSize, curSize, curPos );
            ApplyVerticalAlignment( VerticalAlignment, availSize, curSize, curPos );
            break;
            }

        case WIDGET_LAYOUT_HORIZONTAL:
        case WIDGET_LAYOUT_HORIZONTAL_WRAP:
        case WIDGET_LAYOUT_VERTICAL:
        case WIDGET_LAYOUT_VERTICAL_WRAP:
            Parent->UpdateLayoutIfDirty();

            curPos = LayoutOffset;

            if ( Parent->Layout == WIDGET_LAYOUT_HORIZONTAL ) {
                ApplyVerticalAlignment( VerticalAlignment, availSize, curSize, curPos );
            }
            if ( Parent->Layout == WIDGET_LAYOUT_VERTICAL ) {
                ApplyHorizontalAlignment( HorizontalAlignment, availSize, curSize, curPos );
            }
            break;

        case WIDGET_LAYOUT_CUSTOM:
            AdjustSizeAndPosition( availSize, curSize, curPos );
            break;

        default:
            AN_Assert( 0 );
        }

        if ( bClampWidth && curPos.X + curSize.X > availSize.X ) {
            curSize.X = FMath::Max( 0.0f, availSize.X - curPos.X );
        }
        if ( bClampWidth && curPos.Y + curSize.Y > availSize.Y ) {
            curSize.Y = FMath::Max( 0.0f, availSize.Y - curPos.Y );
        }

        curPos += localOffset;

    } else {
        curPos = localOffset;
        curSize = availSize;
    }

    // from local to desktop
    ActualPosition = curPos + Parent->GetClientPosition();
    ActualSize = curSize;
}

Float2 WWidget::GetDesktopPosition() const {
    const_cast< WWidget * >( this )->UpdateTransformIfDirty();

    return ActualPosition;
}

Float2 WWidget::GetClientPosition() const {
    return GetDesktopPosition() + Margin.Shuffle2< sXY >();
}

Float2 WWidget::GetCurrentSize() const {
    const_cast< WWidget * >( this )->UpdateTransformIfDirty();

    return ActualSize;
}

float WWidget::GetAvailableWidth() const {
    Float2 sz = GetCurrentSize();
    return FMath::Max( sz.X - Margin.X - Margin.Z, 0.0f );
}

float WWidget::GetAvailableHeight() const {
    Float2 sz = GetCurrentSize();
    return FMath::Max( sz.Y - Margin.Y - Margin.W, 0.0f );
}

Float2 WWidget::GetAvailableSize() const {
    Float2 sz = GetCurrentSize();
    return Float2( FMath::Max( sz.X - Margin.X - Margin.Z, 0.0f ),
                   FMath::Max( sz.Y - Margin.Y - Margin.W, 0.0f ) );
}

void WWidget::GetDesktopRect( Float2 & _Mins, Float2 & _Maxs, bool _Margin ) {
    _Mins = GetDesktopPosition();
    _Maxs = _Mins + GetCurrentSize();
    if ( _Margin ) {
        _Mins.X += Margin.X;
        _Mins.Y += Margin.Y;
        _Maxs.X -= Margin.Z;
        _Maxs.Y -= Margin.W;
    }
}

void WWidget::FromClientToDesktop( Float2 & InOut ) const {
    InOut += GetClientPosition();
}

void WWidget::FromDesktopToClient( Float2 & InOut ) const {
    InOut -= GetClientPosition();
}

void WWidget::FromDesktopToWidget( Float2 & InOut ) const {
    InOut -= GetDesktopPosition();
}

void WWidget::GetGridOffset( int & _Column, int & _Row ) const {
    _Column = Column;
    _Row = Row;
}

WWidget & WWidget::SetVisibility( EWidgetVisibility _Visibility ) {
    if ( Visibility != _Visibility ) {
        Visibility = _Visibility;

        // Mark transforms only for collapsed and visible widgets
        if ( Visibility != WIDGET_VISIBILITY_INVISIBLE ) {
            if ( Parent ) {
                // Mark all childs in parent widget to update collapsed/uncollapsed visibility
                Parent->bLayoutDirty = true;
                Parent->MarkTransformDirtyChilds();
            } else {
                MarkTransformDirty();
            }
        }
    }
    return *this;
}

WWidget & WWidget::SetMaximized() {
    if ( bMaximized ) {
        return *this;
    }

    bMaximized = true;

    // TODO: bMaximizedAnimation = true

    MarkTransformDirty();

    return *this;
}

WWidget & WWidget::SetNormal() {
    if ( bMaximized ) {
        bMaximized = false;
        MarkTransformDirty();
    }

    return *this;
}

bool WWidget::IsMaximized() const {
    return bMaximized;
}

bool WWidget::IsDisabled() const {
    for ( WWidget const * w = this ; w && !w->IsRoot() ; w = w->Parent ) {
        if ( w->bDisabled ) {
            return true;
        }
    }
    return false;
}

WWidget & WWidget::BringOnTop( bool _RecursiveForParents ) {
    if ( !Parent ) {
        return *this;
    }

    if ( !(Style & WIDGET_STYLE_BACKGROUND) ) {
        if ( Style & WIDGET_STYLE_FOREGROUND ) {

            if ( AN_HASFLAG( Style, WIDGET_STYLE_POPUP ) ) {
                // bring on top
                if ( Parent->Childs.Last() != this ) {
                    Parent->Childs.Remove( Parent->Childs.IndexOf( this ) );
                    Parent->Childs.Append( this );
                }
            } else {
                int i;

                // skip popup widgets
                for ( i = Parent->Childs.Size() - 1 ; i >= 0 && AN_HASFLAG( Parent->Childs[ i ]->GetStyle(), WIDGET_STYLE_POPUP ) ; i-- ) {}

                // bring before popup widgets
                if ( i >= 0 && Parent->Childs[i] != this ) {
                    int index = Parent->Childs.IndexOf( this );
                    Parent->Childs.Remove( index );
                    Parent->Childs.Insert( i, this );
                }
            }
        } else {
            int i;

            // skip foreground widgets
            for ( i = Parent->Childs.Size() - 1 ; i >= 0 && AN_HASFLAG( Parent->Childs[ i ]->GetStyle(), WIDGET_STYLE_FOREGROUND ) ; i-- ) {}

            // bring before foreground widgets
            if ( i >= 0 && Parent->Childs[i] != this ) {
                int index = Parent->Childs.IndexOf( this );
                Parent->Childs.Remove( index );
                Parent->Childs.Insert( i, this );
            }
        }
    }

    if ( _RecursiveForParents ) {
        Parent->BringOnTop();
    }

    return *this;
}

bool WWidget::IsHovered( Float2 const & _Position ) const {
    WDesktop * desktop = const_cast< WWidget * >( this )->GetDesktop();
    if ( !desktop ) {
        return false;
    }

    WWidget * w = desktop->GetWidgetUnderCursor( _Position );
    return w == this;
}

bool WWidget::IsHoveredByCursor() const {
    WDesktop * desktop = const_cast< WWidget * >( this )->GetDesktop();
    if ( !desktop ) {
        return false;
    }

    WWidget * w = desktop->GetWidgetUnderCursor( desktop->GetCursorPosition() );
    return w == this;
}

AN_FORCEINLINE void ApplyMargins( Float2 & _Mins, Float2 & _Maxs, Float4 const & _Margins ) {
    _Mins.X += _Margins.X;
    _Mins.Y += _Margins.Y;
    _Maxs.X -= _Margins.Z;
    _Maxs.Y -= _Margins.W;
}

void WWidget::Draw_r( FCanvas & _Canvas, Float2 const & _ClipMins, Float2 const & _ClipMaxs ) {
    if ( !IsVisible() ) {
        return;
    }

    Float2 rectMins, rectMaxs;
    Float2 mins, maxs;

    GetDesktopRect( rectMins, rectMaxs, false );

    mins.X = FMath::Max( rectMins.X, _ClipMins.X );
    mins.Y = FMath::Max( rectMins.Y, _ClipMins.Y );

    maxs.X = FMath::Min( rectMaxs.X, _ClipMaxs.X );
    maxs.Y = FMath::Min( rectMaxs.Y, _ClipMaxs.Y );

    if ( mins.X >= maxs.X || mins.Y >= maxs.Y ) {
        return; // invalid rect
    }

    _Canvas.PushClipRect( mins, maxs );

    OnDrawEvent( _Canvas );

    _Canvas.PopClipRect();

    ApplyMargins( rectMins, rectMaxs, GetMargin() );

    mins.X = FMath::Max( rectMins.X, _ClipMins.X );
    mins.Y = FMath::Max( rectMins.Y, _ClipMins.Y );

    maxs.X = FMath::Min( rectMaxs.X, _ClipMaxs.X );
    maxs.Y = FMath::Min( rectMaxs.Y, _ClipMaxs.Y );

    if ( mins.X >= maxs.X || mins.Y >= maxs.Y ) {
        return; // invalid rect
    }

    if ( Layout == WIDGET_LAYOUT_GRID ) {
        Float2 cellMins, cellMaxs;
        Float2 clientPos = GetClientPosition();

        // TODO: Draw grid borders here

        for ( WWidget * child : GetChilds() ) {
            int columnIndex;
            int rowIndex;

            child->GetGridOffset( columnIndex, rowIndex );

            GetCellRect( columnIndex, rowIndex, cellMins, cellMaxs );
            cellMins += clientPos;
            cellMaxs += clientPos;

            cellMins.X = FMath::Max( cellMins.X, mins.X );
            cellMins.Y = FMath::Max( cellMins.Y, mins.Y );

            cellMaxs.X = FMath::Min( cellMaxs.X, maxs.X );
            cellMaxs.Y = FMath::Min( cellMaxs.Y, maxs.Y );

            if ( cellMins.X >= cellMaxs.X || cellMins.Y >= cellMaxs.Y ) {
                continue;
            }

            child->Draw_r( _Canvas, cellMins, cellMaxs );
        }
    } else {
        for ( WWidget * child : Childs ) {
            child->Draw_r( _Canvas, mins, maxs );
        }
    }
}

void WWidget::OnKeyEvent( struct FKeyEvent const & _Event, double _TimeStamp ) {

}

void WWidget::OnMouseButtonEvent( struct FMouseButtonEvent const & _Event, double _TimeStamp ) {

}

void WWidget::OnDblClickEvent( int _ButtonKey, Float2 const & _ClickPos, uint64_t _ClickTime ) {

}

void WWidget::OnMouseWheelEvent( struct FMouseWheelEvent const & _Event, double _TimeStamp ) {

}

void WWidget::OnMouseMoveEvent( struct FMouseMoveEvent const & _Event, double _TimeStamp ) {

}

void WWidget::OnCharEvent( struct FCharEvent const & _Event, double _TimeStamp ) {

}

void WWidget::OnDragEvent( Float2 & _Position ) {

}

void WWidget::OnFocusLost() {

}

void WWidget::OnFocusReceive() {

}

void WWidget::OnDrawEvent( FCanvas & _Canvas ) {
    DrawDecorates( _Canvas );
}

void WWidget::OnTransformDirty() {

}

void WWidget::AdjustSizeAndPosition( Float2 const & _AvailableSize, Float2 & _Size, Float2 & _Position ) {

}

void WWidget::DrawDecorates( FCanvas & _Canvas ) {
    for ( WDecorate * d : Decorates ) {
        d->OnDrawEvent( _Canvas );
    }
}

void WWidget::MarkGridLayoutDirty() {
    if ( Layout == WIDGET_LAYOUT_GRID ) {
        bLayoutDirty = true;
        MarkTransformDirtyChilds();
    }
}

void WWidget::MarkVHLayoutDirty() {
    if ( Layout == WIDGET_LAYOUT_HORIZONTAL
         || Layout == WIDGET_LAYOUT_HORIZONTAL_WRAP
         || Layout == WIDGET_LAYOUT_VERTICAL
         || Layout == WIDGET_LAYOUT_VERTICAL_WRAP ) {
        bLayoutDirty = true;
        MarkTransformDirtyChilds();
    }
}

void WWidget::MarkImageLayoutDirty() {
    if ( Layout == WIDGET_LAYOUT_IMAGE ) {
        bLayoutDirty = true;
        MarkTransformDirtyChilds();
    }
}

WWidget & WWidget::SetGridSize( int _ColumnsCount, int _RowsCount ) {
    ColumnsCount = FMath::Clamp( _ColumnsCount, 1, MAX_COLUMNS );
    RowsCount = FMath::Clamp( _RowsCount, 1, MAX_ROWS );
    MarkGridLayoutDirty();
    return *this;
}

WWidget & WWidget::SetColumnWidth( int _ColumnIndex, float _Width ) {
    if ( _ColumnIndex < 0 || _ColumnIndex >= ColumnsCount ) {
        return *this;
    }

    int oldSize = Columns.Size();

    if ( oldSize <= _ColumnIndex ) {
        Columns.Resize( _ColumnIndex + 1 );

        memset( Columns.ToPtr() + oldSize, 0, ( Columns.Size() - oldSize ) * sizeof( Columns[0] ) );
    }

    Columns[ _ColumnIndex ].Size = FMath::Max( _Width, 0.0f );

    MarkGridLayoutDirty();

    return *this;
}

WWidget & WWidget::SetRowWidth( int _RowIndex, float _Width ) {
    if ( _RowIndex < 0 || _RowIndex >= RowsCount ) {
        return *this;
    }

    int oldSize = Rows.Size();

    if ( oldSize <= _RowIndex ) {
        Rows.Resize( _RowIndex + 1 );

        memset( Rows.ToPtr() + oldSize, 0, ( Rows.Size() - oldSize ) * sizeof( Rows[0] ) );
    }

    Rows[ _RowIndex ].Size = FMath::Max( _Width, 0.0f );

    MarkGridLayoutDirty();

    return *this;
}

WWidget & WWidget::SetFitColumns( bool _FitColumns ) {
    if ( bFitColumns != _FitColumns ) {
        bFitColumns = _FitColumns;
        MarkGridLayoutDirty();
    }
    return *this;
}

WWidget & WWidget::SetFitRows( bool _FitRows ) {
    if ( bFitRows != _FitRows ) {
        bFitRows = _FitRows;
        MarkGridLayoutDirty();
    }
    return *this;
}

void WWidget::GetCellRect( int _ColumnIndex, int _RowIndex, Float2 & _Mins, Float2 & _Maxs ) const {
    const_cast< WWidget * >( this )->UpdateLayoutIfDirty();

    int numColumns = FMath::Min( ColumnsCount, Columns.Size() );
    int numRows = FMath::Min( RowsCount, Rows.Size() );

    if ( _ColumnIndex < 0 || _RowIndex < 0 || numColumns == 0 || numRows == 0 ) {
        _Mins = _Maxs = Float2(0.0f);
        return;
    }

    int col = _ColumnIndex, row = _RowIndex;

    _Mins.X = Columns[ col >= numColumns ? numColumns - 1 : col ].Offset;
    _Mins.Y = Rows[ row >= numRows ? numRows - 1 : row ].Offset;
    _Maxs.X = _Mins.X + ( col >= numColumns ? 0.0f : Columns[ col ].ActualSize );
    _Maxs.Y = _Mins.Y + ( row >= numRows ? 0.0f : Rows[ row ].ActualSize );
}

WWidget & WWidget::SetClampWidth( bool _ClampWidth ) {
    if ( bClampWidth != _ClampWidth ) {
        bClampWidth = _ClampWidth;
        MarkTransformDirty();
    }
    return *this;
}

WWidget & WWidget::SetClampHeight( bool _ClampHeight ) {
    if ( bClampHeight != _ClampHeight ) {
        bClampHeight = _ClampHeight;
        MarkTransformDirty();
    }
    return *this;
}

WWidget & WWidget::SetHorizontalPadding( float _Padding ) {
    HorizontalPadding = _Padding;
    MarkVHLayoutDirty();
    return *this;
}

WWidget & WWidget::SetVerticalPadding( float _Padding ) {
    VerticalPadding = _Padding;
    MarkVHLayoutDirty();
    return *this;
}

WWidget & WWidget::SetImageSize( float _Width, float _Height ) {
    return SetImageSize( Float2( _Width, _Height ) );
}

WWidget & WWidget::SetImageSize( Float2 const & _ImageSize ) {
    ImageSize = _ImageSize;
    if ( ImageSize.X < 1 ) {
        ImageSize.X = 1;
    }
    if ( ImageSize.Y < 1 ) {
        ImageSize.Y = 1;
    }
    MarkImageLayoutDirty();
    return *this;
}

void WWidget::GetLayoutRect( Float2 & _Mins, Float2 & _Maxs ) const {
    if ( !Parent ) {
        _Mins.Clear();
        _Maxs.Clear();
        return;
    }

    switch ( Parent->Layout ) {
    case WIDGET_LAYOUT_EXPLICIT:
    case WIDGET_LAYOUT_HORIZONTAL:
    case WIDGET_LAYOUT_HORIZONTAL_WRAP:
    case WIDGET_LAYOUT_VERTICAL:
    case WIDGET_LAYOUT_VERTICAL_WRAP:
    case WIDGET_LAYOUT_IMAGE:
    case WIDGET_LAYOUT_CUSTOM:
        Parent->GetDesktopRect( _Mins, _Maxs, true );
        break;
    case WIDGET_LAYOUT_GRID: {
        Parent->GetCellRect( Column, Row, _Mins, _Maxs );

        Float2 pos = Parent->GetClientPosition();
        _Mins += pos;
        _Maxs += pos;
        }
        break;
    default:
        AN_Assert( 0 );
        Parent->GetDesktopRect( _Mins, _Maxs, true );
        break;
    }
}

void WWidget::MarkTransformDirty() {
    WWidget * node = this;
    WWidget * nextNode;
    int numChilds;
    while ( !node->bTransformDirty ) {
        node->bTransformDirty = true;
        node->bLayoutDirty = true;
        node->OnTransformDirty();
        numChilds = node->Childs.Size();
        if ( numChilds > 0 ) {
            nextNode = node->Childs[ 0 ];
            for ( int i = 1 ; i < numChilds ; i++ ) {
                node->Childs[ i ]->MarkTransformDirty();
            }
            node = nextNode;
        } else {
            return;
        }
    }
}

void WWidget::MarkTransformDirtyChilds() {
    for ( WWidget * child : Childs ) {
        child->MarkTransformDirty();
    }
}

void WWidget::UpdateLayoutIfDirty() {
    if ( bLayoutDirty ) {
        UpdateLayout();
    }
}

void WWidget::UpdateLayout() {
    bLayoutDirty = false;

    if ( Layout == WIDGET_LAYOUT_GRID ) {
        int numColumns = FMath::Min( ColumnsCount, Columns.Size() );
        int numRows = FMath::Min( RowsCount, Rows.Size() );

        if ( bFitColumns ) {
            float sumWidth = 0;
            for ( int i = 0; i < numColumns; i++ ) {
                sumWidth += Columns[ i ].Size;
            }
            float norm = ( sumWidth > 0 ) ? GetAvailableWidth() / sumWidth : 0;
            for ( int i = 0; i < numColumns; i++ ) {
                Columns[ i ].ActualSize = Columns[ i ].Size * norm;
            }
        } else {
            for ( int i = 0; i < numColumns; i++ ) {
                Columns[ i ].ActualSize = Columns[ i ].Size;
            }
        }

        Columns[ 0 ].Offset = 0;
        for ( int i = 1 ; i < numColumns; i++ ) {
            Columns[ i ].Offset = Columns[ i - 1 ].Offset + Columns[ i - 1 ].ActualSize;
        }

        if ( bFitRows ) {
            float sumWidth = 0;
            for ( int i = 0; i < numRows; i++ ) {
                sumWidth += Rows[ i ].Size;
            }
            float norm = ( sumWidth > 0 ) ? GetAvailableHeight() / sumWidth : 0;
            for ( int i = 0; i < numRows; i++ ) {
                Rows[ i ].ActualSize = Rows[ i ].Size * norm;
            }
        } else {
            for ( int i = 0; i < RowsCount; i++ ) {
                Rows[ i ].ActualSize = Rows[ i ].Size;
            }
        }

        Rows[ 0 ].Offset = 0;
        for ( int i = 1 ; i < numRows; i++ ) {
            Rows[ i ].Offset = Rows[ i - 1 ].Offset + Rows[ i - 1 ].ActualSize;
        }
    } else if ( Layout == WIDGET_LAYOUT_HORIZONTAL || Layout == WIDGET_LAYOUT_HORIZONTAL_WRAP ) {
        float offsetX = 0;
        float offsetY = 0;
        float width = GetAvailableWidth();
        float maxHeight = 0;
        Float2 sz;

        for ( int i = 0 ; i < LayoutSlots.Size() ; i++ ) {
            WWidget * w = LayoutSlots[i];
            WWidget * next = (i == LayoutSlots.Size()-1) ? nullptr : LayoutSlots[i+1];

            if ( w->IsCollapsed() ) {
                continue;
            }

            sz = w->GetSize();

            w->LayoutOffset.X = offsetX;
            w->LayoutOffset.Y = offsetY;

            offsetX += sz.X + HorizontalPadding;

            if ( Layout == WIDGET_LAYOUT_HORIZONTAL_WRAP && next ) {
                maxHeight = FMath::Max( maxHeight, sz.Y );
                if ( offsetX + next->GetWidth() >= width ) {
                    offsetX = 0;
                    offsetY += maxHeight + VerticalPadding;
                    maxHeight = 0;
                }
            }
        }
    } else if ( Layout == WIDGET_LAYOUT_VERTICAL || Layout == WIDGET_LAYOUT_VERTICAL_WRAP ) {
        float offsetX = 0;
        float offsetY = 0;
        float height = GetAvailableHeight();
        float maxWidth = 0;
        Float2 sz;

        for ( int i = 0 ; i < LayoutSlots.Size() ; i++ ) {
            WWidget * w = LayoutSlots[i];
            WWidget * next = (i == LayoutSlots.Size()-1) ? nullptr : LayoutSlots[i+1];

            if ( w->IsCollapsed() ) {
                continue;
            }

            sz = w->GetSize();

            w->LayoutOffset.X = offsetX;
            w->LayoutOffset.Y = offsetY;

            offsetY += sz.Y + VerticalPadding;

            if ( Layout == WIDGET_LAYOUT_VERTICAL_WRAP && next ) {
                maxWidth = FMath::Max( maxWidth, sz.X );
                if ( offsetY + next->GetHeight() >= height ) {
                    offsetY = 0;
                    offsetX += maxWidth + HorizontalPadding;
                    maxWidth = 0;
                }
            }
        }
    }
}












#include <Engine/Widgets/Public/WWindow.h>
#include <Engine/Widgets/Public/WButton.h>
WWidget & ScrollTest() {
    return  WWidget::New< WWindow >()
            .SetCaptionText( "Test Scroll" )
            .SetCaptionHeight( 30 )
            .SetBackgroundColor( FColor4( 0.5f,0.5f,0.5f ) )
            .SetStyle( WIDGET_STYLE_RESIZABLE )
            .SetSize( 400, 300 )
            .SetLayout( WIDGET_LAYOUT_GRID )
            .SetGridSize( 2, 1 )
            .SetColumnWidth( 0, 270 )
            .SetColumnWidth( 1, 30 )
            .SetRowWidth( 0, 1 )
            .SetFitColumns( true )
            .SetFitRows( true )
#if 0
            [
                WWidget::New()
                .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetGridOffset( 0, 0 )
                .SetLayout( WIDGET_LAYOUT_IMAGE )
                .SetImageSize( 640, 480 )
                [
                    WDecorate::New< WBorderDecorate >()
                    .SetColor( 0xff00ffff )
                    .SetFillBackground( true )
                    .SetBackgroundColor( 0xff00ff00 )
                    .SetThickness( 1 )
                ]
                [
                    WDecorate::New< WTextDecorate >()
                    .SetText( "Content view" )
                    .SetColor( 0xff000000 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "1" )
                    .SetSize( 64, 64 )
                    .SetPosition( 640-64, 480-64 )
                    //.SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    //.SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                    .SetEnabled( false )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "2" )
                    .SetSize( 64, 64 )
                    .SetPosition( 640/2-64/2, 480/2-64/2 )
                    //.SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    //.SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
            ]
#endif
#if 1
            [
                WWidget::New()
                .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetGridOffset( 0, 0 )
                .SetLayout( WIDGET_LAYOUT_HORIZONTAL )
                .SetHorizontalPadding( 8 )
                .SetVerticalPadding( 4 )
                [
                    WDecorate::New< WBorderDecorate >()
                    .SetColor( FColor4( 1,1,0 ) )
                    .SetFillBackground( true )
                    .SetBackgroundColor( FColor4( 0,1,0 ) )
                    .SetThickness( 1 )
                ]
                [
                    WDecorate::New< WTextDecorate >()
                    .SetText( "Content view" )
                    .SetColor( FColor4::Black() )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "1" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "2" )
                    .SetSize( 200, 50 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "3" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                    //.SetCollapsed()
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "4" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "5" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_TOP )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "6" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_BOTTOM )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "7" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "8" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "9" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_BOTTOM )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "10" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "11" )
                    .SetSize( 100, 30 )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                ]
            ]
#endif
            [
                WWidget::New()
                .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                .SetGridOffset( 1, 0 )
                .SetGridSize( 1, 3 )
                .SetColumnWidth( 0, 1 )
                .SetRowWidth( 0, 0.2f )
                .SetRowWidth( 1, 0.6f )
                .SetRowWidth( 2, 0.2f )
                .SetFitColumns( true )
                .SetFitRows( true )
                .SetLayout( WIDGET_LAYOUT_GRID )
                [
                    WDecorate::New< WBorderDecorate >()
                    .SetColor( FColor4( 1,0,0 ) )
                    .SetFillBackground( true )
                    .SetBackgroundColor( FColor4( 1,0,1 ) )
                    .SetThickness( 1 )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "Up" )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetGridOffset( 0, 0 )
                ]
                [
                    WWidget::New< WButton >()
                    .SetText( "Down" )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetGridOffset( 0, 2 )
                ]
                [
                    WWidget::New()
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_CENTER )
                    .SetStyle( WIDGET_STYLE_FOREGROUND )
                    .SetHorizontalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetVerticalAlignment( WIDGET_ALIGNMENT_STRETCH )
                    .SetGridOffset( 0, 1 )
                    .AddDecorate( &(*CreateInstanceOf< WBorderDecorate >())
                                  .SetColor( FColor4( 0,1,0 ) )
                                  .SetFillBackground( true )
                                  .SetBackgroundColor( FColor4::Black() )
                                  .SetThickness( 1 )
                                  .SetRounding( 0 )
                                  .SetRoundingCorners( CORNER_ROUND_NONE ) )
                ]
            ];
}




#if 0
AN_CLASS_META( WMenuItem )

WMenuItem::WMenuItem() {
    State = ST_RELEASED;
    Color = FColor4::White();
    HoverColor = FColor4( 1,1,0.5f,1 );
    PressedColor = FColor4( 1,1,0.2f,1 );
    TextColor = FColor4::Black();
    BorderColor = FColor4::Black();
    Rounding = 8;
    RoundingCorners = CORNER_ROUND_ALL;
    BorderThickness = 1;
}

WMenuItem::~WMenuItem() {
}

WMenuItem & WMenuItem::SetText( const char * _Text ) {
    Text = _Text;
    return *this;
}

WMenuItem & WMenuItem::SetColor( FColor4 const & _Color ) {
    Color = _Color;
    return *this;
}

WMenuItem & WMenuItem::SetHoverColor( FColor4 const & _Color ) {
    HoverColor = _Color;
    return *this;
}

WMenuItem & WMenuItem::SetPressedColor( FColor4 const & _Color ) {
    PressedColor = _Color;
    return *this;
}

WMenuItem & WMenuItem::SetTextColor( FColor4 const & _Color ) {
    TextColor = _Color;
    return *this;
}

WMenuItem & WMenuItem::SetBorderColor( FColor4 const & _Color ) {
    BorderColor = _Color;
    return *this;
}

WMenuItem & WMenuItem::SetRounding( float _Rounding ) {
    Rounding = _Rounding;
    return *this;
}

WMenuItem & WMenuItem::SetRoundingCorners( EDrawCornerFlags _RoundingCorners ) {
    RoundingCorners = _RoundingCorners;
    return *this;
}

WMenuItem & WMenuItem::SetBorderThickness( float _BorderThickness ) {
    BorderThickness = _BorderThickness;
    return *this;
}

void WMenuItem::OnMouseButtonEvent( struct FMouseButtonEvent const & _Event, double _TimeStamp ) {
    if ( _Event.Action == IE_Press ) {
        if ( _Event.Button == 0 ) {
            State = ST_PRESSED;
        }
    } else if ( _Event.Action == IE_Release ) {
        if ( _Event.Button == 0 && State == ST_PRESSED && IsHoveredByCursor() ) {

            State = ST_RELEASED;

            E_OnButtonClick.Dispatch();
        } else {
            State = ST_RELEASED;
        }
    }
}

void WMenuItem::OnDrawEvent( FCanvas & _Canvas ) {
    FColor4 bgColor;

    if ( IsHoveredByCursor() && !IsDisabled() ) {
        if ( State == ST_PRESSED ) {
            bgColor = PressedColor;
        } else {
            bgColor = HoverColor;
        }
    } else {
        bgColor = Color;
    }

    Float2 mins, maxs;

    GetDesktopRect( mins, maxs, true );

    FFont * font = _Canvas.GetDefaultFont();

    float width = GetClientWidth();
    float height = GetClientHeight();

    Float2 size = font->CalcTextSizeA( font->FontSize, width, 0, Text.Begin(), Text.End() );

    _Canvas.DrawRectFilled( mins, maxs, bgColor, Rounding, RoundingCorners );
    if ( BorderThickness > 0.0f ) {
        _Canvas.DrawRect( mins, maxs, BorderColor, Rounding, RoundingCorners, BorderThickness );
    }
    _Canvas.DrawTextUTF8( mins + Float2( width - size.X, height - size.Y ) * 0.5f, TextColor, Text.Begin(), Text.End() );
}
#endif