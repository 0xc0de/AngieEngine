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

#include "MeshComponent.h"
#include <Engine/Resource/Public/Skeleton.h>

class AAnimationController;

/**

ASkinnedComponent

Mesh component with skinning

*/
class ASkinnedComponent : public AMeshComponent {
    AN_COMPONENT( ASkinnedComponent, AMeshComponent )

    friend class AWorld;
    friend class AAnimationController;

public:
    /** Get skeleton. Never return null */
    ASkeleton * GetSkeleton() { return Skeleton; }

    /** Add animation controller */
    void AddAnimationController( AAnimationController * _Controller );

    /** Remove animation controller */
    void RemoveAnimationController( AAnimationController * _Controller );

    /** Remove all animation controllers */
    void RemoveAnimationControllers();

    /** Get animation controllers */
    TPodArray< AAnimationController * > const & GetAnimationControllers() const { return AnimControllers; }

    /** Set position on all animation tracks */
    void SetTimeBroadcast( float _Time );

    /** Step time delta on all animation tracks */
    void AddTimeDeltaBroadcast( float _TimeDelta );

    /** Recompute bounding box. Don't use directly. Use GetBounds() instead, it will recompute bounding box automatically. */
    void UpdateBounds();

    /** Get transform of the joint */
    Float3x4 const & GetJointTransform( int _JointIndex );

    /** Iterate meshes in parent world */
    ASkinnedComponent * GetNextSkinnedMesh() { return Next; }
    ASkinnedComponent * GetPrevSkinnedMesh() { return Prev; }

    void UpdateJointTransforms( size_t & _SkeletonOffset, size_t & _SkeletonSize );

protected:
    ASkinnedComponent();

    void InitializeComponent() override;
    void DeinitializeComponent() override;

    void OnMeshChanged() override;
    void OnLazyBoundsUpdate() override;

    void DrawDebug( ADebugDraw * _DebugDraw ) override;

private:
    void UpdateControllersIfDirty();
    void UpdateControllers();

    void UpdateTransformsIfDirty();
    void UpdateTransforms();

    void UpdateAbsoluteTransformsIfDirty();

    void MergeJointAnimations();

    TRef< ASkeleton > Skeleton;

    TPodArray< AAnimationController * > AnimControllers;

    TPodArray< Float3x4 > AbsoluteTransforms;
    TPodArray< Float3x4 > RelativeTransforms;

    ASkinnedComponent * Next;
    ASkinnedComponent * Prev;

    bool bUpdateBounds;
    bool bUpdateControllers;
    bool bUpdateRelativeTransforms;
    //bool bWriteTransforms;

protected:
    bool bUpdateAbsoluteTransforms;
    bool bJointsSimulatedByPhysics;
};
