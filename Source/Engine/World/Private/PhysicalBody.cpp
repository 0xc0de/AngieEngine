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

#include <Engine/World/Public/PhysicalBody.h>
#include <Engine/World/Public/World.h>
#include <Engine/World/Public/DebugDraw.h>

#include "BulletCompatibility/BulletCompatibility.h"
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>

#define PHYS_COMPARE_EPSILON 0.0001f

#define BASIS(t) (t).getBasis()

class FPhysicalBodyMotionState : public btMotionState {
public:
    FPhysicalBodyMotionState()
        : WorldPosition( Float3(0) )
        , WorldRotation( Quat::Identity() )
        , CenterOfMass( Float3(0) )
    {
    }

    // Overrides

    void getWorldTransform( btTransform & _CenterOfMassTransform ) const override;
    void setWorldTransform( btTransform const & _CenterOfMassTransform ) override;

    // Public members

    FPhysicalBody * Self;
    mutable Float3 WorldPosition;
    mutable Quat WorldRotation;
    Float3 CenterOfMass;
};

void FPhysicalBodyMotionState::getWorldTransform( btTransform & _CenterOfMassTransform ) const {
    WorldPosition = Self->GetWorldPosition();
    WorldRotation = Self->GetWorldRotation();

    _CenterOfMassTransform.setRotation( btQuaternionToQuat( WorldRotation ) );
    _CenterOfMassTransform.setOrigin( btVectorToFloat3( WorldPosition ) + _CenterOfMassTransform.getBasis() * btVectorToFloat3( CenterOfMass ) );
}

void FPhysicalBodyMotionState::setWorldTransform( btTransform const & _CenterOfMassTransform ) {
    Self->bTransformWasChangedByPhysicsEngine = true;
    WorldRotation = btQuaternionToQuat( _CenterOfMassTransform.getRotation() );
    WorldPosition = btVectorToFloat3( _CenterOfMassTransform.getOrigin() - _CenterOfMassTransform.getBasis() * btVectorToFloat3( CenterOfMass ) );
    Self->SetWorldPosition( WorldPosition );
    Self->SetWorldRotation( WorldRotation );
    Self->bTransformWasChangedByPhysicsEngine = false;
}

AN_CLASS_META_NO_ATTRIBS( FPhysicalBody )

FPhysicalBody::FPhysicalBody() {
    CachedScale = Float3( 1.0f );
}

void FPhysicalBody::InitializeComponent() {
    Super::InitializeComponent();

    if ( bSimulatePhysics && !bSoftBodySimulation ) {
        CreateRigidBody();
    }
}

void FPhysicalBody::DeinitializeComponent() {
    DestroyRigidBody();

    Super::DeinitializeComponent();
}

void CreateCollisionShape( FCollisionBodyComposition const & BodyComposition, Float3 const & _Scale, btCompoundShape ** _CompoundShape, Float3 * _CenterOfMass ) {
    *_CompoundShape = b3New( btCompoundShape );
    *_CenterOfMass = _Scale * BodyComposition.CenterOfMass;

    if ( !BodyComposition.CollisionBodies.IsEmpty() ) {
        const btVector3 scaling = btVectorToFloat3( _Scale );
        btTransform shapeTransform;

        for ( FCollisionBody * collisionBody : BodyComposition.CollisionBodies ) {
            btCollisionShape * shape = collisionBody->Create();

            shape->setMargin( collisionBody->Margin );
            shape->setUserPointer( collisionBody );
            shape->setLocalScaling( scaling );

            shapeTransform.setOrigin( btVectorToFloat3( _Scale * collisionBody->Position - *_CenterOfMass ) );
            shapeTransform.setRotation( btQuaternionToQuat( collisionBody->Rotation ) );

            (*_CompoundShape)->addChildShape( shapeTransform, shape );

            collisionBody->AddRef();
        }
    }
}

static void DestroyCollisionShape( btCompoundShape * _CompoundShape ) {
    int numShapes = _CompoundShape->getNumChildShapes();
    for ( int i = numShapes-1 ; i >= 0 ; i-- ) {
        btCollisionShape * shape = _CompoundShape->getChildShape( i );
        static_cast< FCollisionBody * >( shape->getUserPointer() )->RemoveRef();
        b3Destroy( shape );
    }
    b3Destroy( _CompoundShape );
}

static void UpdateRigidBodyCollisionShape( btCollisionObject * RigidBody, btCompoundShape * CompoundShape, bool bTrigger, bool bKinematicBody ) {
    int numShapes = CompoundShape->getNumChildShapes();
    bool bUseCompound = !numShapes || numShapes > 1;
    if ( !bUseCompound ) {
        btTransform const & childTransform = CompoundShape->getChildTransform( 0 );

        if ( !btVectorToFloat3( childTransform.getOrigin() ).CompareEps( Float3::Zero(), PHYS_COMPARE_EPSILON )
             || !btQuaternionToQuat( childTransform.getRotation() ).Compare( Quat::Identity() ) ) {
            bUseCompound = true;
        }
    }
    RigidBody->setCollisionShape( bUseCompound ? CompoundShape : CompoundShape->getChildShape( 0 ) );

    int collisionFlags = RigidBody->getCollisionFlags();

    if ( bTrigger ) {
        collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    } else {
        collisionFlags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
    }
    if ( bKinematicBody ) {
        collisionFlags |= btCollisionObject::CF_KINEMATIC_OBJECT;
    } else {
        collisionFlags &= ~btCollisionObject::CF_KINEMATIC_OBJECT;
    }
    if ( !bUseCompound && RigidBody->getCollisionShape()->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE /*&& World->GetInternalEdge()*/ ) {
        collisionFlags |= btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK;
    } else {
        collisionFlags &= ~btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK;
    }

    RigidBody->setCollisionFlags( collisionFlags );
    RigidBody->forceActivationState( bKinematicBody ? DISABLE_DEACTIVATION : ISLAND_SLEEPING );
}

static void UpdateRigidBodyGravity( btRigidBody * RigidBody, bool bDisableGravity, bool bOverrideWorldGravity, Float3 const & SelfGravity, Float3 const & WorldGravity ) {
    int flags = RigidBody->getFlags();
    if ( !bDisableGravity && !bOverrideWorldGravity ) {
        flags &= ~BT_DISABLE_WORLD_GRAVITY;
    } else {
        flags |= BT_DISABLE_WORLD_GRAVITY;
    }
    RigidBody->setFlags( flags );

    if ( bDisableGravity ) {
        RigidBody->setGravity( btVector3( 0.0f, 0.0f, 0.0f ) );
    } else {
        if ( bOverrideWorldGravity ) {
            // Use self gravity
            RigidBody->setGravity( btVectorToFloat3( SelfGravity ) );
        } else {
            // Use world gravity
            RigidBody->setGravity( btVectorToFloat3( WorldGravity ) );
        }
    }
}

AN_FORCEINLINE static unsigned short ClampUnsignedShort( int _Value ) {
    if ( _Value < 0 ) return 0;
    if ( _Value > 0xffff ) return 0xffff;
    return _Value;
}

void FPhysicalBody::CreateRigidBody() {
    btSoftRigidDynamicsWorld * physicsWorld = GetWorld()->PhysicsWorld;

    AN_Assert( MotionState == nullptr );
    AN_Assert( RigidBody == nullptr );
    AN_Assert( CompoundShape == nullptr );

    CachedScale = GetWorldScale();

    MotionState = b3New( FPhysicalBodyMotionState );
    MotionState->Self = this;

    CreateCollisionShape( bUseDefaultBodyComposition ? DefaultBodyComposition() : BodyComposition, CachedScale, &CompoundShape, &MotionState->CenterOfMass );

    btVector3 localInertia( 0.0f, 0.0f, 0.0f );

    if ( Mass < 0.0f ) {
        Mass = 0.0f;
    }
    if ( Mass > 0.0f ) {
        CompoundShape->calculateLocalInertia( Mass, localInertia );
    }

    btRigidBody::btRigidBodyConstructionInfo constructInfo( Mass, MotionState, CompoundShape, localInertia );

    constructInfo.m_linearDamping = LinearDamping;
    constructInfo.m_angularDamping = AngularDamping;
    constructInfo.m_friction = Friction;
    constructInfo.m_rollingFriction = RollingFriction;
//    constructInfo.m_spinningFriction;
    constructInfo.m_restitution = Restitution;
    constructInfo.m_linearSleepingThreshold = LinearSleepingThreshold;
    constructInfo.m_angularSleepingThreshold = AngularSleepingThreshold;

    RigidBody = b3New( btRigidBody, constructInfo );
    RigidBody->setUserPointer( this );

    UpdateRigidBodyCollisionShape( RigidBody, CompoundShape, bTrigger, bKinematicBody );

    Quat worldRotation = GetWorldRotation();
    Float3 worldPosition = GetWorldPosition();

    btTransform & centerOfMassTransform = RigidBody->getWorldTransform();

    centerOfMassTransform.setRotation( btQuaternionToQuat( worldRotation ) );
    centerOfMassTransform.setOrigin( btVectorToFloat3( worldPosition ) + centerOfMassTransform.getBasis() * btVectorToFloat3( MotionState->CenterOfMass ) );

    RigidBody->updateInertiaTensor();

    physicsWorld->addRigidBody( RigidBody, ClampUnsignedShort( CollisionLayer ), ClampUnsignedShort( CollisionMask ) );

    UpdateRigidBodyGravity( RigidBody, bDisableGravity, bOverrideWorldGravity, SelfGravity, GetWorld()->GetGravityVector() );

    ActivatePhysics();

    // Update dynamic attributes
    SetLinearFactor( LinearFactor );
    SetAngularFactor( AngularFactor );
    SetAnisotropicFriction( AnisotropicFriction );
    SetContactProcessingThreshold( ContactProcessingThreshold );
    SetCcdRadius( CcdRadius );
    SetCcdMotionThreshold( CcdMotionThreshold );
}

void FPhysicalBody::DestroyRigidBody() {
    if ( RigidBody ) {
        btSoftRigidDynamicsWorld * physicsWorld = GetWorld()->PhysicsWorld;

        physicsWorld->removeRigidBody( RigidBody );
        b3Destroy( RigidBody );
        RigidBody = nullptr;

        DestroyCollisionShape( CompoundShape );
        CompoundShape = nullptr;

        b3Destroy( MotionState );
        MotionState = nullptr;
    }
}

void FPhysicalBody::UpdatePhysicsAttribs() {
    if ( !bSimulatePhysics ) {
        DestroyRigidBody();
        return;
    }

    if ( bSoftBodySimulation ) {
        DestroyRigidBody();
        return;
    }

    if ( !RigidBody ) {
        CreateRigidBody();
        return;
    }

    btSoftRigidDynamicsWorld * physicsWorld = GetWorld()->PhysicsWorld;

    btTransform const & centerOfMassTransform = RigidBody->getWorldTransform();
    Float3 position = btVectorToFloat3( centerOfMassTransform.getOrigin() - BASIS( centerOfMassTransform ) * btVectorToFloat3( MotionState->CenterOfMass ) );

    physicsWorld->removeRigidBody( RigidBody );

    CachedScale = GetWorldScale();

    DestroyCollisionShape( CompoundShape );
    CreateCollisionShape( bUseDefaultBodyComposition ? DefaultBodyComposition() : BodyComposition, CachedScale, &CompoundShape, &MotionState->CenterOfMass );

    btVector3 localInertia( 0.0f, 0.0f, 0.0f );

    if ( Mass < 0.0f ) {
        Mass = 0.0f;
    }
    if ( Mass > 0.0f ) {
        CompoundShape->calculateLocalInertia( Mass, localInertia );
    }

    RigidBody->setMassProps( Mass, localInertia );

    UpdateRigidBodyCollisionShape( RigidBody, CompoundShape, bTrigger, bKinematicBody );

    // Update position with new center of mass
    SetCenterOfMassPosition( position );

    RigidBody->updateInertiaTensor();

    physicsWorld->addRigidBody( RigidBody, ClampUnsignedShort( CollisionLayer ), ClampUnsignedShort( CollisionMask ) );

    UpdateRigidBodyGravity( RigidBody, bDisableGravity, bOverrideWorldGravity, SelfGravity, GetWorld()->GetGravityVector() );

    ActivatePhysics();

    // Update dynamic attributes
//    SetLinearFactor( LinearFactor );
//    SetAngularFactor( AngularFactor );
//    SetAnisotropicFriction( AnisotropicFriction );
//    SetContactProcessingThreshold( ContactProcessingThreshold );
//    SetCcdRadius( CcdRadius );
//    SetCcdMotionThreshold( CcdMotionThreshold );
}

void FPhysicalBody::OnTransformDirty() {
    Super::OnTransformDirty();

    if ( RigidBody ) {
        if ( !bKinematicBody && !bTransformWasChangedByPhysicsEngine ) {

            Float3 position = GetWorldPosition();
            Quat rotation = GetWorldRotation();

            if ( rotation != MotionState->WorldRotation ) {
                MotionState->WorldRotation = rotation;
                SetCenterOfMassRotation( rotation );
            }
            if ( position != MotionState->WorldPosition ) {
                MotionState->WorldPosition = position;
                SetCenterOfMassPosition( position );
            }
        }

        int numShapes = CompoundShape->getNumChildShapes();
        if ( numShapes > 0 && !CachedScale.CompareEps( GetWorldScale(), PHYS_COMPARE_EPSILON ) ) {
            UpdatePhysicsAttribs();
        }
    }

    if ( SoftBody ) {
        Quat worldRotation = GetWorldRotation();
        Float3 worldPosition = GetWorldPosition();

        btTransform & transform = SoftBody->getWorldTransform();

        transform.setRotation( btQuaternionToQuat( worldRotation ) );
        transform.setOrigin( btVectorToFloat3( worldPosition ) );
    }
}

void FPhysicalBody::SetCenterOfMassPosition( Float3 const & _Position ) {
    btTransform & centerOfMassTransform = RigidBody->getWorldTransform();
    centerOfMassTransform.setOrigin( btVectorToFloat3( _Position ) + BASIS( centerOfMassTransform ) * btVectorToFloat3( MotionState->CenterOfMass ) );

    if ( GetWorld()->IsPhysicsSimulating() ) {
        btTransform interpolationWorldTransform = RigidBody->getInterpolationWorldTransform();
        interpolationWorldTransform.setOrigin( centerOfMassTransform.getOrigin() );
        RigidBody->setInterpolationWorldTransform( interpolationWorldTransform );
    }

    ActivatePhysics();
}

void FPhysicalBody::SetCenterOfMassRotation( Quat const & _Rotation ) {
    btTransform & centerOfMassTransform = RigidBody->getWorldTransform();

    btVector3 bodyPrevPosition = centerOfMassTransform.getOrigin() - BASIS( centerOfMassTransform ) * btVectorToFloat3( MotionState->CenterOfMass );

    centerOfMassTransform.setRotation( btQuaternionToQuat( _Rotation ) );

    if ( !MotionState->CenterOfMass.CompareEps( Float3::Zero(), PHYS_COMPARE_EPSILON ) ) {
        centerOfMassTransform.setOrigin( bodyPrevPosition + centerOfMassTransform.getBasis() * btVectorToFloat3( MotionState->CenterOfMass ) );
    }

    if ( GetWorld()->IsPhysicsSimulating() ) {
        btTransform interpolationWorldTransform = RigidBody->getInterpolationWorldTransform();
        interpolationWorldTransform.setBasis( centerOfMassTransform.getBasis() );
        if ( !MotionState->CenterOfMass.CompareEps( Float3::Zero(), PHYS_COMPARE_EPSILON ) ) {
            interpolationWorldTransform.setOrigin( centerOfMassTransform.getOrigin() );
        }
        RigidBody->setInterpolationWorldTransform( interpolationWorldTransform );
    }

    RigidBody->updateInertiaTensor();

    ActivatePhysics();
}

void FPhysicalBody::SetLinearVelocity( Float3 const & _Velocity ) {
    if ( RigidBody ) {
        RigidBody->setLinearVelocity( btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }

    if ( SoftBody ) {
        SoftBody->setVelocity( btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }
}

void FPhysicalBody::AddLinearVelocity( Float3 const & _Velocity ) {
    if ( RigidBody ) {
        RigidBody->setLinearVelocity( RigidBody->getLinearVelocity() + btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }

    if ( SoftBody ) {
        SoftBody->addVelocity( btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }
}

void FPhysicalBody::SetLinearFactor( Float3 const & _Factor ) {
    if ( RigidBody ) {
        RigidBody->setLinearFactor( btVectorToFloat3( _Factor ) );
    }

    LinearFactor = _Factor;
}

void FPhysicalBody::SetLinearSleepingThreshold( float _Threshold ) {
    if ( RigidBody ) {
        RigidBody->setSleepingThresholds( _Threshold, AngularSleepingThreshold );
    }

    LinearSleepingThreshold = _Threshold;
}

void FPhysicalBody::SetLinearDamping( float _Damping ) {
    if ( RigidBody ) {
        RigidBody->setDamping( _Damping, AngularDamping );
    }

    LinearDamping = _Damping;
}

void FPhysicalBody::SetAngularVelocity( Float3 const & _Velocity ) {
    if ( RigidBody ) {
        RigidBody->setAngularVelocity( btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }
}

void FPhysicalBody::AddAngularVelocity( Float3 const & _Velocity ) {
    if ( RigidBody ) {
        RigidBody->setAngularVelocity( RigidBody->getAngularVelocity() + btVectorToFloat3( _Velocity ) );
        if ( _Velocity != Float3::Zero() ) {
            ActivatePhysics();
        }
    }
}

void FPhysicalBody::SetAngularFactor( Float3 const & _Factor ) {
    if ( RigidBody ) {
        RigidBody->setAngularFactor( btVectorToFloat3( _Factor ) );
    }

    AngularFactor = _Factor;
}

void FPhysicalBody::SetAngularSleepingThreshold( float _Threshold ) {
    if ( RigidBody ) {
        RigidBody->setSleepingThresholds( LinearSleepingThreshold, _Threshold );
    }

    AngularSleepingThreshold = _Threshold;
}

void FPhysicalBody::SetAngularDamping( float _Damping ) {
    if ( RigidBody ) {
        RigidBody->setDamping( LinearDamping, _Damping );
    }

    AngularDamping = _Damping;
}

void FPhysicalBody::SetFriction( float _Friction ) {
    if ( RigidBody ) {
        RigidBody->setFriction( _Friction );
    }

    if ( SoftBody ) {
        SoftBody->setFriction( _Friction );
    }

    Friction = _Friction;
}

void FPhysicalBody::SetAnisotropicFriction( Float3 const & _Friction ) {
    if ( RigidBody ) {
        RigidBody->setAnisotropicFriction( btVectorToFloat3( _Friction ) );
    }

    if ( SoftBody ) {
        SoftBody->setAnisotropicFriction( btVectorToFloat3( _Friction ) );
    }

    AnisotropicFriction = _Friction;
}

void FPhysicalBody::SetRollingFriction( float _Friction ) {
    if ( RigidBody ) {
        RigidBody->setRollingFriction( _Friction );
    }

    if ( SoftBody ) {
        SoftBody->setRollingFriction( _Friction );
    }

    RollingFriction = _Friction;
}

void FPhysicalBody::SetRestitution( float _Restitution ) {
    if ( RigidBody ) {
        RigidBody->setRestitution( _Restitution );
    }

    if ( SoftBody ) {
        SoftBody->setRestitution( _Restitution );
    }

    Restitution = _Restitution;
}

void FPhysicalBody::SetContactProcessingThreshold( float _Threshold ) {
    if ( RigidBody ) {
        RigidBody->setContactProcessingThreshold( _Threshold );
    }

    if ( SoftBody ) {
        SoftBody->setContactProcessingThreshold( _Threshold );
    }

    ContactProcessingThreshold = _Threshold;
}

void FPhysicalBody::SetCcdRadius( float _Radius ) {
    CcdRadius = FMath::Max( _Radius, 0.0f );

    if ( RigidBody ) {
        RigidBody->setCcdSweptSphereRadius( CcdRadius );
    }

    if ( SoftBody ) {
        SoftBody->setCcdSweptSphereRadius( CcdRadius );
    }
}

void FPhysicalBody::SetCcdMotionThreshold( float _Threshold ) {
    CcdMotionThreshold = FMath::Max( _Threshold, 0.0f );

    if ( RigidBody ) {
        RigidBody->setCcdMotionThreshold( CcdMotionThreshold );
    }

    if ( SoftBody ) {
        SoftBody->setCcdMotionThreshold( CcdMotionThreshold );
    }
}

Float3 FPhysicalBody::GetLinearVelocity() const {
    return RigidBody ? btVectorToFloat3( RigidBody->getLinearVelocity() ) : Float3::Zero();
}

Float3 const & FPhysicalBody::GetLinearFactor() const {
    return LinearFactor;
}

Float3 FPhysicalBody::GetVelocityAtPoint( Float3 const & _Position ) const {
    return RigidBody ? btVectorToFloat3( RigidBody->getVelocityInLocalPoint( btVectorToFloat3( _Position - MotionState->CenterOfMass ) ) ) : Float3::Zero();
}

float FPhysicalBody::GetLinearSleepingThreshold() const {
    return LinearSleepingThreshold;
}

float FPhysicalBody::GetLinearDamping() const {
    return LinearDamping;
}

Float3 FPhysicalBody::GetAngularVelocity() const {
    return RigidBody ? btVectorToFloat3( RigidBody->getAngularVelocity() ) : Float3::Zero();
}

Float3 const & FPhysicalBody::GetAngularFactor() const {
    return AngularFactor;
}

float FPhysicalBody::GetAngularSleepingThreshold() const {
    return AngularSleepingThreshold;
}

float FPhysicalBody::GetAngularDamping() const {
    return AngularDamping;
}

float FPhysicalBody::GetFriction() const {
    return Friction;
}

Float3 const & FPhysicalBody::GetAnisotropicFriction() const {
    return AnisotropicFriction;
}

float FPhysicalBody::GetRollingFriction() const {
    return RollingFriction;
}

float FPhysicalBody::GetRestitution() const {
    return Restitution;
}

float FPhysicalBody::GetContactProcessingThreshold() const {
    return ContactProcessingThreshold;
}

float FPhysicalBody::GetCcdRadius() const {
    return CcdRadius;
}

float FPhysicalBody::GetCcdMotionThreshold() const {
    return CcdMotionThreshold;
}

Float3 const & FPhysicalBody::GetCenterOfMass() const {
    return MotionState ? MotionState->CenterOfMass : Float3::Zero();
}

Float3 FPhysicalBody::GetCenterOfMassWorldPosition() const {
    return RigidBody ? btVectorToFloat3( RigidBody->getWorldTransform().getOrigin() ) : GetWorldPosition();
}

void FPhysicalBody::ActivatePhysics() {
    if ( Mass > 0.0f ) {
        if ( RigidBody ) {
            RigidBody->activate( true );
        }

        if ( SoftBody ) {
            SoftBody->activate( true );
        }
    }
}

bool FPhysicalBody::IsPhysicsActive() const {
    
    if ( RigidBody ) {
        return RigidBody->isActive();
    }

    if ( SoftBody ) {
        return SoftBody->isActive();
    }

    return false;
}

void FPhysicalBody::ClearForces() {
    if ( RigidBody ) {
        RigidBody->clearForces();
    }
}

void FPhysicalBody::ApplyCentralForce( Float3 const & _Force ) {
    if ( RigidBody && _Force != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyCentralForce( btVectorToFloat3( _Force ) );
    }
}

void FPhysicalBody::ApplyForce( Float3 const & _Force, Float3 const & _Position ) {
    if ( RigidBody && _Force != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyForce( btVectorToFloat3( _Force ), btVectorToFloat3( _Position - MotionState->CenterOfMass ) );
    }
}

void FPhysicalBody::ApplyTorque( Float3 const & _Torque ) {
    if ( RigidBody && _Torque != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyTorque( btVectorToFloat3( _Torque ) );
    }
}

void FPhysicalBody::ApplyCentralImpulse( Float3 const & _Impulse ) {
    if ( RigidBody && _Impulse != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyCentralImpulse( btVectorToFloat3( _Impulse ) );
    }
}

void FPhysicalBody::ApplyImpulse( Float3 const & _Impulse, Float3 const & _Position ) {
    if ( RigidBody && _Impulse != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyImpulse( btVectorToFloat3( _Impulse ), btVectorToFloat3( _Position - MotionState->CenterOfMass ) );
    }
}

void FPhysicalBody::ApplyTorqueImpulse( Float3 const & _Torque ) {
    if ( RigidBody && _Torque != Float3::Zero() ) {
        ActivatePhysics();
        RigidBody->applyTorqueImpulse( btVectorToFloat3( _Torque ) );
    }
}

void FPhysicalBody::GetCollisionBodiesWorldBounds( TPodArray< BvAxisAlignedBox > & _BoundingBoxes ) const {
    if ( !RigidBody ) {
        _BoundingBoxes.Clear();
    } else {
        btVector3 mins, maxs;

        int numShapes = CompoundShape->getNumChildShapes();

        _BoundingBoxes.ResizeInvalidate( numShapes );

        for ( int i = 0 ; i < numShapes ; i++ ) {
            btCompoundShapeChild & shape = CompoundShape->getChildList()[ i ];

            shape.m_childShape->getAabb( RigidBody->getWorldTransform() * shape.m_transform, mins, maxs );

            _BoundingBoxes[i].Mins = btVectorToFloat3( mins );
            _BoundingBoxes[i].Maxs = btVectorToFloat3( maxs );
        }
    }
}

void FPhysicalBody::GetCollisionBodyWorldBounds( int _Index, BvAxisAlignedBox & _BoundingBox ) const {
    if ( !RigidBody || _Index >= CompoundShape->getNumChildShapes() ) {
        _BoundingBox.Clear();
        return;
    }

    btVector3 mins, maxs;

    btCompoundShapeChild & shape = CompoundShape->getChildList()[ _Index ];

    shape.m_childShape->getAabb( RigidBody->getWorldTransform() * shape.m_transform, mins, maxs );

    _BoundingBox.Mins = btVectorToFloat3( mins );
    _BoundingBox.Maxs = btVectorToFloat3( maxs );
}

void FPhysicalBody::GetCollisionBodyLocalBounds( int _Index, BvAxisAlignedBox & _BoundingBox ) const {
    if ( !RigidBody || _Index >= CompoundShape->getNumChildShapes() ) {
        _BoundingBox.Clear();
        return;
    }

    btVector3 mins, maxs;

    btCompoundShapeChild & shape = CompoundShape->getChildList()[ _Index ];

    shape.m_childShape->getAabb( shape.m_transform, mins, maxs );

    _BoundingBox.Mins = btVectorToFloat3( mins );
    _BoundingBox.Maxs = btVectorToFloat3( maxs );
}

float FPhysicalBody::GetCollisionBodyMargin( int _Index ) const {
    if ( !RigidBody || _Index >= CompoundShape->getNumChildShapes() ) {
        return 0;
    }

    btCompoundShapeChild & shape = CompoundShape->getChildList()[ _Index ];

    return shape.m_childShape->getMargin();
}

int FPhysicalBody::GetCollisionBodiesCount() const {
    if ( !RigidBody ) {
        return 0;
    }

    return CompoundShape->getNumChildShapes();
}

void FPhysicalBody::EndPlay() {
    E_OnBeginContact.UnsubscribeAll();
    E_OnEndContact.UnsubscribeAll();
    E_OnUpdateContact.UnsubscribeAll();
    E_OnBeginOverlap.UnsubscribeAll();
    E_OnEndOverlap.UnsubscribeAll();
    E_OnUpdateOverlap.UnsubscribeAll();
}

void FPhysicalBody::DrawDebug( FDebugDraw * _DebugDraw ) {

    _DebugDraw->SetDepthTest( false );

#if 0
    TPodArray< BvAxisAlignedBox > boundingBoxes;

    GetCollisionBodiesWorldBounds( boundingBoxes );

    _DebugDraw->SetColor( 1, 1, 0, 1 );
    for ( BvAxisAlignedBox const & bb : boundingBoxes ) {
        _DebugDraw->DrawAABB( bb );
    }
#endif
    Float3 centerOfMass = GetCenterOfMassWorldPosition();

    _DebugDraw->SetColor( 1, 0, 0, 1 );
    _DebugDraw->DrawBox( centerOfMass, Float3( 0.02f ) );
}
