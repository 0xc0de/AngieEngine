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

#include <Engine/World/Public/World.h>
#include <Engine/World/Public/Actors/Pawn.h>
#include <Engine/World/Public/Components/SkinnedComponent.h>
#include <Engine/World/Public/Components/DirectionalLightComponent.h>
#include <Engine/World/Public/Components/PointLightComponent.h>
#include <Engine/World/Public/Components/SpotLightComponent.h>
#include <Engine/World/Public/Timer.h>
#include <Engine/Core/Public/Logger.h>
#include <Engine/Core/Public/IntrusiveLinkedListMacro.h>
#include <Engine/Core/Public/BV/BvIntersect.h>
#include <Engine/Runtime/Public/Runtime.h>
#include <Engine/Base/Public/GameModuleInterface.h>
#include "ShadowCascade.h"

#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>
#include <btBulletDynamicsCommon.h>

#include <Engine/BulletCompatibility/BulletCompatibility.h>

#ifdef AN_COMPILER_MSVC
#pragma warning(push)
#pragma warning( disable : 4456 )
#endif
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#ifdef AN_COMPILER_MSVC
#pragma warning(pop)
#endif

AN_BEGIN_CLASS_META( AWorld )
//AN_ATTRIBUTE_( bTickEvenWhenPaused, AF_DEFAULT )
AN_END_CLASS_META()

ARuntimeVariable RVDrawCollisionShapeWireframe( _CTS( "DrawCollisionShapeWireframe" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawContactPoints( _CTS( "DrawContactPoints" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawConstraints( _CTS( "DrawConstraints" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVDrawConstraintLimits( _CTS( "DrawConstraintLimits" ), _CTS( "0" ), VAR_CHEAT );

AN_FORCEINLINE static unsigned short ClampUnsignedShort( int _Value ) {
    if ( _Value < 0 ) return 0;
    if ( _Value > 0xffff ) return 0xffff;
    return _Value;
}

void SActorSpawnInfo::SetTemplate( AActor const * _Template ) {
    AN_Assert( &_Template->FinalClassMeta() == ActorTypeClassMeta );
    Template = _Template;
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

struct ACollisionFilterCallback : public btOverlapFilterCallback {

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

static ACollisionFilterCallback CollisionFilterCallback;

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

AWorld * AWorld::PendingKillWorlds = nullptr;

AWorld::AWorld() {
    PersistentLevel = NewObject< ALevel >();
    PersistentLevel->AddRef();
    PersistentLevel->OwnerWorld = this;
    PersistentLevel->bIsPersistent = true;
    PersistentLevel->IndexInArrayOfLevels = ArrayOfLevels.Size();
    ArrayOfLevels.Append( PersistentLevel );

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
    PhysicsWorld = b3New( btSoftRigidDynamicsWorld, CollisionDispatcher, PhysicsBroadphase, ConstraintSolver, CollisionConfiguration, /* SoftBodySolver */ 0 );
    PhysicsWorld->setGravity( btVectorToFloat3( GravityVector ) );
    PhysicsWorld->getDispatchInfo().m_useContinuous = true;
    PhysicsWorld->getSolverInfo().m_splitImpulse = bContactSolverSplitImpulse;
    PhysicsWorld->getSolverInfo().m_numIterations = NumContactSolverIterations;
    PhysicsWorld->getPairCache()->setOverlapFilterCallback( &CollisionFilterCallback );
    PhysicsWorld->setDebugDrawer( &PhysicsDebugDraw );
    PhysicsWorld->setInternalTickCallback( OnPrePhysics, static_cast< void * >( this ), true );
    PhysicsWorld->setInternalTickCallback( OnPostPhysics, static_cast< void * >( this ), false );
    //PhysicsWorld->setSynchronizeAllMotionStates( true ); // TODO: check how it works

    SoftBodyWorldInfo = &PhysicsWorld->getWorldInfo();
    SoftBodyWorldInfo->m_dispatcher = CollisionDispatcher;
    SoftBodyWorldInfo->m_broadphase = PhysicsBroadphase;
    SoftBodyWorldInfo->m_gravity = btVectorToFloat3( GravityVector );
    SoftBodyWorldInfo->air_density = ( btScalar )1.2;
    SoftBodyWorldInfo->water_density = 0;
    SoftBodyWorldInfo->water_offset = 0;
    SoftBodyWorldInfo->water_normal = btVector3( 0, 0, 0 );
    SoftBodyWorldInfo->m_sparsesdf.Initialize();
}

void AWorld::SetPaused( bool _Paused ) {
    bPauseRequest = _Paused;
    bUnpauseRequest = !_Paused;
}

bool AWorld::IsPaused() const {
    return bPaused;
}

void AWorld::ResetGameplayTimer() {
    bResetGameplayTimer = true;
}

void AWorld::OnPrePhysics( btDynamicsWorld * _World, float _TimeStep ) {
    static_cast< AWorld * >( _World->getWorldUserInfo() )->OnPrePhysics( _TimeStep );
}

void AWorld::OnPostPhysics( btDynamicsWorld * _World, float _TimeStep ) {
    static_cast< AWorld * >( _World->getWorldUserInfo() )->OnPostPhysics( _TimeStep );
}

void AWorld::SetGravityVector( Float3 const & _Gravity ) {
    GravityVector = _Gravity;
    bGravityDirty = true;
}

Float3 const & AWorld::GetGravityVector() const {
    return GravityVector;
}

void AWorld::Destroy() {
    if ( bPendingKill ) {
        return;
    }

    // Mark world to remove it from the game
    bPendingKill = true;
    NextPendingKillWorld = PendingKillWorlds;
    PendingKillWorlds = this;

#if 0
    for ( int i = 0 ; i < 2 ; i++ ) {
        TPodArray< SCollisionContact > & currentContacts = CollisionContacts[ i ];
        THash<> & contactHash = ContactHash[ i ];

        for ( SCollisionContact & contact : currentContacts ) {
            contact.ActorA->RemoveRef();
            contact.ActorB->RemoveRef();
            contact.ComponentA->RemoveRef();
            contact.ComponentB->RemoveRef();
        }

        currentContacts.Clear();
        contactHash.Clear();
    }
#endif

    DestroyActors();
    KickoffPendingKillObjects();

    // Remove all levels from world including persistent level
    for ( ALevel * level : ArrayOfLevels ) {
        if ( !level->bIsPersistent ) {
            level->OnRemoveLevelFromWorld();
        }
        level->IndexInArrayOfLevels = -1;
        level->OwnerWorld = nullptr;
        level->RemoveRef();
    }
    ArrayOfLevels.Clear();

    b3Destroy( PhysicsWorld );
    //b3Destroy( SoftBodyWorldInfo );
    b3Destroy( ConstraintSolver );
    b3Destroy( CollisionDispatcher );
    b3Destroy( CollisionConfiguration );
    b3Destroy( PhysicsBroadphase );

    EndPlay();
}

void AWorld::DestroyActors() {
    for ( AActor * actor : Actors ) {
        actor->Destroy();
    }
}

SActorSpawnInfo::SActorSpawnInfo( uint64_t _ActorClassId )
    : SActorSpawnInfo( AActor::Factory().LookupClass( _ActorClassId ) )
{
}

SActorSpawnInfo::SActorSpawnInfo( const char * _ActorClassName )
    : SActorSpawnInfo( AActor::Factory().LookupClass( _ActorClassName ) )
{
}

AActor * AWorld::SpawnActor( SActorSpawnInfo const & _SpawnParameters ) {

    //GLogger.Printf( "==== Spawn Actor ====\n" );

    AClassMeta const * classMeta = _SpawnParameters.ActorClassMeta();

    if ( !classMeta ) {
        GLogger.Printf( "AWorld::SpawnActor: invalid actor class\n" );
        return nullptr;
    }

    if ( classMeta->Factory() != &AActor::Factory() ) {
        GLogger.Printf( "AWorld::SpawnActor: not an actor class\n" );
        return nullptr;
    }

    AActor const * templateActor = _SpawnParameters.GetTemplate();

    if ( templateActor && classMeta != &templateActor->FinalClassMeta() ) {
        GLogger.Printf( "AWorld::SpawnActor: FActorSpawnParameters::Template class doesn't match meta data\n" );
        return nullptr;
    }

    AActor * actor = static_cast< AActor * >( classMeta->CreateInstance() );
    actor->AddRef();
    actor->bDuringConstruction = false;

    if ( _SpawnParameters.Instigator ) {
        actor->Instigator = _SpawnParameters.Instigator;
        actor->Instigator->AddRef();
    }

    // Add actor to world array of actors
    Actors.Append( actor );
    actor->IndexInWorldArrayOfActors = Actors.Size() - 1;
    actor->ParentWorld = this;

    actor->Level = _SpawnParameters.Level ? _SpawnParameters.Level : PersistentLevel;
    actor->Level->Actors.Append( actor );
    actor->IndexInLevelArrayOfActors = actor->Level->Actors.Size() - 1;

    // Update actor name to make it unique
    //actor->SetObjectName( actor->Name );

    if ( templateActor ) {
        actor->Clone( templateActor );
    }

    actor->Initialize( _SpawnParameters.SpawnTransform );

    BroadcastActorSpawned( actor );

    //GLogger.Printf( "=====================\n" );
    return actor;
}

static Float3 ReadFloat3( ADocument const & _Document, int _FieldsHead, const char * _FieldName, Float3 const & _Default ) {
    SDocumentField * field = _Document.FindField( _FieldsHead, _FieldName );
    if ( !field ) {
        return _Default;
    }

    SDocumentValue * value = &_Document.Values[ field->ValuesHead ];

    Float3 r;
    sscanf( value->Token.ToString().CStr(), "%f %f %f", &r.X, &r.Y, &r.Z );
    return r;
}

static Quat ReadQuat( ADocument const & _Document, int _FieldsHead, const char * _FieldName, Quat const & _Default ) {
    SDocumentField * field = _Document.FindField( _FieldsHead, _FieldName );
    if ( !field ) {
        return _Default;
    }

    SDocumentValue * value = &_Document.Values[ field->ValuesHead ];

    Quat r;
    sscanf( value->Token.ToString().CStr(), "%f %f %f %f", &r.X, &r.Y, &r.Z, &r.W );
    return r;
}

AActor * AWorld::LoadActor( ADocument const & _Document, int _FieldsHead, ALevel * _Level ) {

    //GLogger.Printf( "==== Load Actor ====\n" );

    SDocumentField * classNameField = _Document.FindField( _FieldsHead, "ClassName" );
    if ( !classNameField ) {
        GLogger.Printf( "AWorld::LoadActor: invalid actor class\n" );
        return nullptr;
    }

    SDocumentValue * classNameValue = &_Document.Values[ classNameField->ValuesHead ];

    AClassMeta const * classMeta = AActor::Factory().LookupClass( classNameValue->Token.ToString().CStr() );
    if ( !classMeta ) {
        GLogger.Printf( "AWorld::LoadActor: invalid actor class \"%s\"\n", classNameValue->Token.ToString().CStr() );
        return nullptr;
    }

    AActor * actor = static_cast< AActor * >( classMeta->CreateInstance() );
    actor->AddRef();
    actor->bDuringConstruction = false;

    // Add actor to world array of actors
    Actors.Append( actor );
    actor->IndexInWorldArrayOfActors = Actors.Size() - 1;
    actor->ParentWorld = this;

    actor->Level = _Level ? _Level : PersistentLevel;
    actor->Level->Actors.Append( actor );
    actor->IndexInLevelArrayOfActors = actor->Level->Actors.Size() - 1;

    // Update actor name to make it unique
    //actor->SetObjectName( actor->Name );

    // Load actor attributes
    actor->LoadAttributes( _Document, _FieldsHead );

#if 0
    // Load components
    SDocumentField * componentsArray = _Document.FindField( _FieldsHead, "Components" );
    for ( int i = componentsArray->ValuesHead ; i != -1 ; i = _Document.Values[ i ].Next ) {
        SDocumentValue const * componentObject = &_Document.Values[ i ];
        if ( componentObject->Type != SDocumentValue::T_Object ) {
            continue;
        }
        actor->LoadComponent( _Document, componentObject->FieldsHead );
    }

    // Load root component
    SDocumentField * rootField = _Document.FindField( _FieldsHead, "Root" );
    if ( rootField ) {
        SDocumentValue * rootValue = &_Document.Values[ rootField->ValuesHead ];
        ASceneComponent * root = dynamic_cast< ASceneComponent * >( actor->FindComponent( rootValue->Token.ToString().CStr() ) );
        if ( root ) {
            actor->RootComponent = root;
        }
    }
#endif

    ATransform spawnTransform;

    spawnTransform.Position = ReadFloat3( _Document, _FieldsHead, "SpawnPosition", Float3(0.0f) );
    spawnTransform.Rotation = ReadQuat( _Document, _FieldsHead, "SpawnRotation", Quat::Identity() );
    spawnTransform.Scale    = ReadFloat3( _Document, _FieldsHead, "SpawnScale", Float3(1.0f) );

    actor->Initialize( spawnTransform );

    BroadcastActorSpawned( actor );

    //GLogger.Printf( "=====================\n" );
    return actor;
}

//AString AWorld::GenerateActorUniqueName( const char * _Name ) {
//    // TODO: optimize!
//    if ( !FindActor( _Name ) ) {
//        return _Name;
//    }
//    int uniqueNumber = 0;
//    AString uniqueName;
//    do {
//        uniqueName.Resize( 0 );
//        uniqueName.Concat( _Name );
//        uniqueName.Concat( Int( ++uniqueNumber ).CStr() );
//    } while ( FindActor( uniqueName.CStr() ) != nullptr );
//    return uniqueName;
//}

//AActor * AWorld::FindActor( const char * _UniqueName ) {
//    // TODO: Use hash!
//    for ( AActor * actor : Actors ) {
//        if ( !actor->GetName().Icmp( _UniqueName ) ) {
//            return actor;
//        }
//    }
//    return nullptr;
//}

void AWorld::BroadcastActorSpawned( AActor * _SpawnedActor ) {    
    E_OnActorSpawned.Dispatch( _SpawnedActor );
}

void AWorld::BeginPlay() {
    GLogger.Printf( "AWorld::BeginPlay()\n" );
}

void AWorld::EndPlay() {
    GLogger.Printf( "AWorld::EndPlay()\n" );
}

void AWorld::Tick( float _TimeStep ) {

    //UpdatePauseStatus();
    if ( bPauseRequest ) {
        bPauseRequest = false;
        bPaused = true;
        GLogger.Printf( "Game paused\n" );
    } else if ( bUnpauseRequest ) {
        bUnpauseRequest = false;
        bPaused = false;
        GLogger.Printf( "Game unpaused\n" );
    }

    //UpdateWorldTime();
    GameRunningTimeMicro = GameRunningTimeMicroAfterTick;
    GameplayTimeMicro = GameplayTimeMicroAfterTick;

    //UpdateTimers();
    // Tick all timers. TODO: move timer tick to PrePhysicsTick?
    for ( ATimer * timer = TimerList ; timer ; timer = timer->Next ) {
        timer->Tick( this, _TimeStep );
    }

    //UpdateActors();
    // Tick actors
    for ( AActor * actor : Actors ) {
        if ( actor->IsPendingKill() ) {
            continue;
        }

        if ( bPaused && !actor->bTickEvenWhenPaused ) {
            continue;
        }

        actor->TickComponents( _TimeStep );

        if ( actor->bCanEverTick ) {
            actor->Tick( _TimeStep );
        }
    }

    SimulatePhysics( _TimeStep );

    // Update levels
    for ( ALevel * level : ArrayOfLevels ) {
        level->Tick( _TimeStep );
    }

    KickoffPendingKillObjects();

    uint64_t frameDuration = (double)_TimeStep * 1000000;
    GameRunningTimeMicroAfterTick += frameDuration;
}

void AWorld::AddPhysicalBody( APhysicalBody * _PhysicalBody ) {
    if ( !INTRUSIVE_EXISTS( _PhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail ) ) {
        INTRUSIVE_ADD( _PhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail );
    }
}

void AWorld::RemovePhysicalBody( APhysicalBody * _PhysicalBody ) {
    if ( INTRUSIVE_EXISTS( _PhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail ) ) {
        INTRUSIVE_REMOVE( _PhysicalBody, NextMarked, PrevMarked, PendingAddToWorldHead, PendingAddToWorldTail );
    }
}

void AWorld::OnPrePhysics( float _TimeStep ) {

    GameplayTimeMicro = GameplayTimeMicroAfterTick;

    // Add physical bodies
    APhysicalBody * next;
    for ( APhysicalBody * body = PendingAddToWorldHead ; body ; body = next ) {
        next = body->NextMarked;

        body->NextMarked = body->PrevMarked = nullptr;

        if ( body->RigidBody ) {
            AN_Assert( !body->bInWorld );
            PhysicsWorld->addRigidBody( body->RigidBody, ClampUnsignedShort( body->CollisionGroup ), ClampUnsignedShort( body->CollisionMask ) );
            body->bInWorld = true;
        }
    }
    PendingAddToWorldHead = PendingAddToWorldTail = nullptr;

    // Tick actors
    for ( AActor * actor : Actors ) {
        if ( actor->IsPendingKill() ) {
            continue;
        }

        if ( actor->bCanEverTick && actor->bTickPrePhysics ) {
            //actor->TickComponentsPrePhysics( _TimeStep );
            actor->TickPrePhysics( _TimeStep );
        }
    }
}

static int CacheContactPoints = -1;

void AWorld::GenerateContactPoints( int _ContactIndex, SCollisionContact & _Contact ) {
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

void AWorld::OnPostPhysics( float _TimeStep ) {

    DispatchContactAndOverlapEvents();

    for ( AActor * actor : Actors ) {
        if ( actor->IsPendingKill() ) {
            continue;
        }

        if ( actor->bCanEverTick && actor->bTickPostPhysics ) {

            //actor->TickComponentsPostPhysics( _TimeStep );
            actor->TickPostPhysics( _TimeStep );

        }

        // Update actor life span
        actor->LifeTime += _TimeStep;

        if ( actor->LifeSpan > 0.0f ) {
            actor->LifeSpan -= _TimeStep;

            if ( actor->LifeSpan < 0.0f ) {
                actor->Destroy();
            }
        }
    }

    //WorldLocalTime += _TimeStep;

    FixedTickNumber++;

    if ( bResetGameplayTimer ) {
        bResetGameplayTimer = false;
        GameplayTimeMicroAfterTick = 0;
    } else {
        GameplayTimeMicroAfterTick += (double)_TimeStep * 1000000.0;
    }
}

void AWorld::DispatchContactAndOverlapEvents() {

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
            //AN_Assert( bUnique );
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

void AWorld::SimulatePhysics( float _TimeStep ) {
    if ( bPaused ) {
        return;
    }

    const float FixedTimeStep = 1.0f / PhysicsHertz;

    int numSimulationSteps = Math::Floor( _TimeStep * PhysicsHertz ) + 1.0f;
    //numSimulationSteps = Math::Min( numSimulationSteps, MAX_SIMULATION_STEPS );

    btContactSolverInfo & contactSolverInfo = PhysicsWorld->getSolverInfo();
    contactSolverInfo.m_numIterations = Math::Clamp( NumContactSolverIterations, 1, 256 );
    contactSolverInfo.m_splitImpulse = bContactSolverSplitImpulse;

    if ( bGravityDirty ) {
        PhysicsWorld->setGravity( btVectorToFloat3( GravityVector ) );
        bGravityDirty = false;
    }

    bDuringPhysicsUpdate = true;

    if ( bEnablePhysicsInterpolation ) {
        TimeAccumulation = 0;
        PhysicsWorld->stepSimulation( _TimeStep, numSimulationSteps, FixedTimeStep );
    } else {
        TimeAccumulation += _TimeStep;
        while ( TimeAccumulation >= FixedTimeStep && numSimulationSteps > 0 ) {
            PhysicsWorld->stepSimulation( FixedTimeStep, 0, FixedTimeStep );
            TimeAccumulation -= FixedTimeStep;
            --numSimulationSteps;
        }
    }

    bDuringPhysicsUpdate = false;

    SoftBodyWorldInfo->m_sparsesdf.GarbageCollect();
}

void AWorld::ApplyRadialDamage( float _DamageAmount, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter ) {
    TPodArray< AActor * > damagedActors;
    QueryActors( damagedActors, _Position, _Radius, _QueryFilter );
    for ( AActor * damagedActor : damagedActors ) {
        damagedActor->ApplyDamage( _DamageAmount, _Position, nullptr );
    }
}

void AWorld::KickoffPendingKillObjects() {
    while ( PendingKillComponents ) {
        AActorComponent * component = PendingKillComponents;
        AActorComponent * nextComponent;

        PendingKillComponents = nullptr;

        while ( component ) {
            nextComponent = component->NextPendingKillComponent;

            // FIXME: Call component->EndPlay here?

            // Remove component from actor array of components
            AActor * parent = component->ParentActor;
            if ( parent /*&& !parent->IsPendingKill()*/ ) {
                parent->Components[ component->ComponentIndex ] = parent->Components[ parent->Components.Size() - 1 ];
                parent->Components[ component->ComponentIndex ]->ComponentIndex = component->ComponentIndex;
                parent->Components.RemoveLast();
            }
            component->ComponentIndex = -1;
            component->ParentActor = nullptr;
            component->RemoveRef();

            component = nextComponent;
        }
    }

    while ( PendingKillActors ) {
        AActor * actor = PendingKillActors;
        AActor * nextActor;

        PendingKillActors = nullptr;

        while ( actor ) {
            nextActor = actor->NextPendingKillActor;

            // FIXME: Call actor->EndPlay here?

            // Remove actor from world array of actors
            Actors[ actor->IndexInWorldArrayOfActors ] = Actors[ Actors.Size() - 1 ];
            Actors[ actor->IndexInWorldArrayOfActors ]->IndexInWorldArrayOfActors = actor->IndexInWorldArrayOfActors;
            Actors.RemoveLast();
            actor->IndexInWorldArrayOfActors = -1;
            actor->ParentWorld = nullptr;

            // Remove actor from level array of actors
            ALevel * level = actor->Level;
            level->Actors[ actor->IndexInLevelArrayOfActors ] = level->Actors[ level->Actors.Size() - 1 ];
            level->Actors[ actor->IndexInLevelArrayOfActors ]->IndexInLevelArrayOfActors = actor->IndexInLevelArrayOfActors;
            level->Actors.RemoveLast();
            actor->IndexInLevelArrayOfActors = -1;
            actor->Level = nullptr;

            actor->RemoveRef();

            actor = nextActor;
        }
    }
}
//#include <unordered_set>
int AWorld::Serialize( ADocument & _Doc ) {
    int object = Super::Serialize( _Doc );

    if ( !Actors.IsEmpty() ) {
        int actors = _Doc.AddArray( object, "Actors" );

//        std::unordered_set< std::string > precacheStrs;

        for ( AActor * actor : Actors ) {
            if ( actor->IsPendingKill() ) {
                continue;
            }
            int actorObject = actor->Serialize( _Doc );
            _Doc.AddValueToField( actors, actorObject );

//            AClassMeta const & classMeta = actor->FinalClassMeta();

//            for ( APrecacheMeta const * precache = classMeta.GetPrecacheList() ; precache ; precache = precache->Next() ) {
//                precacheStrs.insert( precache->GetResourcePath() );
//            }
//            // TODO: precache all objects, not only actors
        }

//        if ( !precacheStrs.empty() ) {
//            int precache = _Doc.AddArray( object, "Precache" );
//            for ( auto it : precacheStrs ) {
//                const std::string & s = it;

//                _Doc.AddValueToField( precache, _Doc.CreateStringValue( _Doc.ProxyBuffer.NewString( s.c_str() ).CStr() ) );
//            }
//        }
    }

    return object;
}

void AWorld::AddLevel( ALevel * _Level ) {
    if ( _Level->IsPersistentLevel() ) {
        GLogger.Printf( "AWorld::AddLevel: Can't add persistent level\n" );
        return;
    }

    if ( _Level->OwnerWorld == this ) {
        // Already in world
        return;
    }

    if ( _Level->OwnerWorld ) {
        _Level->OwnerWorld->RemoveLevel( _Level );
    }

    _Level->OwnerWorld = this;
    _Level->IndexInArrayOfLevels = ArrayOfLevels.Size();
    _Level->AddRef();
    _Level->OnAddLevelToWorld();
    ArrayOfLevels.Append( _Level );
}

void AWorld::RemoveLevel( ALevel * _Level ) {
    if ( !_Level ) {
        return;
    }

    if ( _Level->IsPersistentLevel() ) {
        GLogger.Printf( "AWorld::AddLevel: Can't remove persistent level\n" );
        return;
    }

    if ( _Level->OwnerWorld != this ) {
        GLogger.Printf( "AWorld::AddLevel: level is not in world\n" );
        return;
    }

    _Level->OnRemoveLevelFromWorld();

    ArrayOfLevels[ _Level->IndexInArrayOfLevels ] = ArrayOfLevels[ ArrayOfLevels.Size() - 1 ];
    ArrayOfLevels[ _Level->IndexInArrayOfLevels ]->IndexInArrayOfLevels = _Level->IndexInArrayOfLevels;
    ArrayOfLevels.RemoveLast();

    _Level->OwnerWorld = nullptr;
    _Level->IndexInArrayOfLevels = -1;
    _Level->RemoveRef();
}

void AWorld::AddMesh( AMeshComponent * _Mesh ) {
    if ( INTRUSIVE_EXISTS( _Mesh, Next, Prev, MeshList, MeshListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Mesh, Next, Prev, MeshList, MeshListTail );
}

void AWorld::RemoveMesh( AMeshComponent * _Mesh ) {
    INTRUSIVE_REMOVE( _Mesh, Next, Prev, MeshList, MeshListTail );
}

void AWorld::AddSkinnedMesh( ASkinnedComponent * _Skeleton ) {
    if ( INTRUSIVE_EXISTS( _Skeleton, Next, Prev, SkinnedMeshList, SkinnedMeshListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Skeleton, Next, Prev, SkinnedMeshList, SkinnedMeshListTail );
}

void AWorld::RemoveSkinnedMesh( ASkinnedComponent * _Skeleton ) {
    INTRUSIVE_REMOVE( _Skeleton, Next, Prev, SkinnedMeshList, SkinnedMeshListTail );
}

void AWorld::AddShadowCaster( AMeshComponent * _Mesh ) {
    if ( INTRUSIVE_EXISTS( _Mesh, NextShadowCaster, PrevShadowCaster, ShadowCasters, ShadowCastersTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Mesh, NextShadowCaster, PrevShadowCaster, ShadowCasters, ShadowCastersTail );
}

void AWorld::RemoveShadowCaster( AMeshComponent * _Mesh ) {
    INTRUSIVE_REMOVE( _Mesh, NextShadowCaster, PrevShadowCaster, ShadowCasters, ShadowCastersTail );
}

void AWorld::AddDirectionalLight( ADirectionalLightComponent * _Light ) {
    if ( INTRUSIVE_EXISTS( _Light, Next, Prev, DirectionalLightList, DirectionalLightListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Light, Next, Prev, DirectionalLightList, DirectionalLightListTail );
}

void AWorld::RemoveDirectionalLight( ADirectionalLightComponent * _Light ) {
    INTRUSIVE_REMOVE( _Light, Next, Prev, DirectionalLightList, DirectionalLightListTail );
}

void AWorld::AddPointLight( APointLightComponent * _Light ) {
    if ( INTRUSIVE_EXISTS( _Light, Next, Prev, PointLightList, PointLightListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Light, Next, Prev, PointLightList, PointLightListTail );
}

void AWorld::RemovePointLight( APointLightComponent * _Light ) {
    INTRUSIVE_REMOVE( _Light, Next, Prev, PointLightList, PointLightListTail );
}

void AWorld::AddSpotLight( ASpotLightComponent * _Light ) {
    if ( INTRUSIVE_EXISTS( _Light, Next, Prev, SpotLightList, SpotLightListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Light, Next, Prev, SpotLightList, SpotLightListTail );
}

void AWorld::RemoveSpotLight( ASpotLightComponent * _Light ) {
    INTRUSIVE_REMOVE( _Light, Next, Prev, SpotLightList, SpotLightListTail );
}

void AWorld::RegisterTimer( ATimer * _Timer ) {
    if ( INTRUSIVE_EXISTS( _Timer, Next, Prev, TimerList, TimerListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    INTRUSIVE_ADD( _Timer, Next, Prev, TimerList, TimerListTail );
}

void AWorld::UnregisterTimer( ATimer * _Timer ) {
    INTRUSIVE_REMOVE( _Timer, Next, Prev, TimerList, TimerListTail );
}

void AWorld::DrawDebug( ADebugDraw * _DebugDraw ) {

    for ( ALevel * level : ArrayOfLevels ) {
        level->DrawDebug( _DebugDraw );
    }

    _DebugDraw->SetDepthTest( true );

    for ( AActor * actor : Actors ) {
        actor->DrawDebug( _DebugDraw );
    }

    _DebugDraw->SetDepthTest( false );
    PhysicsDebugDraw.DD = _DebugDraw;

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

    PhysicsDebugDraw.setDebugMode( Mode );
    PhysicsWorld->debugDrawWorld();
}

void AWorld::RenderFrontend_AddInstances( SRenderFrontendDef * _Def ) {
    SRenderFrame * frameData = GRuntime.GetFrameData();
    SRenderView * view = _Def->View;

    for ( ALevel * level : ArrayOfLevels ) {
        level->RenderFrontend_AddInstances( _Def );
    }

    // Add directional lights
    for ( ADirectionalLightComponent * light = DirectionalLightList ; light ; light = light->Next ) {

        if ( view->NumDirectionalLights > MAX_DIRECTIONAL_LIGHTS ) {
            GLogger.Printf( "MAX_DIRECTIONAL_LIGHTS hit\n" );
            break;
        }

        if ( !light->IsEnabled() ) {
            continue;
        }

        SDirectionalLightDef * lightDef = (SDirectionalLightDef *)GRuntime.AllocFrameMem( sizeof( SDirectionalLightDef ) );
        if ( !lightDef ) {
            break;
        }

        frameData->DirectionalLights.Append( lightDef );

        lightDef->ColorAndAmbientIntensity = light->GetEffectiveColor();
        lightDef->Matrix = light->GetWorldRotation().ToMatrix();
        lightDef->MaxShadowCascades = light->GetMaxShadowCascades();
        lightDef->RenderMask = light->RenderingGroup;
        lightDef->NumCascades = 0;  // this will be calculated later
        lightDef->FirstCascade = 0; // this will be calculated later
        lightDef->bCastShadow = light->bCastShadow;

        view->NumDirectionalLights++;
    }


    // Add point lights
    for ( APointLightComponent * light = PointLightList ; light ; light = light->Next ) {

        if ( !light->IsEnabled() ) {
            continue;
        }

        // TODO: cull light

        SLightDef * lightDef = (SLightDef *)GRuntime.AllocFrameMem( sizeof( SLightDef ) );
        if ( !lightDef ) {
            break;
        }

        frameData->Lights.Append( lightDef );

        lightDef->bSpot = false;
        lightDef->BoundingBox = light->GetWorldBounds();
        lightDef->ColorAndAmbientIntensity = light->GetEffectiveColor();
        lightDef->Position = light->GetWorldPosition();
        lightDef->RenderMask = light->RenderingGroup;
        lightDef->InnerRadius = light->GetInnerRadius();
        lightDef->OuterRadius = light->GetOuterRadius();
        lightDef->OBBTransformInverse = light->GetOBBTransformInverse();

        view->NumLights++;
    }


    // Add spot lights
    for ( ASpotLightComponent * light = SpotLightList ; light ; light = light->Next ) {

        if ( !light->IsEnabled() ) {
            continue;
        }

        // TODO: cull light

        SLightDef * lightDef = (SLightDef *)GRuntime.AllocFrameMem( sizeof( SLightDef ) );
        if ( !lightDef ) {
            break;
        }

        frameData->Lights.Append( lightDef );

        lightDef->bSpot = true;
        lightDef->BoundingBox = light->GetWorldBounds();
        lightDef->ColorAndAmbientIntensity = light->GetEffectiveColor();
        lightDef->Position = light->GetWorldPosition();
        lightDef->RenderMask = light->RenderingGroup;        
        lightDef->InnerRadius = light->GetInnerRadius();
        lightDef->OuterRadius = light->GetOuterRadius();
        lightDef->InnerConeAngle = light->GetInnerConeAngle();
        lightDef->OuterConeAngle = light->GetOuterConeAngle();
        lightDef->SpotDirection = light->GetWorldDirection();
        lightDef->SpotExponent = light->GetSpotExponent();
        lightDef->OBBTransformInverse = light->GetOBBTransformInverse();

        view->NumLights++;
    }

    void Voxelize( SRenderFrame * Frame, SRenderView * RV );

    Voxelize( frameData, view );
}

void AWorld::RenderFrontend_AddDirectionalShadowmapInstances( SRenderFrontendDef * _Def ) {

    CreateDirectionalLightCascades( GRuntime.GetFrameData(), _Def->View );

    if ( !_Def->View->NumShadowMapCascades ) {
        return;
    }

    // Create shadow instances

    for ( AMeshComponent * component = ShadowCasters ; component ; component = component->GetNextShadowCaster() ) {

        // TODO: Perform culling for each shadow cascade, set CascadeMask

        //if ( component->RenderMark == _Def->VisMarker ) {
        //    return;
        //}

        if ( (component->RenderingGroup & _Def->RenderingMask) == 0 ) {
        //    component->RenderMark = _Def->VisMarker;
            continue;
        }

//        if ( component->VSDPasses & VSD_PASS_FACE_CULL ) {
//            // TODO: bTwoSided and bFrontSided must came from component
//            const bool bTwoSided = false;
//            const bool bFrontSided = true;
//            const float EPS = 0.25f;
//
//            if ( !bTwoSided ) {
//                PlaneF const & plane = component->FacePlane;
//                float d = _Def->View->ViewPosition.Dot( plane.Normal );
//
//                bool bFaceCull = false;
//
//                if ( bFrontSided ) {
//                    if ( d < -plane.D - EPS ) {
//                        bFaceCull = true;
//                    }
//                } else {
//                    if ( d > -plane.D + EPS ) {
//                        bFaceCull = true;
//                    }
//                }
//
//                if ( bFaceCull ) {
//                    component->RenderMark = _Def->VisMarker;
//#ifdef DEBUG_TRAVERSING_COUNTERS
//                    Dbg_CulledByDotProduct++;
//#endif
//                    return;
//                }
//            }
//        }

//        if ( component->VSDPasses & VSD_PASS_BOUNDS ) {
//
//            // TODO: use SSE cull
//            BvAxisAlignedBox const & bounds = component->GetWorldBounds();
//
//            if ( Cull( _CullPlanes, _CullPlanesCount, bounds ) ) {
//#ifdef DEBUG_TRAVERSING_COUNTERS
//                Dbg_CulledBySurfaceBounds++;
//#endif
//                return;
//            }
//        }

//        component->RenderMark = _Def->VisMarker;

        //if ( component->VSDPasses & VSD_PASS_CUSTOM_VISIBLE_STEP ) {

        //    bool bVisible;
        //    component->RenderFrontend_CustomVisibleStep( _Def, bVisible );

        //    if ( !bVisible ) {
        //        return;
        //    }
        //}

        //if ( component->VSDPasses & VSD_PASS_VIS_MARKER ) {
        //    bool bVisible = component->VisMarker == _Def->VisMarker;
        //    if ( !bVisible ) {
        //        return;
        //    }
        //}

        Float3x4 const * instanceMatrix;

        AIndexedMesh * mesh = component->GetMesh();

        size_t skeletonOffset = 0;
        size_t skeletonSize = 0;
        if ( mesh->IsSkinned() && component->IsSkinnedMesh() ) {
            ASkinnedComponent * skeleton = static_cast< ASkinnedComponent * >(component);

            // TODO: Если кости уже были обновлены на этом кадре, то не нужно их обновлять снова!
            skeleton->UpdateJointTransforms( skeletonOffset, skeletonSize );
        }

        if ( component->bNoTransform ) {
            instanceMatrix = &Float3x4::Identity();
        } else {
            instanceMatrix = &component->GetWorldTransformMatrix();
        }

        AIndexedMeshSubpartArray const & subparts = mesh->GetSubparts();

        for ( int subpartIndex = 0; subpartIndex < subparts.Size(); subpartIndex++ ) {

            // FIXME: check subpart bounding box here

            AIndexedMeshSubpart * subpart = subparts[subpartIndex];

            AMaterialInstance * materialInstance = component->GetMaterialInstance( subpartIndex );
            AN_Assert( materialInstance );

            AMaterial * material = materialInstance->GetMaterial();

            // Prevent rendering of instances with disabled shadow casting
            if ( material->GetGPUResource()->bNoCastShadow ) {
                continue;
            }

            SMaterialFrameData * materialInstanceFrameData = materialInstance->RenderFrontend_Update( _Def->VisMarker );

            // Add render instance
            SShadowRenderInstance * instance = (SShadowRenderInstance *)GRuntime.AllocFrameMem( sizeof( SShadowRenderInstance ) );
            if ( !instance ) {
                break;
            }

            GRuntime.GetFrameData()->ShadowInstances.Append( instance );

            instance->Material = material->GetGPUResource();
            instance->MaterialInstance = materialInstanceFrameData;
            instance->VertexBuffer = mesh->GetVertexBufferGPU();
            instance->IndexBuffer = mesh->GetIndexBufferGPU();
            instance->WeightsBuffer = mesh->GetWeightsBufferGPU();

            if ( component->bUseDynamicRange ) {
                instance->IndexCount = component->DynamicRangeIndexCount;
                instance->StartIndexLocation = component->DynamicRangeStartIndexLocation;
                instance->BaseVertexLocation = component->DynamicRangeBaseVertexLocation;
            } else {
                instance->IndexCount = subpart->GetIndexCount();
                instance->StartIndexLocation = subpart->GetFirstIndex();
                instance->BaseVertexLocation = subpart->GetBaseVertex() + component->SubpartBaseVertexOffset;
            }

            instance->SkeletonOffset = skeletonOffset;
            instance->SkeletonSize = skeletonSize;
            instance->WorldTransformMatrix = *instanceMatrix;
            instance->CascadeMask = 0xffff; // TODO: Calculate!!!

            _Def->View->ShadowInstanceCount++;

            _Def->ShadowMapPolyCount += instance->IndexCount / 3;

            if ( component->bUseDynamicRange ) {
                // If component uses dynamic range, mesh has actually one subpart
                break;
            }
        }
    }
}


TPodArray< AWorld * > AWorld::Worlds;

AWorld * AWorld::CreateWorld() {
    AWorld * world = CreateInstanceOf< AWorld >();

    world->AddRef();

    // Add world to game array of worlds
    Worlds.Append( world );
    world->IndexInGameArrayOfWorlds = Worlds.Size() - 1;

    world->BeginPlay();

    return world;
}

void AWorld::DestroyWorlds() {
    for ( AWorld * world : Worlds ) {
        world->Destroy();
    }
}

void AWorld::KickoffPendingKillWorlds() {
    while ( PendingKillWorlds ) {
        AWorld * world = PendingKillWorlds;
        AWorld * nextWorld;

        PendingKillWorlds = nullptr;

        while ( world ) {
            nextWorld = world->NextPendingKillWorld;

            // FIXME: Call world->EndPlay here?

            // Remove world from game array of worlds
            Worlds[ world->IndexInGameArrayOfWorlds ] = Worlds[ Worlds.Size() - 1 ];
            Worlds[ world->IndexInGameArrayOfWorlds ]->IndexInGameArrayOfWorlds = world->IndexInGameArrayOfWorlds;
            Worlds.RemoveLast();
            world->IndexInGameArrayOfWorlds = -1;
            world->RemoveRef();

            world = nextWorld;
        }
    }
}

void AWorld::UpdateWorlds( IGameModule * _GameModule, float _TimeStep ) {
    _GameModule->OnPreGameTick( _TimeStep );
    for ( AWorld * world : Worlds ) {
        if ( world->IsPendingKill() ) {
            continue;
        }
        world->Tick( _TimeStep );
    }
    _GameModule->OnPostGameTick( _TimeStep );

    KickoffPendingKillWorlds();

    ASpatialObject::_UpdateSurfaceAreas();
}
