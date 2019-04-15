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

#include <Engine/World/Public/Pawn.h>
#include <Engine/World/Public/MeshComponent.h>
#include <Engine/World/Public/CameraComponent.h>

class FPlayer : public FPawn {
    AN_ACTOR( FPlayer, FPawn )

public:
    FCameraComponent * Camera;
    FMeshComponent * unitBoxComponent;

protected:

    FPlayer();

    void BeginPlay() override;
    void EndPlay() override;
    void SetupPlayerInputComponent( FInputComponent * _Input ) override;
    void Tick( float _TimeStep ) override;

private:
    void MoveForward( float _Value );
    void MoveRight( float _Value );
    void MoveUp( float _Value );
    void MoveDown( float _Value );
    void TurnRight( float _Value );
    void TurnUp( float _Value );
    void SpeedPress();
    void SpeedRelease();

    Angl Angles;
    Float3 MoveVector;
    bool bSpeed;
};
