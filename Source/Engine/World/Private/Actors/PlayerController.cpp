/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2020 Alexander Samusev.

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

#include <World/Public/Actors/PlayerController.h>
#include <World/Public/Components/InputComponent.h>
#include <World/Public/Components/CameraComponent.h>
#include <World/Public/World.h>
#include <World/Public/Widgets/WViewport.h>
#include <Runtime/Public/Runtime.h>

AN_CLASS_META( APlayerController )
AN_CLASS_META( ARenderingParameters )

APlayerController * APlayerController::CurrentAudioListener = nullptr;
ACommandContext * APlayerController::CurrentCommandContext = nullptr;

APlayerController::APlayerController() {
    InputComponent = CreateComponent< AInputComponent >( "PlayerControllerInput" );

    bCanEverTick = true;

    //ViewportSize.X = 512;
    //ViewportSize.Y = 512;

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

    if ( CurrentAudioListener == this ) {
        CurrentAudioListener = nullptr;
    }

    if ( CurrentCommandContext == &CommandContext ) {
        CurrentCommandContext = nullptr;
    }
}

void APlayerController::OnPawnChanged()
{
    InputComponent->UnbindAll();

    InputComponent->BindAction( "Pause", IE_Press, this, &APlayerController::TogglePause, true );
    InputComponent->BindAction( "TakeScreenshot", IE_Press, this, &APlayerController::TakeScreenshot, true );
    InputComponent->BindAction( "ToggleWireframe", IE_Press, this, &APlayerController::ToggleWireframe, true );
    InputComponent->BindAction( "ToggleDebugDraw", IE_Press, this, &APlayerController::ToggleDebugDraw, true );

    if ( Pawn ) {
        Pawn->SetupPlayerInputComponent( InputComponent );
        Pawn->SetupRuntimeCommands( CommandContext );
    }

    if ( HUD ) {
        HUD->OwnerPawn = Pawn;
    }
}

void APlayerController::SetAudioListener( ASceneComponent * _AudioListener ) {
    AudioListener = _AudioListener;
}

void APlayerController::SetHUD( AHUD * _HUD ) {
    if ( IsSame( HUD, _HUD ) ) {
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

void APlayerController::SetPlayerIndex( int _ControllerId ) {
    InputComponent->ControllerId = _ControllerId;
}

int APlayerController::GetPlayerIndex() const {
    return InputComponent->ControllerId;
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
    Core::ZeroMem( &screenshotParams, sizeof( screenshotParams ) );

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

ASceneComponent * APlayerController::GetAudioListener() {
    if ( AudioListener )
    {
        return AudioListener;
    }

    if ( Pawn )
    {
        return Pawn->GetPawnCamera();
    }

    return nullptr;
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

float APlayerController::GetViewportAspectRatio() const {
    if ( Viewport ) {
        Float2 size = Viewport->GetAvailableSize();
        if ( size.X > 0 && size.Y > 0 ) {
            return size.X / size.Y;
        }
    }
    return 1.0f;
}

Float2 APlayerController::GetLocalCursorPosition() const {
    return Viewport ? Viewport->GetLocalCursorPosition()
                    : Float2::Zero();
}

Float2 APlayerController::GetNormalizedCursorPosition() const {
    if ( Viewport ) {
        Float2 size = Viewport->GetAvailableSize();
        if ( size.X > 0 && size.Y > 0 )
        {
            Float2 pos = GetLocalCursorPosition();
            pos.X /= size.X;
            pos.Y /= size.Y;
            pos.X = Math::Saturate( pos.X );
            pos.Y = Math::Saturate( pos.Y );
            return pos;
        }
    }
    return Float2::Zero();
}
