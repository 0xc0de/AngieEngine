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

#include <World/Public/PhysicsWorld.h>
#include <World/Public/Components/PhysicalBody.h>
#include <World/Public/Actors/Actor.h>
#include <World/Public/World.h>
#include <Runtime/Public/RuntimeVariable.h>
#include <World/Public/Base/DebugDraw.h>
#include <Core/Public/IntrusiveLinkedListMacro.h>

#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>
#include <BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include <btBulletDynamicsCommon.h>

#include "BulletCompatibility/BulletCompatibility.h"

#ifdef AN_COMPILER_MSVC
#pragma warning(push)
#pragma warning( disable : 4456 )
#endif
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#ifdef AN_COMPILER_MSVC
#pragma warning(pop)
#endif

ARuntimeVariable RVDrawCollisionShapeWireframe( _CTS( "DrawCollisionShapeWireframe" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawContactPoints( _CTS( "DrawContactPoints" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawConstraints( _CTS( "DrawConstraints" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawConstraintLimits( _CTS( "DrawConstraintLimits" ), _CTS( "0" ), VAR_CHEAT );

static SCollisionQueryFilter DefaultCollisionQueryFilter;

AN_FORCEINLINE static unsigned short ClampUnsignedShort( int _Value ) {
    if ( _Value < 0 ) return 0;
    if ( _Value > 0xffff ) return 0xffff;
    return _Value;
}

class APhysicsDebugDraw : public btIDebugDraw {
public:
    ADebugDraw * DD;
    int DebugMode;

    void drawLine( btVector3 const & from, btVector3 const & to, btVector3 const & color ) override {
        DD->SetColor( AColor4( color.x(), color.y(), color.z(), 1.0f ) );
        DD->DrawLine( btVectorToFloat3( from ), btVectorToFloat3( to ) );
    }

    void drawContactPoint( btVector3 const & pointOnB, btVector3 const & normalOnB, btScalar distance, int lifeTime, btVector3 const & color ) override {
        DD->SetColor( AColor4( color.x(), color.y(), color.z(), 1.0f ) );
        DD->DrawPoint( btVectorToFloat3( pointOnB ) );
        DD->DrawPoint( btVectorToFloat3( normalOnB ) );
    }

    void reportErrorWarning( const char * warningString ) override {
    }

    void draw3dText( btVector3 const & location, const char * textString ) override {
    }

    void setDebugMode( int debugMode ) override {
        DebugMode = debugMode;
    }

    int getDebugMode() const override {
        return DebugMode;
    }

    void flushLines() override {
    }
};

static APhysicsDebugDraw PhysicsDebugDraw;

struct SCollisionFilterCallback : public btOverlapFilterCallback {

    // Return true when pairs need collision
    bool needBroadphaseCollision( btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1 ) const override {

        if ( ( proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask )
             && ( proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask ) )
        {
            // FIXME: can we safe cast m_clientObject to btCollisionObject?

            btCollisionObject const * colObj0 = reinterpret_cast< btCollisionObject const * >( proxy0->m_clientObject );
            btCollisionObject const * colObj1 = reinterpret_cast< btCollisionObject const * >( proxy1->m_clientObject );

            APhysicalBody const * body0 = ( APhysicalBody const * )colObj0->getUserPointer();
            APhysicalBody const * body1 = ( APhysicalBody const * )colObj1->getUserPointer();

            if ( !body0 || !body1 ) {
                GLogger.Printf( "Null body\n" );
                return true;
            }

//            APhysicalBody * body0 = static_cast< APhysicalBody * >( static_cast< btCollisionObject * >( proxy0->m_clientObject )->getUserPointer() );
//            APhysicalBody * body1 = static_cast< APhysicalBody * >( static_cast< btCollisionObject * >( proxy1->m_clientObject )->getUserPointer() );

            if ( body0->CollisionIgnoreActors.Find( body1->GetParentActor() ) != body0->CollisionIgnoreActors.End() ) {
                return false;
            }

            if ( body1->CollisionIgnoreActors.Find( body0->GetParentActor() ) != body1->CollisionIgnoreActors.End() ) {
                return false;
            }

            return true;
        }

        return false;
    }
};

static SCollisionFilterCallback CollisionFilterCallback;

extern ContactAddedCallback gContactAddedCallback;

static bool CustomMaterialCombinerCallback( btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0,
                                            int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1 )
{
    const int normalAdjustFlags = 0
    //| BT_TRIANGLE_CONVEX_BACKFACE_MODE
    //| BT_TRIANGLE_CONCAVE_DOUBLE_SIDED //double sided options are experimental, single sided is recommended
    //| BT_TRIANGLE_CONVEX_DOUBLE_SIDED
        ;

    btAdjustInternalEdgeContacts( cp, colObj1Wrap, colObj0Wrap, partId1, index1, normalAdjustFlags );

    cp.m_combinedFriction = btManifoldResult::calculateCombinedFriction( colObj0Wrap->getCollisionObject(), colObj1Wrap->getCollisionObject() );
    cp.m_combinedRestitution = btManifoldResult::calculateCombinedRestitution( colObj0Wrap->getCollisionObject(), colObj1Wrap->getCollisionObject() );

    return true;
}

static int CacheContactPoints = -1;

void APhysicsWorld::GenerateContactPoints( int _ContactIndex, SCollisionContact & _Contact ) {
    if ( CacheContactPoints == _ContactIndex ) {
        // Contact points already generated for this contact
        return;
    }

    CacheContactPoints = _ContactIndex;

    ContactPoints.ResizeInvalidate( _Contact.Manifold->getNumContacts() );

    bool bSwapped = static_cast< APhysicalBody * >( _Contact.Manifold->getBody0()->getUserPointer() ) == _Contact.ComponentB;

    if ( ( _ContactIndex & 1 ) == 0 ) {
        // BodyA

        if ( bSwapped ) {
            for ( int j = 0; j < _Contact.Manifold->getNumContacts(); ++j ) {
                btManifoldPoint & point = _Contact.Manifold->getContactPoint( j );
                SContactPoint & contact = ContactPoints[ j ];
                contact.Position = btVectorToFloat3( point.m_positionWorldOnA );
                contact.Normal = -btVectorToFloat3( point.m_normalWorldOnB );
                contact.Distance = point.m_distance1;
                contact.Impulse = point.m_appliedImpulse;
            }
        } else {
            for ( int j = 0; j < _Contact.Manifold->getNumContacts(); ++j ) {
                btManifoldPoint & point = _Contact.Manifold->getContactPoint( j );
                SContactPoint & contact = ContactPoints[ j ];
                contact.Position = btVectorToFloat3( point.m_positionWorldOnB );
                contact.Normal = btVectorToFloat3( point.m_normalWorldOnB );
                contact.Distance = point.m_distance1;
                contact.Impulse = point.m_appliedImpulse;
            }
        }

    } else {
        // BodyB

        if ( bSwapped ) {
            for ( int j = 0; j < _Contact.Manifold->getNumContacts(); ++j ) {
                btManifoldPoint & point = _Contact.Manifold->getContactPoint( j );
                SContactPoint & contact = ContactPoints[ j ];
                contact.Position = btVectorToFloat3( point.m_positionWorldOnB );
                contact.Normal = btVectorToFloat3( point.m_normalWorldOnB );
                contact.Distance = point.m_distance1;
                contact.Impulse = point.m_appliedImpulse;
            }
        } else {
            for ( int j = 0; j < _Contact.Manifold->getNumContacts(); ++j ) {
                btManifoldPoint & point = _Contact.Manifold->getContactPoint( j );
                SContactPoint & contact = ContactPoints[ j ];
                contact.Position = btVectorToFloat3( point.m_positionWorldOnA );
                contact.Normal = -btVectorToFloat3( point.m_normalWorldOnB );
                contact.Distance = point.m_distance1;
                contact.Impulse = point.m_appliedImpulse;
            }
        }
    }
}

APhysicsWorld::APhysicsWorld( IPhysicsWorldInterface * InOwnerWorld )
    : pOwnerWorld( InOwnerWorld )
{
    GravityVector = Float3( 0.0f, -9.81f, 0.0f );

    gContactAddedCallback = CustomMaterialCombinerCallback;

#if 0
    PhysicsBroadphase = b3New( btDbvtBroadphase ); // TODO: AxisSweep3Internal?
#else
    PhysicsBroadphase = b3New( btAxisSweep3, btVector3(-10000,-10000,-10000), btVector3(10000,10000,10000) );
#endif
    //CollisionConfiguration = b3New( btDefaultCollisionConfiguration );
    CollisionConfiguration = b3New( btSoftBodyRigidBodyCollisionConfiguration );
    CollisionDispatcher = b3New( btCollisionDispatcher, CollisionConfiguration );
    // TODO: remove this if we don't use gimpact
    btGImpactCollisionAlgorithm::registerAlgorithm( CollisionDispatcher );
    ConstraintSolver = b3New( btSequentialImpulseConstraintSolver );
    DynamicsWorld = b3New( btSoftRigidDynamicsWorld, CollisionDispatcher, PhysicsBroadphase, ConstraintSolver, CollisionConfiguration, /* SoftBodySolver */ 0 );
    DynamicsWorld->setGravity( btVectorToFloat3( GravityVector ) );
    DynamicsWorld->getDispatchInfo().m_useContinuous = true;
    //DynamicsWorld->getSolverInfo().m_splitImpulse = pOwnerWorld->bContactSolverSplitImpulse;
    //DynamicsWorld->getSolverInfo().m_numIterations = pOwnerWorld->NumContactSolverIterations;
    DynamicsWorld->getPairCache()->setOverlapFilterCallback( &CollisionFilterCallback );
    DynamicsWorld->setDebugDrawer( &PhysicsDebugDraw );
    DynamicsWorld->setInternalTickCallback( OnPrePhysics, static_cast< void * >( this ), true );
    DynamicsWorld->setInternalTickCallback( OnPostPhysics, static_cast< void * >( this ), false );
    //DynamicsWorld->setSynchronizeAllMotionStates( true ); // TODO: check how it works

    SoftBodyWorldInfo = &DynamicsWorld->getWorldInfo();
    SoftBodyWorldInfo->m_dispatcher = CollisionDispatcher;
    SoftBodyWorldInfo->m_broadphase = PhysicsBroadphase;
    SoftBodyWorldInfo->m_gravity = btVectorToFloat3( GravityVector );
    SoftBodyWorldInfo->air_density = ( btScalar )1.2;
    SoftBodyWorldInfo->water_density = 0;
    SoftBodyWorldInfo->water_offset = 0;
    SoftBodyWorldInfo->water_normal = btVector3( 0, 0, 0 );
    SoftBodyWorldInfo->m_sparsesdf.Initialize();
}

APhysicsWorld::~APhysicsWorld() {
    b3Destroy( DynamicsWorld );
    //b3Destroy( SoftBodyWorldInfo );
    b3Destroy( ConstraintSolver );
    b3Destroy( CollisionDispatcher );
    b3Destroy( CollisionConfiguration );
    b3Destroy( PhysicsBroadphase );
}

void APhysicsWorld::AddPendingBody( APhysicalBody * InPhysicalBody ) {
    INTRUSIVE_ADD_UNIQUE( InPhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail );
}

void APhysicsWorld::RemovePendingBody( APhysicalBody * InPhysicalBody ) {
    INTRUSIVE_REMOVE( InPhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail );
}

void APhysicsWorld::AddPhysicalBody( APhysicalBody * InPhysicalBody ) {
    if ( !InPhysicalBody )
    {
        // Passed a null pointer
        return;
    }

    if ( InPhysicalBody->bInWorld )
    {
        // Physical body is already in world, so remove it from the world
        if ( InPhysicalBody->RigidBody )
        {
            DynamicsWorld->removeRigidBody( InPhysicalBody->RigidBody );
        }
        InPhysicalBody->bInWorld = false;
    }

    if ( InPhysicalBody->RigidBody ) {
        // Add physical body to pending list
        AddPendingBody( InPhysicalBody );
    }
}

void APhysicsWorld::RemovePhysicalBody( APhysicalBody * InPhysicalBody ) {
    if ( !InPhysicalBody )
    {
        // Passed a null pointer
        return;
    }

    // Remove physical body from pending list
    RemovePendingBody( InPhysicalBody );

    if ( !InPhysicalBody->bInWorld )
    {
        // Physical body is not in world
        return;
    }

    DynamicsWorld->removeRigidBody( InPhysicalBody->RigidBody );

    InPhysicalBody->bInWorld = false;
}

void APhysicsWorld::AddPendingBodies() {
    APhysicalBody * next;
    for ( APhysicalBody * body = PendingAddToWorldHead ; body ; body = next )
    {
        next = body->NextMarked;

        body->NextMarked = body->PrevMarked = nullptr;

        if ( body->RigidBody )
        {
            AN_ASSERT( !body->bInWorld );
            DynamicsWorld->addRigidBody( body->RigidBody, ClampUnsignedShort( body->CollisionGroup ), ClampUnsignedShort( body->CollisionMask ) );
            body->bInWorld = true;
        }
    }
    PendingAddToWorldHead = PendingAddToWorldTail = nullptr;
}

void APhysicsWorld::DispatchContactAndOverlapEvents() {

#ifdef AN_COMPILER_MSVC
#pragma warning( disable : 4456 )
#endif

    int curTickNumber = FixedTickNumber & 1;
    int prevTickNumber = ( FixedTickNumber + 1 ) & 1;

    TPodArray< SCollisionContact > & currentContacts = CollisionContacts[ curTickNumber ];
    TPodArray< SCollisionContact > & prevContacts = CollisionContacts[ prevTickNumber ];

    THash<> & contactHash = ContactHash[ curTickNumber ];
    THash<> & prevContactHash = ContactHash[ prevTickNumber ];

    SCollisionContact contact;

    SOverlapEvent overlapEvent;
    SContactEvent contactEvent;

#if 0
    for ( int i = 0 ; i < currentContacts.Length() ; i++ ) {
        SCollisionContact & contact = currentContacts[ i ];

        contact.ActorA->RemoveRef();
        contact.ActorB->RemoveRef();
        contact.ComponentA->RemoveRef();
        contact.ComponentB->RemoveRef();
    }
#endif

    contactHash.Clear();
    currentContacts.Clear();

    int numManifolds = CollisionDispatcher->getNumManifolds();
    for ( int i = 0; i < numManifolds; i++ ) {
        btPersistentManifold * contactManifold = CollisionDispatcher->getManifoldByIndexInternal( i );

        if ( !contactManifold->getNumContacts() ) {
            continue;
        }

        APhysicalBody * objectA = static_cast< APhysicalBody * >( contactManifold->getBody0()->getUserPointer() );
        APhysicalBody * objectB = static_cast< APhysicalBody * >( contactManifold->getBody1()->getUserPointer() );

        if ( !objectA || !objectB ) {
            // ghost object
            continue;
        }

        if ( objectA < objectB ) {
            StdSwap( objectA, objectB );
        }

        AActor * actorA = objectA->GetParentActor();
        AActor * actorB = objectB->GetParentActor();

        if ( actorA->IsPendingKill() || actorB->IsPendingKill() || objectA->IsPendingKill() || objectB->IsPendingKill() ) {
            continue;
        }

#if 0
        if ( objectA->Mass <= 0.0f && objectB->Mass <= 0.0f ) {
            // Static vs Static
            GLogger.Printf( "Static vs Static %s %s\n", objectA->GetName().CStr()
                            , objectB->GetName().CStr() );
            continue;
        }

        if ( objectA->bTrigger && objectB->bTrigger ) {
            // Trigger vs Trigger
            GLogger.Printf( "Trigger vs Tsrigger %s %s\n", objectA->GetName().CStr()
                            , objectB->GetName().CStr() );
            continue;
        }

        if ( objectA->Mass <= 0.0f && objectB->bTrigger ) {
            // Static vs Trigger
            GLogger.Printf( "Static vs Trigger %s %s\n", objectA->GetName().CStr()
                , objectB->GetName().CStr() );
            continue;
        }

        if ( objectB->Mass <= 0.0f && objectA->bTrigger ) {
            // Static vs Trigger
            GLogger.Printf( "Static vs Trigger %s %s\n", objectB->GetName().CStr()
                , objectA->GetName().CStr() );
            continue;
        }
#endif

        bool bContactWithTrigger = objectA->bTrigger || objectB->bTrigger; // Do not generate contact events if one of components is trigger

        //GLogger.Printf( "Contact %s %s\n", objectA->GetName().CStr()
        //                , objectB->GetName().CStr() );

        contact.bComponentADispatchContactEvents = !bContactWithTrigger && objectA->bDispatchContactEvents
            && ( objectA->E_OnBeginContact
                || objectA->E_OnEndContact
                || objectA->E_OnUpdateContact );

        contact.bComponentBDispatchContactEvents = !bContactWithTrigger && objectB->bDispatchContactEvents
            && ( objectB->E_OnBeginContact
                || objectB->E_OnEndContact
                || objectB->E_OnUpdateContact );

        contact.bComponentADispatchOverlapEvents = objectA->bTrigger && objectA->bDispatchOverlapEvents
            && ( objectA->E_OnBeginOverlap
                || objectA->E_OnEndOverlap
                || objectA->E_OnUpdateOverlap );

        contact.bComponentBDispatchOverlapEvents = objectB->bTrigger && objectB->bDispatchOverlapEvents
            && ( objectB->E_OnBeginOverlap
                || objectB->E_OnEndOverlap
                || objectB->E_OnUpdateOverlap );

        contact.bActorADispatchContactEvents = !bContactWithTrigger && objectA->bDispatchContactEvents
            && ( actorA->E_OnBeginContact
                || actorA->E_OnEndContact
                || actorA->E_OnUpdateContact );

        contact.bActorBDispatchContactEvents = !bContactWithTrigger && objectB->bDispatchContactEvents
            && ( actorB->E_OnBeginContact
                || actorB->E_OnEndContact
                || actorB->E_OnUpdateContact );

        contact.bActorADispatchOverlapEvents = objectA->bTrigger && objectA->bDispatchOverlapEvents
            && ( actorA->E_OnBeginOverlap
                || actorA->E_OnEndOverlap
                || actorA->E_OnUpdateOverlap );

        contact.bActorBDispatchOverlapEvents = objectB->bTrigger && objectB->bDispatchOverlapEvents
           && ( actorB->E_OnBeginOverlap
                || actorB->E_OnEndOverlap
                || actorB->E_OnUpdateOverlap );

        if ( contact.bComponentADispatchContactEvents
            || contact.bComponentBDispatchContactEvents
            || contact.bComponentADispatchOverlapEvents
            || contact.bComponentBDispatchOverlapEvents
            || contact.bActorADispatchContactEvents
            || contact.bActorBDispatchContactEvents
            || contact.bActorADispatchOverlapEvents
            || contact.bActorBDispatchOverlapEvents ) {

            contact.ActorA = actorA;
            contact.ActorB = actorB;
            contact.ComponentA = objectA;
            contact.ComponentB = objectB;
            contact.Manifold = contactManifold;

            int hash = contact.Hash();

            bool bUnique = true;
#if 1//#ifdef AN_DEBUG
            for ( int h = contactHash.First( hash ) ; h != -1 ; h = contactHash.Next( h ) ) {
                if ( currentContacts[ h ].ComponentA == objectA
                    && currentContacts[ h ].ComponentB == objectB ) {
                    bUnique = false;
                    break;
                }
            }
            //AN_ASSERT( bUnique );
#endif
            if ( bUnique ) {

#if 0
                actorA->AddRef();
                actorB->AddRef();
                objectA->AddRef();
                objectB->AddRef();
#endif

                currentContacts.Append( contact );
                contactHash.Insert( hash, currentContacts.Size() - 1 );
            } else {
                GLogger.Printf( "Assertion failed: bUnique\n" );
            }
        }
    }

    // Reset cache
    CacheContactPoints = -1;

    // Dispatch contact and overlap events (OnBeginContact, OnBeginOverlap, OnUpdateContact, OnUpdateOverlap)
    for ( int i = 0 ; i < currentContacts.Size() ; i++ ) {
        SCollisionContact & contact = currentContacts[ i ];

        int hash = contact.Hash();
        bool bFirstContact = true;

        for ( int h = prevContactHash.First( hash ); h != -1; h = prevContactHash.Next( h ) ) {
            if ( prevContacts[ h ].ComponentA == contact.ComponentA
                && prevContacts[ h ].ComponentB == contact.ComponentB ) {
                bFirstContact = false;
                break;
            }
        }

        if ( contact.bActorADispatchContactEvents ) {

            if ( contact.ActorA->E_OnBeginContact || contact.ActorA->E_OnUpdateContact ) {

                if ( contact.ComponentA->bGenerateContactPoints ) {
                    GenerateContactPoints( i << 1, contact );

                    contactEvent.Points = ContactPoints.ToPtr();
                    contactEvent.NumPoints = ContactPoints.Size();
                } else {
                    contactEvent.Points = NULL;
                    contactEvent.NumPoints = 0;
                }

                contactEvent.SelfActor = contact.ActorA;
                contactEvent.SelfBody = contact.ComponentA;
                contactEvent.OtherActor = contact.ActorB;
                contactEvent.OtherBody = contact.ComponentB;

                if ( bFirstContact ) {
                    contact.ActorA->E_OnBeginContact.Dispatch( contactEvent );
                } else {
                    contact.ActorA->E_OnUpdateContact.Dispatch( contactEvent );
                }
            }

        } else if ( contact.bActorADispatchOverlapEvents ) {
            overlapEvent.SelfActor = contact.ActorA;
            overlapEvent.SelfBody = contact.ComponentA;
            overlapEvent.OtherActor = contact.ActorB;
            overlapEvent.OtherBody = contact.ComponentB;

            if ( bFirstContact ) {
                contact.ActorA->E_OnBeginOverlap.Dispatch( overlapEvent );
            } else {
                contact.ActorA->E_OnUpdateOverlap.Dispatch( overlapEvent );
            }
        }

        if ( contact.bComponentADispatchContactEvents ) {

            if ( contact.ComponentA->E_OnBeginContact || contact.ComponentA->E_OnUpdateContact ) {
                if ( contact.ComponentA->bGenerateContactPoints ) {
                    GenerateContactPoints( i << 1, contact );

                    contactEvent.Points = ContactPoints.ToPtr();
                    contactEvent.NumPoints = ContactPoints.Size();
                } else {
                    contactEvent.Points = NULL;
                    contactEvent.NumPoints = 0;
                }

                contactEvent.SelfActor = contact.ActorA;
                contactEvent.SelfBody = contact.ComponentA;
                contactEvent.OtherActor = contact.ActorB;
                contactEvent.OtherBody = contact.ComponentB;

                if ( bFirstContact ) {
                    contact.ComponentA->E_OnBeginContact.Dispatch( contactEvent );
                } else {
                    contact.ComponentA->E_OnUpdateContact.Dispatch( contactEvent );
                }
            }
        } else if ( contact.bComponentADispatchOverlapEvents ) {

            overlapEvent.SelfActor = contact.ActorA;
            overlapEvent.SelfBody = contact.ComponentA;
            overlapEvent.OtherActor = contact.ActorB;
            overlapEvent.OtherBody = contact.ComponentB;

            if ( bFirstContact ) {
                contact.ComponentA->E_OnBeginOverlap.Dispatch( overlapEvent );
            } else {
                contact.ComponentA->E_OnUpdateOverlap.Dispatch( overlapEvent );
            }
        }

        if ( contact.bActorBDispatchContactEvents ) {

            if ( contact.ActorB->E_OnBeginContact || contact.ActorB->E_OnUpdateContact ) {
                if ( contact.ComponentB->bGenerateContactPoints ) {
                    GenerateContactPoints( ( i << 1 ) + 1, contact );

                    contactEvent.Points = ContactPoints.ToPtr();
                    contactEvent.NumPoints = ContactPoints.Size();
                } else {
                    contactEvent.Points = NULL;
                    contactEvent.NumPoints = 0;
                }

                contactEvent.SelfActor = contact.ActorB;
                contactEvent.SelfBody = contact.ComponentB;
                contactEvent.OtherActor = contact.ActorA;
                contactEvent.OtherBody = contact.ComponentA;

                if ( bFirstContact ) {
                    contact.ActorB->E_OnBeginContact.Dispatch( contactEvent );
                } else {
                    contact.ActorB->E_OnUpdateContact.Dispatch( contactEvent );
                }
            }
        } else if ( contact.bActorBDispatchOverlapEvents ) {
            overlapEvent.SelfActor = contact.ActorB;
            overlapEvent.SelfBody = contact.ComponentB;
            overlapEvent.OtherActor = contact.ActorA;
            overlapEvent.OtherBody = contact.ComponentA;

            if ( bFirstContact ) {
                contact.ActorB->E_OnBeginOverlap.Dispatch( overlapEvent );
            } else {
                contact.ActorB->E_OnUpdateOverlap.Dispatch( overlapEvent );
            }
        }

        if ( contact.bComponentBDispatchContactEvents ) {

            if ( contact.ComponentB->E_OnBeginContact || contact.ComponentB->E_OnUpdateContact ) {
                if ( contact.ComponentB->bGenerateContactPoints ) {
                    GenerateContactPoints( ( i << 1 ) + 1, contact );

                    contactEvent.Points = ContactPoints.ToPtr();
                    contactEvent.NumPoints = ContactPoints.Size();
                } else {
                    contactEvent.Points = NULL;
                    contactEvent.NumPoints = 0;
                }

                contactEvent.SelfActor = contact.ActorB;
                contactEvent.SelfBody = contact.ComponentB;
                contactEvent.OtherActor = contact.ActorA;
                contactEvent.OtherBody = contact.ComponentA;

                if ( bFirstContact ) {
                    contact.ComponentB->E_OnBeginContact.Dispatch( contactEvent );
                } else {
                    contact.ComponentB->E_OnUpdateContact.Dispatch( contactEvent );
                }
            }
        } else if ( contact.bComponentBDispatchOverlapEvents ) {

            overlapEvent.SelfActor = contact.ActorB;
            overlapEvent.SelfBody = contact.ComponentB;
            overlapEvent.OtherActor = contact.ActorA;
            overlapEvent.OtherBody = contact.ComponentA;

            if ( bFirstContact ) {
                contact.ComponentB->E_OnBeginOverlap.Dispatch( overlapEvent );
            } else {
                contact.ComponentB->E_OnUpdateOverlap.Dispatch( overlapEvent );
            }
        }
    }

    // Dispatch contact and overlap events (OnEndContact, OnEndOverlap)
    for ( int i = 0; i < prevContacts.Size(); i++ ) {
        SCollisionContact & contact = prevContacts[ i ];

        int hash = contact.Hash();
        bool bHaveContact = false;

        for ( int h = contactHash.First( hash ); h != -1; h = contactHash.Next( h ) ) {
            if ( currentContacts[ h ].ComponentA == contact.ComponentA
                && currentContacts[ h ].ComponentB == contact.ComponentB ) {
                bHaveContact = true;
                break;
            }
        }

        if ( bHaveContact ) {
            continue;
        }

        if ( contact.bActorADispatchContactEvents ) {

            if ( contact.ActorA->E_OnEndContact ) {
                contactEvent.SelfActor = contact.ActorA;
                contactEvent.SelfBody = contact.ComponentA;
                contactEvent.OtherActor = contact.ActorB;
                contactEvent.OtherBody = contact.ComponentB;
                contactEvent.Points = NULL;
                contactEvent.NumPoints = 0;

                contact.ActorA->E_OnEndContact.Dispatch( contactEvent );
            }

        } else if ( contact.bActorADispatchOverlapEvents ) {
            overlapEvent.SelfActor = contact.ActorA;
            overlapEvent.SelfBody = contact.ComponentA;
            overlapEvent.OtherActor = contact.ActorB;
            overlapEvent.OtherBody = contact.ComponentB;

            contact.ActorA->E_OnEndOverlap.Dispatch( overlapEvent );
        }

        if ( contact.bComponentADispatchContactEvents ) {

            if ( contact.ComponentA->E_OnEndContact ) {
                contactEvent.SelfActor = contact.ActorA;
                contactEvent.SelfBody = contact.ComponentA;
                contactEvent.OtherActor = contact.ActorB;
                contactEvent.OtherBody = contact.ComponentB;
                contactEvent.Points = NULL;
                contactEvent.NumPoints = 0;

                contact.ComponentA->E_OnEndContact.Dispatch( contactEvent );
            }
        } else if ( contact.bComponentADispatchOverlapEvents ) {

            overlapEvent.SelfActor = contact.ActorA;
            overlapEvent.SelfBody = contact.ComponentA;
            overlapEvent.OtherActor = contact.ActorB;
            overlapEvent.OtherBody = contact.ComponentB;

            contact.ComponentA->E_OnEndOverlap.Dispatch( overlapEvent );
        }

        if ( contact.bActorBDispatchContactEvents ) {

            if ( contact.ActorB->E_OnEndContact ) {
                contactEvent.SelfActor = contact.ActorB;
                contactEvent.SelfBody = contact.ComponentB;
                contactEvent.OtherActor = contact.ActorA;
                contactEvent.OtherBody = contact.ComponentA;
                contactEvent.Points = NULL;
                contactEvent.NumPoints = 0;

                contact.ActorB->E_OnEndContact.Dispatch( contactEvent  );
            }
        } else if ( contact.bActorBDispatchOverlapEvents ) {
            overlapEvent.SelfActor = contact.ActorB;
            overlapEvent.SelfBody = contact.ComponentB;
            overlapEvent.OtherActor = contact.ActorA;
            overlapEvent.OtherBody = contact.ComponentA;

            contact.ActorB->E_OnEndOverlap.Dispatch( overlapEvent );
        }

        if ( contact.bComponentBDispatchContactEvents ) {

            if ( contact.ComponentB->E_OnEndContact ) {
                contactEvent.SelfActor = contact.ActorB;
                contactEvent.SelfBody = contact.ComponentB;
                contactEvent.OtherActor = contact.ActorA;
                contactEvent.OtherBody = contact.ComponentA;
                contactEvent.Points = NULL;
                contactEvent.NumPoints = 0;

                contact.ComponentB->E_OnEndContact.Dispatch( contactEvent );
            }
        } else if ( contact.bComponentBDispatchOverlapEvents ) {

            overlapEvent.SelfActor = contact.ActorB;
            overlapEvent.SelfBody = contact.ComponentB;
            overlapEvent.OtherActor = contact.ActorA;
            overlapEvent.OtherBody = contact.ComponentA;

            contact.ComponentB->E_OnEndOverlap.Dispatch( overlapEvent );
        }
    }
}

void APhysicsWorld::OnPrePhysics( btDynamicsWorld * _World, float _TimeStep ) {
    APhysicsWorld * ThisWorld = static_cast< APhysicsWorld * >( _World->getWorldUserInfo() );

    ThisWorld->AddPendingBodies();

    ThisWorld->pOwnerWorld->OnPrePhysics( _TimeStep );
}

void APhysicsWorld::OnPostPhysics( btDynamicsWorld * _World, float _TimeStep ) {
    APhysicsWorld * ThisWorld = static_cast< APhysicsWorld * >( _World->getWorldUserInfo() );

    ThisWorld->DispatchContactAndOverlapEvents();

    ThisWorld->pOwnerWorld->OnPostPhysics( _TimeStep );

    ThisWorld->FixedTickNumber++;
}

void APhysicsWorld::Simulate( float _TimeStep )
{
    const float FixedTimeStep = 1.0f / PhysicsHertz;

    int numSimulationSteps = Math::Floor( _TimeStep * PhysicsHertz ) + 1.0f;
    //numSimulationSteps = Math::Min( numSimulationSteps, MAX_SIMULATION_STEPS );

    btContactSolverInfo & contactSolverInfo = DynamicsWorld->getSolverInfo();
    contactSolverInfo.m_numIterations = Math::Clamp( NumContactSolverIterations, 1, 256 );
    contactSolverInfo.m_splitImpulse = bContactSolverSplitImpulse;

    if ( bGravityDirty ) {
        DynamicsWorld->setGravity( btVectorToFloat3( GravityVector ) );
        bGravityDirty = false;
    }

    bDuringPhysicsUpdate = true;

    if ( bEnablePhysicsInterpolation ) {
        TimeAccumulation = 0;
        DynamicsWorld->stepSimulation( _TimeStep, numSimulationSteps, FixedTimeStep );
    } else {
        TimeAccumulation += _TimeStep;
        while ( TimeAccumulation >= FixedTimeStep && numSimulationSteps > 0 ) {
            DynamicsWorld->stepSimulation( FixedTimeStep, 0, FixedTimeStep );
            TimeAccumulation -= FixedTimeStep;
            --numSimulationSteps;
        }
    }

    bDuringPhysicsUpdate = false;

    SoftBodyWorldInfo->m_sparsesdf.GarbageCollect();
}

void APhysicsWorld::DrawDebug( ADebugDraw * _DebugDraw ) {
    int Mode = 0;
    if ( RVDrawCollisionShapeWireframe ) {
        Mode |= APhysicsDebugDraw::DBG_DrawWireframe;
    }
    //if ( RVDrawCollisionShapeAABBs ) {
    //    Mode |= APhysicsDebugDraw::DBG_DrawAabb;
    //}
    if ( RVDrawContactPoints ) {
        Mode |= APhysicsDebugDraw::DBG_DrawContactPoints;
    }
    if ( RVDrawConstraints ) {
        Mode |= APhysicsDebugDraw::DBG_DrawConstraints;
    }
    if ( RVDrawConstraintLimits ) {
        Mode |= APhysicsDebugDraw::DBG_DrawConstraintLimits;
    }
    //if ( RVDrawCollisionShapeNormals ) {
    //    Mode |= APhysicsDebugDraw::DBG_DrawNormals;
    //}

    _DebugDraw->SetDepthTest( false );

    PhysicsDebugDraw.DD = _DebugDraw;
    PhysicsDebugDraw.setDebugMode( Mode );
    DynamicsWorld->debugDrawWorld();
}

static bool CompareDistance( SCollisionTraceResult const & A, SCollisionTraceResult const & B ) {
    return A.Distance < B.Distance;
}

static bool FindCollisionActor( SCollisionQueryFilter const & _QueryFilter, AActor * _Actor ) {
    for ( int i = 0; i < _QueryFilter.ActorsCount; i++ ) {
        if ( _Actor == _QueryFilter.IgnoreActors[ i ] ) {
            return true;
        }
    }
    return false;
}

static bool FindCollisionBody( SCollisionQueryFilter const & _QueryFilter, APhysicalBody * _Body ) {
    for ( int i = 0; i < _QueryFilter.BodiesCount; i++ ) {
        if ( _Body == _QueryFilter.IgnoreBodies[ i ] ) {
            return true;
        }
    }
    return false;
}

AN_FORCEINLINE static bool NeedsCollision( SCollisionQueryFilter const & _QueryFilter, btBroadphaseProxy * _Proxy ) {
    APhysicalBody * body = static_cast< APhysicalBody * >( static_cast< btCollisionObject * >( _Proxy->m_clientObject )->getUserPointer() );

    if ( body ) {
        if ( FindCollisionActor( _QueryFilter, body->GetParentActor() ) ) {
            return false;
        }

        if ( FindCollisionBody( _QueryFilter, body ) ) {
            return false;
        }
    } else {
        // ghost object
    }

    return ( _Proxy->m_collisionFilterGroup & _QueryFilter.CollisionMask ) && _Proxy->m_collisionFilterMask;
}

struct STraceRayResultCallback : btCollisionWorld::AllHitsRayResultCallback {

    STraceRayResultCallback( SCollisionQueryFilter const * _QueryFilter, btVector3 const & rayFromWorld, btVector3 const & rayToWorld )
        : btCollisionWorld::AllHitsRayResultCallback( rayFromWorld, rayToWorld )
        , QueryFilter( _QueryFilter ? *_QueryFilter : DefaultCollisionQueryFilter )
    {
        m_collisionFilterGroup = ( short )0xffff;
        m_collisionFilterMask = ClampUnsignedShort( QueryFilter.CollisionMask );

        m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
        m_flags |= btTriangleRaycastCallback::kF_KeepUnflippedNormal;

        //    enum EFlags
        //    {
        //       kF_None                 = 0,
        //       kF_FilterBackfaces      = 1 << 0,
        //       kF_KeepUnflippedNormal  = 1 << 1,   // Prevents returned face normal getting flipped when a ray hits a back-facing triangle
        //         ///SubSimplexConvexCastRaytest is the default, even if kF_None is set.
        //       kF_UseSubSimplexConvexCastRaytest = 1 << 2,   // Uses an approximate but faster ray versus convex intersection algorithm
        //       kF_UseGjkConvexCastRaytest = 1 << 3,
        //       kF_Terminator        = 0xFFFFFFFF
        //    };
    }

    bool needsCollision( btBroadphaseProxy* proxy0 ) const override {
        return NeedsCollision( QueryFilter, proxy0 );
    }

    SCollisionQueryFilter const & QueryFilter;
};

struct STraceClosestRayResultCallback : btCollisionWorld::ClosestRayResultCallback {

    STraceClosestRayResultCallback( SCollisionQueryFilter const * _QueryFilter, btVector3 const & rayFromWorld, btVector3 const & rayToWorld )
        : ClosestRayResultCallback( rayFromWorld, rayToWorld )
        , QueryFilter( _QueryFilter ? *_QueryFilter : DefaultCollisionQueryFilter )
    {
        m_collisionFilterGroup = ( short )0xffff;
        m_collisionFilterMask = ClampUnsignedShort( QueryFilter.CollisionMask );

        m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;
        m_flags |= btTriangleRaycastCallback::kF_KeepUnflippedNormal;
    }

    bool needsCollision( btBroadphaseProxy* proxy0 ) const override {
        return NeedsCollision( QueryFilter, proxy0 );
    }

    SCollisionQueryFilter const & QueryFilter;
};

struct STraceClosestConvexResultCallback : btCollisionWorld::ClosestConvexResultCallback {

    STraceClosestConvexResultCallback( SCollisionQueryFilter const * _QueryFilter, btVector3 const & rayFromWorld, btVector3 const & rayToWorld )
        : ClosestConvexResultCallback( rayFromWorld, rayToWorld )
        , QueryFilter( _QueryFilter ? *_QueryFilter : DefaultCollisionQueryFilter )
    {
        m_collisionFilterGroup = ( short )0xffff;
        m_collisionFilterMask = ClampUnsignedShort( QueryFilter.CollisionMask );
    }

    bool needsCollision( btBroadphaseProxy* proxy0 ) const override {
        return NeedsCollision( QueryFilter, proxy0 );
    }

    SCollisionQueryFilter const & QueryFilter;
};

bool APhysicsWorld::Trace( TPodArray< SCollisionTraceResult > & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    STraceRayResultCallback hitResult( _QueryFilter, btVectorToFloat3( _RayStart ), btVectorToFloat3( _RayEnd ) );

    DynamicsWorld->rayTest( hitResult.m_rayFromWorld, hitResult.m_rayToWorld, hitResult );

    _Result.Resize( hitResult.m_collisionObjects.size() );

    for ( int i = 0; i < hitResult.m_collisionObjects.size(); i++ ) {
        SCollisionTraceResult & result = _Result[ i ];
        result.Body = static_cast< APhysicalBody * >( hitResult.m_collisionObjects[ i ]->getUserPointer() );
        result.Position = btVectorToFloat3( hitResult.m_hitPointWorld[ i ] );
        result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld[ i ] );
        result.Distance = ( result.Position - _RayStart ).Length();
        result.Fraction = hitResult.m_closestHitFraction;
    }

    //if ( bForcePerTriangleCheck ) {
        // TODO: check intersection with triangles!
    //}

    if ( !_QueryFilter ) {
        _QueryFilter = &DefaultCollisionQueryFilter;
    }

    if ( _QueryFilter->bSortByDistance ) {
        StdSort( _Result.Begin(), _Result.End(), CompareDistance );
    }

    return !_Result.IsEmpty();
}

bool APhysicsWorld::TraceClosest( SCollisionTraceResult & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    STraceClosestRayResultCallback hitResult( _QueryFilter, btVectorToFloat3( _RayStart ), btVectorToFloat3( _RayEnd ) );

    DynamicsWorld->rayTest( hitResult.m_rayFromWorld, hitResult.m_rayToWorld, hitResult );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_collisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = ( _Result.Position - _RayStart ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

bool APhysicsWorld::TraceSphere( SCollisionTraceResult & _Result, float _Radius, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    STraceClosestConvexResultCallback hitResult( _QueryFilter, btVectorToFloat3( _RayStart ), btVectorToFloat3( _RayEnd ) );

    btSphereShape shape( _Radius );
    shape.setMargin( 0.0f );

    DynamicsWorld->convexSweepTest( &shape,
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexFromWorld ),
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexToWorld ), hitResult );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_hitCollisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = hitResult.m_closestHitFraction * ( _RayEnd - _RayStart ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

bool APhysicsWorld::TraceBox( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    Float3 boxPosition = ( _Maxs + _Mins ) * 0.5f;
    Float3 halfExtents = ( _Maxs - _Mins ) * 0.5f;
    Float3 startPos = boxPosition + _RayStart;
    Float3 endPos = boxPosition + _RayEnd;

    STraceClosestConvexResultCallback hitResult( _QueryFilter, btVectorToFloat3( startPos ), btVectorToFloat3( endPos ) );

    btBoxShape shape( btVectorToFloat3( halfExtents ) );
    shape.setMargin( 0.0f );

    DynamicsWorld->convexSweepTest( &shape,
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexFromWorld ),
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexToWorld ), hitResult );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_hitCollisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = hitResult.m_closestHitFraction * ( endPos - startPos ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

bool APhysicsWorld::TraceCylinder( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    Float3 boxPosition = ( _Maxs + _Mins ) * 0.5f;
    Float3 halfExtents = ( _Maxs - _Mins ) * 0.5f;
    Float3 startPos = boxPosition + _RayStart;
    Float3 endPos = boxPosition + _RayEnd;

    STraceClosestConvexResultCallback hitResult( _QueryFilter, btVectorToFloat3( startPos ), btVectorToFloat3( endPos ) );

    btCylinderShape shape( btVectorToFloat3( halfExtents ) );
    shape.setMargin( 0.0f );

    DynamicsWorld->convexSweepTest( &shape,
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexFromWorld ),
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexToWorld ), hitResult );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_hitCollisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = hitResult.m_closestHitFraction * ( endPos - startPos ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

bool APhysicsWorld::TraceCapsule( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter ) const {
    Float3 boxPosition = ( _Maxs + _Mins ) * 0.5f;
    Float3 halfExtents = ( _Maxs - _Mins ) * 0.5f;
    Float3 startPos = boxPosition + _RayStart;
    Float3 endPos = boxPosition + _RayEnd;

    STraceClosestConvexResultCallback hitResult( _QueryFilter, btVectorToFloat3( startPos ), btVectorToFloat3( endPos ) );

    float radius = Math::Max( halfExtents[0], halfExtents[2] );

    btCapsuleShape shape( radius, (halfExtents[1]-radius)*2.0f );
    shape.setMargin( 0.0f );

    DynamicsWorld->convexSweepTest( &shape,
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexFromWorld ),
        btTransform( btQuaternion::getIdentity(), hitResult.m_convexToWorld ), hitResult );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_hitCollisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = hitResult.m_closestHitFraction * ( endPos - startPos ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

bool APhysicsWorld::TraceConvex( SCollisionTraceResult & _Result, SConvexSweepTest const & _SweepTest ) const {
    if ( !_SweepTest.CollisionBody->IsConvex() ) {
        GLogger.Printf( "AWorld::TraceConvex: non-convex collision body for convex trace\n" );
        _Result.Clear();
        return false;
    }

    btCollisionShape * shape = _SweepTest.CollisionBody->Create();
    shape->setMargin( _SweepTest.CollisionBody->Margin );

    AN_ASSERT( shape->isConvex() );

    Float3x4 startTransform, endTransform;

    startTransform.Compose( _SweepTest.StartPosition, _SweepTest.StartRotation.ToMatrix(), _SweepTest.Scale );
    endTransform.Compose( _SweepTest.EndPosition, _SweepTest.EndRotation.ToMatrix(), _SweepTest.Scale );

    Float3 startPos = startTransform * _SweepTest.CollisionBody->Position;
    Float3 endPos = endTransform * _SweepTest.CollisionBody->Position;
    Quat startRot = _SweepTest.StartRotation * _SweepTest.CollisionBody->Rotation;
    Quat endRot = _SweepTest.EndRotation * _SweepTest.CollisionBody->Rotation;

    STraceClosestConvexResultCallback hitResult( &_SweepTest.QueryFilter, btVectorToFloat3( startPos ), btVectorToFloat3( endPos ) );

    DynamicsWorld->convexSweepTest( static_cast< btConvexShape * >( shape ),
        btTransform( btQuaternionToQuat( startRot ), hitResult.m_convexFromWorld ),
        btTransform( btQuaternionToQuat( endRot ), hitResult.m_convexToWorld ), hitResult );

    b3Destroy( shape );

    if ( !hitResult.hasHit() ) {
        _Result.Clear();
        return false;
    }

    _Result.Body = static_cast< APhysicalBody * >( hitResult.m_hitCollisionObject->getUserPointer() );
    _Result.Position = btVectorToFloat3( hitResult.m_hitPointWorld );
    _Result.Normal = btVectorToFloat3( hitResult.m_hitNormalWorld );
    _Result.Distance = hitResult.m_closestHitFraction * ( endPos - startPos ).Length();
    _Result.Fraction = hitResult.m_closestHitFraction;
    return true;
}

struct SQueryPhysicalBodiesCallback : public btCollisionWorld::ContactResultCallback {
    SQueryPhysicalBodiesCallback( TPodArray< APhysicalBody * > & _Result, SCollisionQueryFilter const * _QueryFilter )
        : Result( _Result )
        , QueryFilter( _QueryFilter ? *_QueryFilter : DefaultCollisionQueryFilter )
    {
        _Result.Clear();

        m_collisionFilterGroup = ( short )0xffff;
        m_collisionFilterMask = ClampUnsignedShort( QueryFilter.CollisionMask );
    }

    bool needsCollision( btBroadphaseProxy* proxy0 ) const override {
        return NeedsCollision( QueryFilter, proxy0 );
    }

    btScalar addSingleResult( btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1 ) override {
        APhysicalBody * body;

        body = reinterpret_cast< APhysicalBody * >( colObj0Wrap->getCollisionObject()->getUserPointer() );
        if ( body && Result.Find( body ) == Result.End() && ( body->CollisionGroup & QueryFilter.CollisionMask ) ) {
            Result.Append( body );
        }

        body = reinterpret_cast< APhysicalBody * >( colObj1Wrap->getCollisionObject()->getUserPointer() );
        if ( body && Result.Find( body ) == Result.End() && ( body->CollisionGroup & QueryFilter.CollisionMask ) ) {
            Result.Append( body );
        }

        return 0.0f;
    }

    TPodArray< APhysicalBody * > & Result;
    SCollisionQueryFilter const & QueryFilter;
};

struct SQueryActorsCallback : public btCollisionWorld::ContactResultCallback {
    SQueryActorsCallback( TPodArray< AActor * > & _Result, SCollisionQueryFilter const * _QueryFilter )
        : Result( _Result )
        , QueryFilter( _QueryFilter ? *_QueryFilter : DefaultCollisionQueryFilter )
    {
        _Result.Clear();

        m_collisionFilterGroup = ( short )0xffff;
        m_collisionFilterMask = ClampUnsignedShort( QueryFilter.CollisionMask );
    }

    bool needsCollision( btBroadphaseProxy* proxy0 ) const override {
        return NeedsCollision( QueryFilter, proxy0 );
    }

    btScalar addSingleResult( btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1 ) override {
        APhysicalBody * body;

        body = reinterpret_cast< APhysicalBody * >( colObj0Wrap->getCollisionObject()->getUserPointer() );
        if ( body && Result.Find( body->GetParentActor() ) == Result.End() && ( body->CollisionGroup & QueryFilter.CollisionMask ) ) {
            Result.Append( body->GetParentActor() );
        }

        body = reinterpret_cast< APhysicalBody * >( colObj1Wrap->getCollisionObject()->getUserPointer() );
        if ( body && Result.Find( body->GetParentActor() ) == Result.End() && ( body->CollisionGroup & QueryFilter.CollisionMask ) ) {
            Result.Append( body->GetParentActor() );
        }

        return 0.0f;
    }

    TPodArray< AActor * > & Result;
    SCollisionQueryFilter const & QueryFilter;
};

void APhysicsWorld::QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter ) const {
    SQueryPhysicalBodiesCallback callback( _Result, _QueryFilter );
    btSphereShape shape( _Radius );
    shape.setMargin( 0.0f );
    btRigidBody * tempBody = b3New( btRigidBody, 1.0f, nullptr, &shape );
    tempBody->setWorldTransform( btTransform( btQuaternion::getIdentity(), btVectorToFloat3( _Position ) ) );
    tempBody->activate();
    btSoftRigidDynamicsWorld * physWorld = const_cast< btSoftRigidDynamicsWorld * >( DynamicsWorld );
    physWorld->addRigidBody( tempBody );
    physWorld->contactTest( tempBody, callback );
    physWorld->removeRigidBody( tempBody );
    b3Destroy( tempBody );
}

void APhysicsWorld::QueryActors( TPodArray< AActor * > & _Result, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter ) const {
    SQueryActorsCallback callback( _Result, _QueryFilter );
    btSphereShape shape( _Radius );
    shape.setMargin( 0.0f );
    btRigidBody * tempBody = b3New( btRigidBody, 1.0f, nullptr, &shape );
    tempBody->setWorldTransform( btTransform( btQuaternion::getIdentity(), btVectorToFloat3( _Position ) ) );
    tempBody->activate();
    btSoftRigidDynamicsWorld * physWorld = const_cast< btSoftRigidDynamicsWorld * >( DynamicsWorld );
    physWorld->addRigidBody( tempBody );
    physWorld->contactTest( tempBody, callback );
    physWorld->removeRigidBody( tempBody );
    b3Destroy( tempBody );
}

void APhysicsWorld::QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, Float3 const & _Position, Float3 const & _HalfExtents, SCollisionQueryFilter const * _QueryFilter ) const {
    SQueryPhysicalBodiesCallback callback( _Result, _QueryFilter );
    btBoxShape shape( btVectorToFloat3( _HalfExtents ) );
    shape.setMargin( 0.0f );
    btRigidBody * tempBody = b3New( btRigidBody, 1.0f, nullptr, &shape );
    tempBody->setWorldTransform( btTransform( btQuaternion::getIdentity(), btVectorToFloat3( _Position ) ) );
    tempBody->activate();
    btSoftRigidDynamicsWorld * physWorld = const_cast< btSoftRigidDynamicsWorld * >( DynamicsWorld );
    physWorld->addRigidBody( tempBody );
    physWorld->contactTest( tempBody, callback );
    physWorld->removeRigidBody( tempBody );
    b3Destroy( tempBody );
}

void APhysicsWorld::QueryActors( TPodArray< AActor * > & _Result, Float3 const & _Position, Float3 const & _HalfExtents, SCollisionQueryFilter const * _QueryFilter ) const {
    SQueryActorsCallback callback( _Result, _QueryFilter );
    btBoxShape shape( btVectorToFloat3( _HalfExtents ) );
    shape.setMargin( 0.0f );
    btRigidBody * tempBody = b3New( btRigidBody, 1.0f, nullptr, &shape );
    tempBody->setWorldTransform( btTransform( btQuaternion::getIdentity(), btVectorToFloat3( _Position ) ) );
    tempBody->activate();
    btSoftRigidDynamicsWorld * physWorld = const_cast< btSoftRigidDynamicsWorld * >( DynamicsWorld );
    physWorld->addRigidBody( tempBody );
    physWorld->contactTest( tempBody, callback );
    physWorld->removeRigidBody( tempBody );
    b3Destroy( tempBody );
}

void APhysicsWorld::QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, BvAxisAlignedBox const & _BoundingBox, SCollisionQueryFilter const * _QueryFilter ) const {
    QueryPhysicalBodies( _Result, _BoundingBox.Center(), _BoundingBox.HalfSize(), _QueryFilter );
}

void APhysicsWorld::QueryActors( TPodArray< AActor * > & _Result, BvAxisAlignedBox const & _BoundingBox, SCollisionQueryFilter const * _QueryFilter ) const {
    QueryActors( _Result, _BoundingBox.Center(), _BoundingBox.HalfSize(), _QueryFilter );
}
