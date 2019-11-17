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

#include <Engine/World/Public/Actors/PlayerController.h>
#include <Engine/World/Public/Components/InputComponent.h>
#include <Engine/World/Public/World.h>
#include <Engine/Runtime/Public/Runtime.h>
#include <Engine/GameThread/Public/EngineInstance.h>

AN_CLASS_META( APlayerController )
AN_CLASS_META( ARenderingParameters )

APlayerController * APlayerController::CurrentAudioListener = nullptr;
ACommandContext * APlayerController::CurrentCommandContext = nullptr;

APlayerController::APlayerController() {
    InputComponent = CreateComponent< AInputComponent >( "PlayerControllerInput" );

    bCanEverTick = true;

    ViewportSize.X = 512;
    ViewportSize.Y = 512;

    if ( !CurrentAudioListener ) {
        CurrentAudioListener = this;
    }

    if ( !CurrentCommandContext ) {
        CurrentCommandContext = &CommandContext;
    }

    CommandContext.AddCommand( "quit", { this, &APlayerController::Quit }, "Quit from application" );
}

void APlayerController::Quit( ARuntimeCommandProcessor const & _Proc ) {
    GRuntime.PostTerminateEvent();
}

void APlayerController::EndPlay() {
    Super::EndPlay();

    //for ( int i = 0 ; i < ViewActors.Size() ; i++ ) {
    //    FViewActor * viewer = ViewActors[i];
    //    viewer->RemoveRef();
    //}

    if ( CurrentAudioListener == this ) {
        CurrentAudioListener = nullptr;
    }

    if ( CurrentCommandContext == &CommandContext ) {
        CurrentCommandContext = nullptr;
    }
}

void APlayerController::Tick( float _TimeStep ) {
    Super::Tick( _TimeStep );

    if ( Pawn && Pawn->IsPendingKill() ) {
        SetPawn( nullptr );
    }
}

void APlayerController::SetPawn( APawn * _Pawn ) {
    if ( Pawn == _Pawn ) {
        return;
    }

    if ( _Pawn && _Pawn->OwnerController ) {
        GLogger.Printf( "Pawn already controlled by other controller\n" );
        return;
    }

    if ( Pawn ) {
        Pawn->OwnerController = nullptr;
    }

    Pawn = _Pawn;

    InputComponent->UnbindAll();

    InputComponent->BindAction( "Pause", IE_Press, this, &APlayerController::TogglePause, true );
    InputComponent->BindAction( "TakeScreenshot", IE_Press, this, &APlayerController::TakeScreenshot, true );
    InputComponent->BindAction( "ToggleWireframe", IE_Press, this, &APlayerController::ToggleWireframe, true );
    InputComponent->BindAction( "ToggleDebugDraw", IE_Press, this, &APlayerController::ToggleDebugDraw, true );

    if ( Pawn ) {
        Pawn->OwnerController = this;
        Pawn->SetupPlayerInputComponent( InputComponent );
        Pawn->SetupRuntimeCommands( CommandContext );
    }

    if ( HUD ) {
        HUD->OwnerPawn = Pawn;
    }
}

void APlayerController::SetViewCamera( ACameraComponent * _Camera ) {
    CameraComponent = _Camera;
}

void APlayerController::SetViewport( Float2 const & _Position, Float2 const & _Size ) {
    ViewportPosition = _Position;
    ViewportSize = _Size;
}

void APlayerController::SetAudioListener( ASceneComponent * _AudioListener ) {
    AudioListener = _AudioListener;
}

void APlayerController::SetHUD( AHUD * _HUD ) {
    if ( HUD == _HUD ) {
        return;
    }

    if ( _HUD && _HUD->OwnerPlayer ) {
        _HUD->OwnerPlayer->SetHUD( nullptr );
    }

    if ( HUD ) {
        HUD->OwnerPlayer = nullptr;
        HUD->OwnerPawn = nullptr;
    }

    HUD = _HUD;

    if ( HUD ) {
        HUD->OwnerPlayer = this;
        HUD->OwnerPawn = Pawn;
    }
}

void APlayerController::SetRenderingParameters( ARenderingParameters * _RP ) {
    RenderingParameters = _RP;
}

void APlayerController::SetAudioParameters( AAudioParameters * _AudioParameters ) {
    AudioParameters = _AudioParameters;
}

void APlayerController::SetInputMappings( AInputMappings * _InputMappings ) {
    InputComponent->SetInputMappings( _InputMappings );
}

AInputMappings * APlayerController::GetInputMappings() {
    return InputComponent->GetInputMappings();
}

//void APlayerController::AddViewActor( FViewActor * _ViewActor ) {
//    ViewActors.Append( _ViewActor );
//    _ViewActor->AddRef();
//}

//void APlayerController::VisitViewActors() {
//    for ( int i = 0 ; i < ViewActors.Size() ; ) {
//        FViewActor * viewer = ViewActors[i];

//        if ( viewer->IsPendingKill() ) {
//            viewer->RemoveRef();
//            ViewActors.Remove( i );
//            continue;
//        }

//        if ( CameraComponent ) {
//            viewer->OnView( CameraComponent );
//        }

//        i++;
//    }
//}

void APlayerController::SetPlayerIndex( int _ControllerId ) {
    InputComponent->ControllerId = _ControllerId;
}

int APlayerController::GetPlayerIndex() const {
    return InputComponent->ControllerId;
}

void APlayerController::SetActive( bool _Active ) {
    InputComponent->bActive = _Active;
}

bool APlayerController::IsActive() const {
    return InputComponent->bActive;
}

void APlayerController::TogglePause() {
    GetWorld()->SetPaused( !GetWorld()->IsPaused() );
}

void APlayerController::TakeScreenshot() {
    /*

    TODO: something like this?

    struct SScreenshotParameters {
        int Width;          // Screenshot custom width or zero to use display width
        int Height;         // Screenshot custom height or zero to use display height
        bool bJpeg;         // Use JPEG compression instead of PNG
        int Number;         // Custom screenshot number. Set to zero to generate unique number.
    };

    SScreenshotParameters screenshotParams;
    memset( &screenshotParams, 0, sizeof( screenshotParams ) );

    GRuntime.TakeScreenshot( screenshotParams );

    Screenshot path example: /Screenshots/Year-Month-Day/Display1/ShotNNNN.png
    Where NNNN is screenshot number.

    */
}

void APlayerController::ToggleWireframe() {
    if ( RenderingParameters ) {
        RenderingParameters->bWireframe ^= 1;
    }
}

void APlayerController::ToggleDebugDraw() {
    if ( RenderingParameters ) {
        RenderingParameters->bDrawDebug ^= 1;
    }
}

void APlayerController::SetCurrentAudioListener() {
    CurrentAudioListener = this;
}

APlayerController * APlayerController::GetCurrentAudioListener() {
    return CurrentAudioListener;
}

void APlayerController::SetCurrentCommandContext() {
    CurrentCommandContext = &CommandContext;
}

ACommandContext * APlayerController::GetCurrentCommandContext() {
    return CurrentCommandContext;
}

Float2 APlayerController::GetNormalizedCursorPos() const {
    if ( ViewportSize.X > 0 && ViewportSize.Y > 0 ) {
        Float2 pos = GEngine.GetCursorPosition();
        pos.X -= ViewportPosition.X;
        pos.Y -= ViewportPosition.Y;
        pos.X /= ViewportSize.X;
        pos.Y /= ViewportSize.Y;
        pos.X = Math::Saturate( pos.X );
        pos.Y = Math::Saturate( pos.Y );
        return pos;
    } else {
        return Float2::Zero();
    }
}
