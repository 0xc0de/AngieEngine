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

#include <Engine/World/Public/Components/SpotLightComponent.h>
#include <Engine/World/Public/World.h>
#include <Engine/Base/Public/DebugDraw.h>

constexpr float DEFAULT_INNER_RADIUS = 0.5f;
constexpr float DEFAULT_OUTER_RADIUS = 1.0f;
constexpr float DEFAULT_INNER_CONE_ANGLE = 30.0f;
constexpr float DEFAULT_OUTER_CONE_ANGLE = 35.0f;
constexpr float DEFAULT_SPOT_EXPONENT = 1.0f;

FRuntimeVariable RVDrawSpotLights( _CTS( "DrawSpotLights" ), _CTS( "0" ), VAR_CHEAT );

AN_CLASS_META( FSpotLightComponent )

FSpotLightComponent::FSpotLightComponent() {
    InnerRadius = DEFAULT_INNER_RADIUS;
    OuterRadius = DEFAULT_OUTER_RADIUS;
    InnerConeAngle = DEFAULT_INNER_CONE_ANGLE;
    OuterConeAngle = DEFAULT_OUTER_CONE_ANGLE;
    SpotExponent = DEFAULT_SPOT_EXPONENT;
#ifdef FUTURE
    SphereWorldBounds.Radius = OuterRadius;
    SphereWorldBounds.Center = Float3(0);
    AABBWorldBounds.Mins = SphereWorldBounds.Center - OuterRadius;
    AABBWorldBounds.Maxs = SphereWorldBounds.Center + OuterRadius;
#endif
    UpdateBoundingBox();
}

void FSpotLightComponent::InitializeComponent() {
    Super::InitializeComponent();

    GetWorld()->AddSpotLight( this );
}

void FSpotLightComponent::DeinitializeComponent() {
    Super::DeinitializeComponent();

    GetWorld()->RemoveSpotLight( this );
}

void FSpotLightComponent::SetInnerRadius( float _Radius ) {
    InnerRadius = FMath::Max( 0.001f, _Radius );
}

float FSpotLightComponent::GetInnerRadius() const {
    return InnerRadius;
}

void FSpotLightComponent::SetOuterRadius( float _Radius ) {
    OuterRadius = FMath::Max( 0.001f, _Radius );

    UpdateBoundingBox();
}

float FSpotLightComponent::GetOuterRadius() const {
    return OuterRadius;
}

void FSpotLightComponent::SetInnerConeAngle( float _Angle ) {
    InnerConeAngle = FMath::Clamp( _Angle, 0.0001f, 180.0f );
}

float FSpotLightComponent::GetInnerConeAngle() const {
    return InnerConeAngle;
}

void FSpotLightComponent::SetOuterConeAngle( float _Angle ) {
    OuterConeAngle = FMath::Clamp( _Angle, 0.0001f, 180.0f );

    UpdateBoundingBox();
}

float FSpotLightComponent::GetOuterConeAngle() const {
    return OuterConeAngle;
}

void FSpotLightComponent::SetSpotExponent( float _Exponent ) {
    SpotExponent = _Exponent;
}

float FSpotLightComponent::GetSpotExponent() const {
    return SpotExponent;
}

void FSpotLightComponent::SetDirection( Float3 const & _Direction ) {
    Float3x3 orientation;

    orientation[2] = -_Direction.Normalized();
    orientation[2].ComputeBasis( orientation[0], orientation[1] );

    Quat rotation;
    rotation.FromMatrix( orientation );
    SetRotation( rotation );
}

Float3 FSpotLightComponent::GetDirection() const {
    return GetForwardVector();
}

void FSpotLightComponent::SetWorldDirection( Float3 const & _Direction ) {
    Float3x3 orientation;
    orientation[2] = -_Direction.Normalized();
    orientation[2].ComputeBasis( orientation[0], orientation[1] );

    Quat rotation;
    rotation.FromMatrix( orientation );
    SetWorldRotation( rotation );
}

Float3 FSpotLightComponent::GetWorldDirection() const {
    return GetWorldForwardVector();
}

BvAxisAlignedBox const & FSpotLightComponent::GetWorldBounds() const {
    return AABBWorldBounds;
}

void FSpotLightComponent::OnTransformDirty() {
    Super::OnTransformDirty();

    UpdateBoundingBox();
    //MarkAreaDirty();
}

void FSpotLightComponent::UpdateBoundingBox() {
    const float ToHalfAngleRadians = 0.5f / 180.0f * FMath::_PI;
    const float HalfConeAngle = OuterConeAngle * ToHalfAngleRadians;
    const Float3 WorldPos = GetWorldPosition();

    // Compute cone OBB for voxelization
    OBBWorldBounds.Orient = GetWorldRotation().ToMatrix();

    const Float3 SpotDir = -OBBWorldBounds.Orient[ 2 ];

    //OBBWorldBounds.HalfSize.X = OBBWorldBounds.HalfSize.Y = tan( HalfConeAngle ) * OuterRadius;
    OBBWorldBounds.HalfSize.X = OBBWorldBounds.HalfSize.Y = sin( HalfConeAngle ) * OuterRadius;
    OBBWorldBounds.HalfSize.Z = OuterRadius * 0.5f;
    OBBWorldBounds.Center = WorldPos + SpotDir * ( OBBWorldBounds.HalfSize.Z );

    // TODO: Optimize?
    Float4x4 OBBTransform = Float4x4::Translation( OBBWorldBounds.Center ) * Float4x4( OBBWorldBounds.Orient ) * Float4x4::Scale( OBBWorldBounds.HalfSize );
    OBBTransformInverse = OBBTransform.Inversed();

    // Compute cone AABB for culling
    AABBWorldBounds.Clear();
    AABBWorldBounds.AddPoint( WorldPos );
    Float3 v = WorldPos + SpotDir * OuterRadius;
    Float3 vx = OBBWorldBounds.Orient[ 0 ] * OBBWorldBounds.HalfSize.X;
    Float3 vy = OBBWorldBounds.Orient[ 1 ] * OBBWorldBounds.HalfSize.X;
    AABBWorldBounds.AddPoint( v + vx );
    AABBWorldBounds.AddPoint( v - vx );
    AABBWorldBounds.AddPoint( v + vy );
    AABBWorldBounds.AddPoint( v - vy );

    // Посмотреть, как более эффективно распределяется площадь - у сферы или AABB
    // Compute cone Sphere bounds
    if ( HalfConeAngle > FMath::_PI / 4 ) {
        SphereWorldBounds.Radius = sin( HalfConeAngle ) * OuterRadius;
        SphereWorldBounds.Center = WorldPos + SpotDir * ( cos( HalfConeAngle ) * OuterRadius );
    } else {
        SphereWorldBounds.Radius = OuterRadius / ( 2.0 * cos( HalfConeAngle ) );
        SphereWorldBounds.Center = WorldPos + SpotDir * SphereWorldBounds.Radius;
    }
}

void FSpotLightComponent::DrawDebug( FDebugDraw * _DebugDraw ) {
    Super::DrawDebug( _DebugDraw );

    if ( RVDrawSpotLights ) {
        Float3 pos = GetWorldPosition();
        Float3x3 orient = GetWorldRotation().ToMatrix();
        _DebugDraw->SetDepthTest( false );
        _DebugDraw->SetColor( FColor4( 0.5f, 0.5f, 0.5f, 1 ) );
        _DebugDraw->DrawCone( pos, orient, OuterRadius, InnerConeAngle * 0.5f );
        _DebugDraw->SetColor( FColor4( 1, 1, 1, 1 ) );
        _DebugDraw->DrawCone( pos, orient, OuterRadius, OuterConeAngle * 0.5f );
    }
}
