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

#include "Level.h"
#include "WorldRaycastQuery.h"
#include "PhysicsWorld.h"
#include "RenderWorld.h"
#include "AINavigationMesh.h"

class AActor;
class APawn;
class AActorComponent;
class ACameraComponent;
class ATimer;
class ADebugDraw;
class IGameModule;

/** Actor spawn parameters */
struct SActorSpawnInfo {

    /** Initial actor transform */
    ATransform SpawnTransform;

    /** Level for actor spawn */
    ALevel * Level;

    /** Who spawn the actor */
    APawn * Instigator;

    SActorSpawnInfo() = delete;

    SActorSpawnInfo( AClassMeta const * _ActorTypeClassMeta )
        : Level( nullptr )
        , Instigator( nullptr )
        , Template( nullptr )
        , ActorTypeClassMeta( _ActorTypeClassMeta )
    {
    }

    SActorSpawnInfo( uint64_t _ActorClassId );

    SActorSpawnInfo( const char * _ActorClassName );

    /** Set actor template */
    void SetTemplate( AActor const * _Template );

    /** Get actor template */
    AActor const * GetTemplate() const { return Template; }

    /** Get actor meta class */
    AClassMeta const * ActorClassMeta() const { return ActorTypeClassMeta; }

protected:

    /** NOTE: template type meta must match ActorTypeClassMeta */
    AActor const * Template;

    /** Actor type */
    AClassMeta const * ActorTypeClassMeta;
};

/** Template helper for actor spawn parameters */
template< typename ActorType >
struct TActorSpawnInfo : SActorSpawnInfo
{
    TActorSpawnInfo()
        : SActorSpawnInfo( &ActorType::ClassMeta() )
    {
    }
};

/** AWorld. Defines a game map or editor/tool scene */
class ANGIE_API AWorld : public ABaseObject, public IPhysicsWorldInterface
{
    AN_CLASS( AWorld, ABaseObject )

    friend class AActor;
    friend class AActorComponent;

public:
    /** Scale audio volume in the entire world */
    float AudioVolume = 1.0f;

    /** Delegate to notify when any actor spawned */
    using AOnActorSpawned = TEvent< AActor * >;
    AOnActorSpawned E_OnActorSpawned;

    /** Delegate to prepare for rendering */
    using AOnPrepareRenderFrontend = TEvent< ACameraComponent *, int >;
    AOnPrepareRenderFrontend E_OnPrepareRenderFrontend;

    /** Create a new world */
    static AWorld * CreateWorld();

    /** Destroy all existing worlds */
    static void DestroyWorlds();

    /** Get array of worlds */
    static TPodArray< AWorld * > const & GetWorlds() { return Worlds; }

    /** Tick the worlds */
    static void UpdateWorlds( IGameModule * _GameModule, float _TimeStep );

    /** Remove worlds, marked pending kill */
    static void KickoffPendingKillWorlds();

    void BuildNavigation( SAINavigationConfig const & _NavigationConfig );

    /** Spawn a new actor */
    AActor * SpawnActor( SActorSpawnInfo const & _SpawnParameters );

    /** Spawn a new actor */
    template< typename ActorType >
    ActorType * SpawnActor( TActorSpawnInfo< ActorType > const & _SpawnParameters ) {
        SActorSpawnInfo const & spawnParameters = _SpawnParameters;
        return static_cast< ActorType * >( SpawnActor( spawnParameters ) );
    }

    /** Spawn a new actor */
    template< typename ActorType >
    ActorType * SpawnActor( ALevel * _Level = nullptr ) {
        TActorSpawnInfo< ActorType > spawnParameters;
        spawnParameters.Level = _Level;
        return static_cast< ActorType * >( SpawnActor( spawnParameters ) );
    }

    /** Spawn a new actor */
    template< typename ActorType >
    ActorType * SpawnActor( ATransform const & _SpawnTransform, ALevel * _Level = nullptr ) {
        TActorSpawnInfo< ActorType > spawnParameters;
        spawnParameters.SpawnTransform = _SpawnTransform;
        spawnParameters.Level = _Level;
        return static_cast< ActorType * >( SpawnActor( spawnParameters ) );
    }

    /** Spawn a new actor */
    template< typename ActorType >
    ActorType * SpawnActor( Float3 const & _Position, Quat const & _Rotation, ALevel * _Level = nullptr ) {
        TActorSpawnInfo< ActorType > spawnParameters;
        spawnParameters.SpawnTransform.Position = _Position;
        spawnParameters.SpawnTransform.Rotation = _Rotation;
        spawnParameters.Level = _Level;
        return static_cast< ActorType * >( SpawnActor( spawnParameters ) );
    }

    /** Load actor from the document */
    AActor * LoadActor( ADocument const & _Document, int _FieldsHead, ALevel * _Level = nullptr );

    /** Get all actors in the world */
    TPodArray< AActor * > const & GetActors() const { return Actors; }

    /** Serialize world to the document */
    int Serialize( ADocument & _Doc ) override;

    /** Destroy this world */
    void Destroy();

    /** Destroy all actors in the world */
    void DestroyActors();

    /** Add level to the world */
    void AddLevel( ALevel * _Level );

    /** Remove level from the world */
    void RemoveLevel( ALevel * _Level );

    /** Get world's persistent level */
    ALevel * GetPersistentLevel() { return PersistentLevel; }

    /** Get all levels in the world */
    TPodArray< ALevel * > const & GetArrayOfLevels() const { return ArrayOfLevels; }

    /** Pause the game. Freezes world and actor ticking since the next game tick. */
    void SetPaused( bool _Paused );

    /** Returns current pause state */
    bool IsPaused() const;

    /** Game virtual time based on variable frame step */
    int64_t GetRunningTimeMicro() const { return GameRunningTimeMicro; }

    /** Gameplay virtual time based on fixed frame step, running when unpaused */
    int64_t GetGameplayTimeMicro() const { return GameplayTimeMicro; }

    /** Reset gameplay timer to zero. This is delayed operation. */
    void ResetGameplayTimer();

    /** Physics simulation refresh rate */
    void SetPhysicsHertz( int _Hertz );

    /** Enable interpolation during physics simulation */
    void SetPhysicsInterpolation( bool _Interpolation );

    /** Contact solver split impulse. Disabled by default for performance */
    void SetContactSolverSplitImpulse( bool _SplitImpulse );

    /** Contact solver iterations count */
    void SetContactSolverIterations( int _InterationsCount );

    /** Set world gravity vector */
    void SetGravityVector( Float3 const & _Gravity );

    /** Get world gravity vector */
    Float3 const & GetGravityVector() const;

    /** Is in physics update now */
    bool IsDuringPhysicsUpdate() const { return PhysicsWorld.bDuringPhysicsUpdate; }

    /** Is world destroyed, but not removed yet. */
    bool IsPendingKill() const { return bPendingKill; }

    /** Per-triangle raycast */
    bool Raycast( SWorldRaycastResult & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SWorldRaycastFilter * _Filter = nullptr ) const {
        return AWorldRaycastQuery::Raycast( &RenderWorld, _Result, _RayStart, _RayEnd, _Filter );
    }

    /** Per-AABB raycast */
    bool RaycastAABB( TPodArray< SBoxHitResult > & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SWorldRaycastFilter * _Filter = nullptr ) const {
        return AWorldRaycastQuery::RaycastAABB( &RenderWorld, _Result, _RayStart, _RayEnd, _Filter );
    }

    /** Per-triangle raycast */
    bool RaycastClosest( SWorldRaycastClosestResult & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SWorldRaycastFilter * _Filter = nullptr ) const {
        return AWorldRaycastQuery::RaycastClosest( &RenderWorld, _Result, _RayStart, _RayEnd, _Filter );
    }

    /** Per-AABB raycast */
    bool RaycastClosestAABB( SBoxHitResult & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SWorldRaycastFilter * _Filter = nullptr ) const {
        return AWorldRaycastQuery::RaycastClosestAABB( &RenderWorld, _Result, _RayStart, _RayEnd, _Filter );
    }

    /** Trace collision bodies */
    bool Trace( TPodArray< SCollisionTraceResult > & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.Trace( _Result, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceClosest( SCollisionTraceResult & _Result, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.TraceClosest( _Result, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceSphere( SCollisionTraceResult & _Result, float _Radius, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.TraceSphere( _Result, _Radius, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceBox( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.TraceBox( _Result, _Mins, _Maxs, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceCylinder( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.TraceCylinder( _Result, _Mins, _Maxs, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceCapsule( SCollisionTraceResult & _Result, Float3 const & _Mins, Float3 const & _Maxs, Float3 const & _RayStart, Float3 const & _RayEnd, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        return PhysicsWorld.TraceCapsule( _Result, _Mins, _Maxs, _RayStart, _RayEnd, _QueryFilter );
    }

    /** Trace collision bodies */
    bool TraceConvex( SCollisionTraceResult & _Result, SConvexSweepTest const & _SweepTest ) const {
        return PhysicsWorld.TraceConvex( _Result, _SweepTest );
    }

    /** Query objects in sphere */
    void QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryPhysicalBodies( _Result, _Position, _Radius, _QueryFilter );
    }

    /** Query objects in box */
    void QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, Float3 const & _Position, Float3 const & _HalfExtents, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryPhysicalBodies( _Result, _Position, _HalfExtents, _QueryFilter );
    }

    /** Query objects in AABB */
    void QueryPhysicalBodies( TPodArray< APhysicalBody * > & _Result, BvAxisAlignedBox const & _BoundingBox, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryPhysicalBodies( _Result, _BoundingBox, _QueryFilter );
    }

    /** Query objects in sphere */
    void QueryActors( TPodArray< AActor * > & _Result, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryActors( _Result, _Position, _Radius, _QueryFilter );
    }

    /** Query objects in box */
    void QueryActors( TPodArray< AActor * > & _Result, Float3 const & _Position, Float3 const & _HalfExtents, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryActors( _Result, _Position, _HalfExtents, _QueryFilter );
    }

    /** Query objects in AABB */
    void QueryActors( TPodArray< AActor * > & _Result, BvAxisAlignedBox const & _BoundingBox, SCollisionQueryFilter const * _QueryFilter = nullptr ) const {
        PhysicsWorld.QueryActors( _Result, _BoundingBox, _QueryFilter );
    }

    /** Apply amount of damage in specified radius */
    void ApplyRadialDamage( float _DamageAmount, Float3 const & _Position, float _Radius, SCollisionQueryFilter const * _QueryFilter = nullptr );

    //
    // Internal
    //

    APhysicsWorld & GetPhysicsWorld() { return PhysicsWorld; }

    ARenderWorld & GetRenderWorld() { return RenderWorld; }

    AAINavigationMesh & GetNavigationMesh() { return NavigationMesh; }

    btSoftRigidDynamicsWorld * GetDynamicsWorld() { return PhysicsWorld.DynamicsWorld; }
    btSoftRigidDynamicsWorld const * GetDynamicsWorld() const { return PhysicsWorld.DynamicsWorld; }

    btSoftBodyWorldInfo * GetSoftBodyWorldInfo() { return PhysicsWorld.SoftBodyWorldInfo; }

    void UpdateDrawableArea( ADrawable * _Drawable );

    void DrawDebug( ADebugDraw * _DebugDraw );

protected:
    void BeginPlay();

    void EndPlay();

    void Tick( float _TimeStep );

    AWorld();

private:
    void BroadcastActorSpawned( AActor * _SpawnedActor );

    void RegisterTimer( ATimer * _Timer );      // friend AActor
    void UnregisterTimer( ATimer * _Timer );    // friend AActor

    void KickoffPendingKillObjects();

    void UpdatePauseStatus();
    void UpdateTimers( float _TimeStep );
    void UpdateActors( float _TimeStep );
    void UpdateActorsPrePhysics( float _TimeStep );
    void UpdateActorsPostPhysics( float _TimeStep );
    void UpdateLevels( float _TimeStep );
    void UpdatePhysics( float _TimeStep );
    void UpdateDrawableAreas();

    // IPhysicsWorldInterface
    void OnPrePhysics( float _TimeStep ) override;
    void OnPostPhysics( float _TimeStep ) override;

    TPodArray< AActor * > Actors;

    bool bPauseRequest;
    bool bUnpauseRequest;
    bool bPaused;
    bool bResetGameplayTimer;

    // Game virtual time based on variable frame step
    int64_t GameRunningTimeMicro;
    int64_t GameRunningTimeMicroAfterTick;

    // Gameplay virtual time based on fixed frame step, running when unpaused
    int64_t GameplayTimeMicro;
    int64_t GameplayTimeMicroAfterTick;

    ATimer * TimerList;
    ATimer * TimerListTail;

    int IndexInGameArrayOfWorlds = -1;

    bool bPendingKill;

    AActor * PendingKillActors; // friend AActor
    AActorComponent * PendingKillComponents; // friend AActorComponent

    AWorld * NextPendingKillWorld;
    static AWorld * PendingKillWorlds;

    // All existing worlds
    static TPodArray< AWorld * > Worlds;

    TRef< ALevel > PersistentLevel;
    TPodArray< ALevel * > ArrayOfLevels;

    APhysicsWorld PhysicsWorld;
    ARenderWorld RenderWorld;
    AAINavigationMesh NavigationMesh;

    ADrawable * DrawableUpdateList;
    ADrawable * DrawableUpdateListTail;
};


#include "Actors/Actor.h"
#include "Components/ActorComponent.h"

/*

TActorIterator

Iterate world actors

Example:
for ( TActorIterator< FMyActor > it( GetWorld() ) ; it ; ++it ) {
    FMyActor * actor = *it;
    ...
}

*/
template< typename T >
struct TActorIterator {
    explicit TActorIterator( AWorld * _World )
        : Actors( _World->GetActors() ), i(0)
    {
        Next();
    }

    operator bool() const {
        return Actor != nullptr;
    }

    T * operator++() {
        Next();
        return Actor;
    }

    T * operator++(int) {
        T * a = Actor;
        Next();
        return a;
    }

    T * operator*() const {
        return Actor;
    }

    T * operator->() const {
        return Actor;
    }

    void Next() {
        AActor * a;
        while ( i < Actors.Size() ) {
            a = Actors[i++];
            if ( a->IsPendingKill() ) {
                continue;
            }
            if ( &a->FinalClassMeta() == &T::ClassMeta() ) {
                Actor = static_cast< T * >( a );
                return;
            }
        }
        Actor = nullptr;
    }

private:
    TPodArray< AActor * > const & Actors;
    T * Actor;
    int i;
};

/*

TActorIterator2

Iterate world actors

Example:

TActorIterator2< FMyActor > it( this );
for ( FMyActor * actor = it.First() ; actor ; actor = it.Next() ) {
    ...
}

*/
template< typename T >
struct TActorIterator2 {
    explicit TActorIterator2( AWorld * _World )
        : Actors( _World->GetActors() ), i( 0 )
    {
    }

    T * First() {
        i = 0;
        return Next();
    }

    T * Next() {
        AActor * a;
        while ( i < Actors.Size() ) {
            a = Actors[i++];
            if ( a->IsPendingKill() ) {
                continue;
            }
            if ( &a->FinalClassMeta() == &T::ClassMeta() ) {
                return static_cast< T * >( a );
            }
        }
        return nullptr;
    }

private:
    TPodArray< AActor * > const & Actors;
    int i;
};

/*

TComponentIterator

Iterate actor components

Example:
for ( TComponentIterator< FMyComponent > it( actor ) ; it ; ++it ) {
    FMyComponent * component = *it;
    ...
}

*/
template< typename T >
struct TComponentIterator {
    explicit TComponentIterator( AActor * _Actor )
        : Components( _Actor->GetComponents() ), i(0)
    {
        Next();
    }

    operator bool() const {
        return Component != nullptr;
    }

    T * operator++() {
        Next();
        return Component;
    }

    T * operator++(int) {
        T * a = Component;
        Next();
        return a;
    }

    T * operator*() const {
        return Component;
    }

    T * operator->() const {
        return Component;
    }

    void Next() {
        AActorComponent * a;
        while ( i < Components.Size() ) {
            a = Components[i++];
            if ( a->IsPendingKill() ) {
                continue;
            }
            if ( &a->FinalClassMeta() == &T::ClassMeta() ) {
                Component = static_cast< T * >( a );
                return;
            }
        }
        Component = nullptr;
    }

private:
    TPodArray< AActorComponent * > const & Components;
    T * Component;
    int i;
};

/*

TComponentIterator2

Iterate actor components

Example:

TComponentIterator2< FMyComponent > it( this );
for ( FMyComponent * component = it.First() ; component ; component = it.Next() ) {
    ...
}

*/
template< typename T >
struct TComponentIterator2 {
    explicit TComponentIterator2( AActor * _Actor )
        : Components( _Actor->GetComponents() ), i( 0 )
    {
    }

    T * First() {
        i = 0;
        return Next();
    }

    T * Next() {
        AActorComponent * a;
        while ( i < Components.Size() ) {
            a = Components[i++];
            if ( a->IsPendingKill() ) {
                continue;
            }
            if ( &a->FinalClassMeta() == &T::ClassMeta() ) {
                return static_cast< T * >( a );
            }
        }
        return nullptr;
    }

private:
    TPodArray< AActorComponent * > const & Components;
    int i;
};
