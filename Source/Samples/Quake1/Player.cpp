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

#include "Player.h"
#include "Game.h"

#include <Engine/World/Public/CameraComponent.h>
#include <Engine/World/Public/InputComponent.h>

AN_BEGIN_CLASS_META( FPlayer )
AN_END_CLASS_META()

FPlayer::FPlayer() {
    Camera = CreateComponent< FCameraComponent >( "Camera" );
    RootComponent = Camera;

    // Animation single frame holder
    WeaponModel = CreateComponent< FQuakeModelFrame >( "Frame" );

    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_axe.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_shot.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_shot2.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_nail.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_nail2.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_rock.mdl");
    FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_rock2.mdl");
    //FQuakeModel * model = GGameModule->LoadQuakeModel( "progs/v_light.mdl");

    WeaponModel->SetModel( model );

    WeaponFramesCount = model ? model->Frames.Length() : 0;

    FMaterialInstance * matInst = NewObject< FMaterialInstance >();
    matInst->Material = GGameModule->SkinMaterial;

    WeaponModel->SetMaterialInstance( matInst );

    if ( model && !model->Skins.IsEmpty() ) {
        // Set random skin (just for fun)
        matInst->SetTexture( 0, model->Skins[rand()%model->Skins.Length()].Texture );
    }

    WeaponModel->AttachTo( RootComponent );
    WeaponModel->SetAngles(0,180,0);

    bCanEverTick = true;
}

void FPlayer::PreInitializeComponents() {
    Super::PreInitializeComponents();
}

void FPlayer::PostInitializeComponents() {
    Super::PostInitializeComponents();
}

void FPlayer::BeginPlay() {
    Super::BeginPlay();

    Float3 vec = RootComponent->GetBackVector();
    Float2 projected( vec.X, vec.Z );
    float lenSqr = projected.LengthSqr();
    if ( lenSqr < 0.0001 ) {
        vec = RootComponent->GetRightVector();
        projected.X = vec.X;
        projected.Y = vec.Z;
//        float lenSqr = projected.LengthSqr();
//        if ( lenSqr < 0.0001 ) {

//        } else {
            projected.NormalizeSelf();
            Angles.Yaw = FMath::Degrees( atan2( projected.X, projected.Y ) ) + 90;
//        }
    } else {
        projected.NormalizeSelf();
        Angles.Yaw = FMath::Degrees( atan2( projected.X, projected.Y ) );
    }

    Angles.Pitch = Angles.Roll = 0;

    RootComponent->SetAngles( Angles );
}

void FPlayer::EndPlay() {
    Super::EndPlay();
}

void FPlayer::SetupPlayerInputComponent( FInputComponent * _Input ) {
    _Input->BindAxis( "MoveForward", this, &FPlayer::MoveForward );
    _Input->BindAxis( "MoveRight", this, &FPlayer::MoveRight );
    _Input->BindAxis( "MoveUp", this, &FPlayer::MoveUp );
    _Input->BindAxis( "MoveDown", this, &FPlayer::MoveDown );
    _Input->BindAxis( "TurnRight", this, &FPlayer::TurnRight );
    _Input->BindAxis( "TurnUp", this, &FPlayer::TurnUp );
    _Input->BindAction( "Speed", IE_Press, this, &FPlayer::SpeedPress );
    _Input->BindAction( "Speed", IE_Release, this, &FPlayer::SpeedRelease );
}

void FPlayer::Tick( float _TimeStep ) {
    Super::Tick( _TimeStep );

    constexpr float PLAYER_MOVE_SPEED = 10.0f; // Meters per second
    constexpr float PLAYER_MOVE_HIGH_SPEED = 20.0f;

    const float MoveSpeed = _TimeStep * ( bSpeed ? PLAYER_MOVE_HIGH_SPEED : PLAYER_MOVE_SPEED );
    float lenSqr = MoveVector.LengthSqr();
    if ( lenSqr > 0 ) {

        if ( lenSqr > 1 ) {
            MoveVector.NormalizeSelf();
        }

        Float3 dir = MoveVector * MoveSpeed;

        RootComponent->Step( dir );

        MoveVector.Clear();
    }


    //if ( WeaponFramesCount > 0 ) {
    //    int keyFrame = AnimationTime.Floor();
    //    float lerp = AnimationTime.Fract();

    //    WeaponModel->SetFrame( keyFrame % WeaponFramesCount, ( keyFrame + 1 ) % WeaponFramesCount, lerp );
    //}

    //constexpr float ANIMATION_SPEED = 10.0f; // frames per second

    //AnimationTime += _TimeStep * ANIMATION_SPEED;
}

void FPlayer::MoveForward( float _Value ) {
    MoveVector += RootComponent->GetForwardVector() * _Value;
}

void FPlayer::MoveRight( float _Value ) {
    MoveVector += RootComponent->GetRightVector() * _Value;
}

void FPlayer::MoveUp( float _Value ) {
    MoveVector.Y += _Value;
}

void FPlayer::MoveDown( float _Value ) {
    MoveVector.Y -= _Value;
}

void FPlayer::TurnRight( float _Value ) {
    Angles.Yaw -= _Value * 0.5f;
    Angles.Yaw = Angl::Normalize180( Angles.Yaw );
    RootComponent->SetAngles( Angles );
}

void FPlayer::TurnUp( float _Value ) {
    Angles.Pitch += _Value * 0.5f;
    Angles.Pitch = Angles.Pitch.Clamp( -90.0f, 90.0f );
    RootComponent->SetAngles( Angles );
}

void FPlayer::SpeedPress() {
    extern FAtomicBool GSyncFrame;
    GSyncFrame.Store( !GSyncFrame.Load() );
    if ( GSyncFrame.Load() ) {
        GLogger.Printf( "Sync frame ON\n" );
    } else {
        GLogger.Printf( "Sync frame OFF\n" );
    }
    //bSpeed = true;
}

void FPlayer::SpeedRelease() {
    //bSpeed = false;
}
