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

#include "ActorComponent.h"

class FSceneComponent;

using FArrayOfChildComponents = TPodArray< FSceneComponent *, 8 >;


class FSkinnedComponent;
class FSocketDef;
struct FSocket {
    FSocketDef * SocketDef;
    FSkinnedComponent * Parent;
};

/*

FSceneComponent

Base class for all actor components that have its position, rotation and scale

*/
class ANGIE_API FSceneComponent : public FActorComponent {
    AN_COMPONENT( FSceneComponent, FActorComponent )

public:
    // Attach to parent component
    void AttachTo( FSceneComponent * _Parent, const char * _Socket = nullptr, bool _KeepWorldTransform = false );

    // Detach from parent component
    void Detach( bool _KeepWorldTransform = false );

    // Detach all childs
    void DetachChilds( bool _bRecursive = false, bool _KeepWorldTransform = false );

    // Is component parent of specified child
    bool IsChild( FSceneComponent * _Child, bool _Recursive ) const;

    // Is component root
    bool IsRoot() const;

    // Find child by name
    FSceneComponent * FindChild( const char * _UniqueName, bool _Recursive );

    // Get reference to array of child components
    FArrayOfChildComponents const & GetChilds() const { return Childs; }

    // Get parent component
    FSceneComponent * GetParent() const { return AttachParent; }

    // Get socket index by name
    int FindSocket( const char * _Name ) const;

    // Get attached socket
    int GetAttachedSocket() const { return SocketIndex; }

    // Is component attached to socket
    bool IsAttachedToSocket() const { return SocketIndex >= 0; }

    // Set ignore parent position
    void SetAbsolutePosition( bool _AbsolutePosition );

    bool IsAbsolutePosition() const { return bAbsolutePosition; }

    // Set ignore parent rotation
    void SetAbsoluteRotation( bool _AbsoluteRotation );

    bool IsAbsoluteRotation() const { return bAbsoluteRotation; }

    // Set ignore parent scale
    void SetAbsoluteScale( bool _AbsoluteScale );

    bool IsAbsoluteScale() const { return bAbsoluteScale; }

    // Set local position
    void SetPosition( Float3 const & _Position );

    // Set local position
    void SetPosition( float const & _X, float const & _Y, float const & _Z );

    // Set local orient
    //void SetOrient( Float3x3 const & _Orient );

    // Set local rotation
    void SetRotation( Quat const & _Rotation );

    // Set local rotation
    void SetAngles( Angl const & _Angles );

    // Set local rotation
    void SetAngles( float const & _Pitch, float const & _Yaw, float const & _Roll );

    // Set scale
    void SetScale( Float3 const & _Scale );

    // Set scale
    void SetScale( float const & _X, float const & _Y, float const & _Z );

    // Set scale
    void SetScale( float const & _ScaleXYZ );

    // Set local transform
    void SetTransform( Float3 const & _Position, Quat const & _Rotation );

    // Set local transform
    void SetTransform( Float3 const & _Position, Quat const & _Rotation, Float3 const & _Scale );

    // Set local transform
    void SetTransform( FTransform const & _Transform );

    // Set local transform
    void SetTransform( FSceneComponent const * _Transform );

    // Set world position
    void SetWorldPosition( Float3 const & _Position );

    // Set world position
    void SetWorldPosition( float const & _X, float const & _Y, float const & _Z );

    // Set world rotation
    void SetWorldRotation( Quat const & _Rotation );

    // Set world scale
    void SetWorldScale( Float3 const & _Scale );

    // Set world scale
    void SetWorldScale( float const & _X, float const & _Y, float const & _Z );

    // Set world transform
    void SetWorldTransform( Float3 const & _Position, Quat const & _Rotation );

    // Set world transform
    void SetWorldTransform( Float3 const & _Position, Quat const & _Rotation, Float3 const & _Scale );

    // Set world transform
    void SetWorldTransform( FTransform const & _Transform );

    // Get local position
    Float3 const & GetPosition() const;

    // Get local orient
    //Float3x3 GetOrient() const;

    // Get local rotation
    Quat const & GetRotation() const;

    Angl GetAngles() const;
    float GetPitch() const;
    float GetYaw() const;
    float GetRoll() const;

    Float3 GetRightVector() const;
    Float3 GetLeftVector() const;
    Float3 GetUpVector() const;
    Float3 GetDownVector() const;
    Float3 GetBackVector() const;
    Float3 GetForwardVector() const;
    void GetVectors( Float3 * _Right, Float3 * _Up, Float3 * _Back ) const;

    Float3 GetWorldRightVector() const;
    Float3 GetWorldLeftVector() const;
    Float3 GetWorldUpVector() const;
    Float3 GetWorldDownVector() const;
    Float3 GetWorldBackVector() const;
    Float3 GetWorldForwardVector() const;
    void GetWorldVectors( Float3 * _Right, Float3 * _Up, Float3 * _Back ) const;

    // Get scale
    Float3 const & GetScale() const;

    // Get world position
    Float3 GetWorldPosition() const;

    // Get world rotation
    Quat const & GetWorldRotation() const;

    // Get world scale
    Float3 GetWorldScale() const;

    // Mark to recompute transforms
    void MarkTransformDirty();

    // Compute component local transform matrix
    void ComputeLocalTransformMatrix( Float3x4 & _LocalTransformMatrix ) const;

    // Get transposed world transform matrix
    Float3x4 const & GetWorldTransformMatrix() const;

    Float3x4 ComputeWorldTransformInverse() const;
    Quat ComputeWorldRotationInverse() const;

    // First person shooter rotations
    void TurnRightFPS( float _DeltaAngleRad );
    void TurnLeftFPS( float _DeltaAngleRad );
    void TurnUpFPS( float _DeltaAngleRad );
    void TurnDownFPS( float _DeltaAngleRad );

    // Rotations
    void TurnAroundAxis( float _DeltaAngleRad, Float3 const & _NormalizedAxis );
    void TurnAroundVector( float _DeltaAngleRad, Float3 const & _Vector );

    // Move
    void StepRight( float _Units );
    void StepLeft( float _Units );
    void StepUp( float _Units );
    void StepDown( float _Units );
    void StepBack( float _Units );
    void StepForward( float _Units );
    void Step( Float3 const & _Vector );

protected:
    FSceneComponent();

    void DeinitializeComponent() override;

    virtual void OnTransformDirty() {}

    using FArrayOfSockets = TPodArray< FSocket, 1 >;
    FArrayOfSockets Sockets;

private:
    void _AttachTo( FSceneComponent * _Parent, bool _KeepWorldTransform );

    void ComputeWorldTransform() const;

    Float3 Position;
    Quat Rotation;
    Float3 Scale;
    mutable Float3x4 WorldTransformMatrix;   // Transposed world transform matrix
    mutable Quat WorldRotation;
    mutable bool bTransformDirty;
    FArrayOfChildComponents Childs;
    FSceneComponent * AttachParent;
    int SocketIndex;
    //bool bIgnoreLocalTransform;
    bool bAbsolutePosition : 1;
    bool bAbsoluteRotation : 1;
    bool bAbsoluteScale : 1;
};
