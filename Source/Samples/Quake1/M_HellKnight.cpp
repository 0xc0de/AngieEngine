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

#include "M_HellKnight.h"
#include "Game.h"

AN_BEGIN_CLASS_META( M_HellKnight )
AN_END_CLASS_META()

M_HellKnight::M_HellKnight() {
    // Animation single frame holder
    Frame = CreateComponent< FQuakeModelFrame >( "Frame" );

    FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/hknight.mdl" );
    Frame->SetModel( model );

    FramesCount = model ? model->Frames.Length() : 0;

    FMaterialInstance * matInst = NewObject< FMaterialInstance >();
    matInst->Material = GGameModule->SkinMaterial;

    Frame->SetMaterialInstance( matInst );

    if ( model && !model->Skins.IsEmpty() ) {
        // Set random skin (just for fun)
        matInst->SetTexture( 0, model->Skins[rand()%model->Skins.Length()].Texture );
    }

    // Set root component
    RootComponent = Frame;

    AnimationTime = FMath::Rand() * 100.0f;

    bCanEverTick = true;
}

void M_HellKnight::Tick( float _TimeStep ) {
    Super::Tick( _TimeStep );

    if ( FramesCount > 0 ) {
        int keyFrame = AnimationTime.Floor();
        float lerp = AnimationTime.Fract();

        Frame->SetFrame( keyFrame % FramesCount, ( keyFrame + 1 ) % FramesCount, lerp );
    }

    constexpr float ANIMATION_SPEED = 10.0f; // frames per second

    AnimationTime += _TimeStep * ANIMATION_SPEED;
}