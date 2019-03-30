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
#include <Engine/World/Public/Actor.h>
#include <Engine/World/Public/ActorComponent.h>
#include <Engine/World/Public/SceneComponent.h>
#include <Engine/World/Public/StaticMeshComponent.h>
#include <Engine/World/Public/GameMaster.h>
#include <Engine/World/Public/Timer.h>
#include <Engine/Core/Public/Logger.h>
#include <Engine/Core/Public/IntrusiveLinkedListMacro.h>

AN_BEGIN_CLASS_META( FWorld )
//AN_ATTRIBUTE_( bTickEvenWhenPaused, AF_DEFAULT )
AN_END_CLASS_META()

void FActorSpawnParameters::SetTemplate( FActor const * _Template ) {
    AN_Assert( &_Template->FinalClassMeta() == ActorTypeClassMeta );
    Template = _Template;
}

FWorld::FWorld() {
    PersistentLevel = NewObject< FLevel >();
}

void FWorld::Destroy() {
    if ( bPendingKill ) {
        return;
    }

    // Mark world to remove them from the game
    bPendingKill = true;
    NextPendingKillWorld = GGameMaster.PendingKillWorlds;
    GGameMaster.PendingKillWorlds = this;

    DestroyActors();
    KickoffPendingKillObjects();

    EndPlay();
}

void FWorld::DestroyActors() {
    for ( FActor * actor : Actors ) {
        actor->Destroy();
    }
}

FActor * FWorld::SpawnActor( FActorSpawnParameters const & _SpawnParameters ) {

    GLogger.Printf( "==== Spawn Actor ====\n" );

    FClassMeta const * classMeta = _SpawnParameters.ActorClassMeta();

    if ( !classMeta ) {
        GLogger.Printf( "FWorld::SpawnActor: invalid actor class\n" );
        return nullptr;
    }

    if ( classMeta->Factory() != &FActor::Factory() ) {
        GLogger.Printf( "FWorld::SpawnActor: not an actor class\n" );
        return nullptr;
    }

    FActor const * templateActor = _SpawnParameters.GetTemplate();

    if ( templateActor && classMeta != &templateActor->FinalClassMeta() ) {
        GLogger.Printf( "FWorld::SpawnActor: FActorSpawnParameters::Template class doesn't match meta data\n" );
        return nullptr;
    }

    FActor * actor = static_cast< FActor * >( classMeta->CreateInstance() );
    actor->AddRef();
    actor->bDuringConstruction = false;

    // Add actor to world array of actors
    Actors.Append( actor );
    actor->IndexInWorldArrayOfActors = Actors.Length() - 1;
    actor->ParentWorld = this;

    actor->Level = _SpawnParameters.Level ? _SpawnParameters.Level : PersistentLevel;
    actor->Level->Actors.Append( actor );
    actor->IndexInLevelArrayOfActors = actor->Level->Actors.Length() - 1;

    // Update actor name to make it unique
    actor->SetName( actor->Name );

    if ( templateActor ) {
        actor->Clone( templateActor );
    }

    actor->PostSpawnInitialize( _SpawnParameters.SpawnTransform );
    //actor->PostActorCreated();
    //actor->ExecuteContruction( _SpawnParameters.SpawnTransform );
    actor->PostActorConstruction();

    BroadcastActorSpawned( actor );
    actor->BeginPlayComponents();
    actor->BeginPlay();

    GLogger.Printf( "=====================\n" );
    return actor;
}

FActor * FWorld::LoadActor( FDocument const & _Document, int _FieldsHead, FLevel * _Level ) {

    GLogger.Printf( "==== Load Actor ====\n" );

    FDocumentField * classNameField = _Document.FindField( _FieldsHead, "ClassName" );
    if ( !classNameField ) {
        GLogger.Printf( "FWorld::LoadActor: invalid actor class\n" );
        return nullptr;
    }

    FDocumentValue * classNameValue = &_Document.Values[ classNameField->ValuesHead ];

    FClassMeta const * classMeta = FActor::Factory().LookupClass( classNameValue->Token.ToString().ToConstChar() );
    if ( !classMeta ) {
        GLogger.Printf( "FWorld::LoadActor: invalid actor class \"%s\"\n", classNameValue->Token.ToString().ToConstChar() );
        return nullptr;
    }

    FActor * actor = static_cast< FActor * >( classMeta->CreateInstance() );
    actor->AddRef();
    actor->bDuringConstruction = false;

    // Add actor to world array of actors
    Actors.Append( actor );
    actor->IndexInWorldArrayOfActors = Actors.Length() - 1;
    actor->ParentWorld = this;

    actor->Level = _Level ? _Level : PersistentLevel;
    actor->Level->Actors.Append( actor );
    actor->IndexInLevelArrayOfActors = actor->Level->Actors.Length() - 1;

    // Update actor name to make it unique
    actor->SetName( actor->Name );

    // Load actor attributes
    actor->LoadAttributes( _Document, _FieldsHead );

    // Load components
    FDocumentField * componentsArray = _Document.FindField( _FieldsHead, "Components" );
    for ( int i = componentsArray->ValuesHead ; i != -1 ; i = _Document.Values[ i ].Next ) {
        FDocumentValue const * componentObject = &_Document.Values[ i ];
        if ( componentObject->Type != FDocumentValue::T_Object ) {
            continue;
        }
        actor->LoadComponent( _Document, componentObject->FieldsHead );
    }

    // Load root component
    FDocumentField * rootField = _Document.FindField( _FieldsHead, "Root" );
    if ( rootField ) {
        FDocumentValue * rootValue = &_Document.Values[ rootField->ValuesHead ];
        FSceneComponent * root = dynamic_cast< FSceneComponent * >( actor->FindComponent( rootValue->Token.ToString().ToConstChar() ) );
        if ( root ) {
            actor->RootComponent = root;
        }
    }

    //actor->PostActorCreated();
    //actor->ExecuteContruction( _SpawnParameters.SpawnTransform );
    actor->PostActorConstruction();
    BroadcastActorSpawned( actor );
    actor->BeginPlayComponents();
    actor->BeginPlay();

    GLogger.Printf( "=====================\n" );
    return actor;
}

FString FWorld::GenerateActorUniqueName( const char * _Name ) {
    if ( !FindActor( _Name ) ) {
        return _Name;
    }
    int uniqueNumber = 0;
    FString uniqueName;
    do {
        uniqueName.Resize( 0 );
        uniqueName.Concat( _Name );
        uniqueName.Concat( Int( ++uniqueNumber ).ToConstChar() );
    } while ( FindActor( uniqueName.ToConstChar() ) != nullptr );
    return uniqueName;
}

FActor * FWorld::FindActor( const char * _UniqueName ) {
    for ( FActor * actor : Actors ) {
        if ( !actor->GetName().Icmp( _UniqueName ) ) {
            return actor;
        }
    }
    return nullptr;
}

void FWorld::BroadcastActorSpawned( FActor * _SpawnedActor ) {
    for ( FActor * actor : Actors ) {
        if ( actor != _SpawnedActor ) {
            actor->OnActorSpawned( _SpawnedActor );
        }
    }
}

void FWorld::BeginPlay() {
    GLogger.Printf( "FWorld::BeginPlay()\n" );
}

void FWorld::EndPlay() {
    GLogger.Printf( "FWorld::EndPlay()\n" );
}

void FWorld::Tick( float _TimeStep ) {
    //if ( GGameMaster.IsGamePaused() && !bTickEvenWhenPaused ) {
    //    return;
    //}

    // Tick all timers
    for ( FTimer * timer = TimerList ; timer ; timer = timer->Next ) {
        timer->Tick( _TimeStep );
    }

    // Tick actors
    for ( FActor * actor : Actors ) {
        if ( actor->IsPendingKill() ) {
            continue;
        }

        if ( actor->bCanEverTick ) {

            if ( GGameMaster.IsGamePaused() && !actor->bTickEvenWhenPaused ) {
                continue;
            }

            actor->TickComponents( _TimeStep );
            actor->Tick( _TimeStep );

            actor->LifeTime += _TimeStep;
            if ( actor->LifeSpan > 0.0f && actor->LifeTime > actor->LifeSpan ) {
                actor->Destroy();
            }
        }
    }

    KickoffPendingKillObjects();

    WorldLocalTime += _TimeStep;
    if ( !GGameMaster.IsGamePaused() ) {
        WorldPlayTime += _TimeStep;
    }
}

void FWorld::KickoffPendingKillObjects() {
    while ( PendingKillComponents ) {
        FActorComponent * component = PendingKillComponents;
        FActorComponent * nextComponent;

        PendingKillComponents = nullptr;

        while ( component ) {
            nextComponent = component->NextPendingKillComponent;

            // FIXME: Call component->EndPlay here?

            // Remove component from actor array of components
            FActor * parent = component->ParentActor;
            if ( parent /*&& !parent->IsPendingKill()*/ ) {
                parent->Components[ component->ComponentIndex ] = parent->Components[ parent->Components.Length() - 1 ];
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
        FActor * actor = PendingKillActors;
        FActor * nextActor;

        PendingKillActors = nullptr;

        while ( actor ) {
            nextActor = actor->NextPendingKillActor;

            // FIXME: Call actor->EndPlay here?

            // Remove actor from world array of actors
            Actors[ actor->IndexInWorldArrayOfActors ] = Actors[ Actors.Length() - 1 ];
            Actors[ actor->IndexInWorldArrayOfActors ]->IndexInWorldArrayOfActors = actor->IndexInWorldArrayOfActors;
            Actors.RemoveLast();
            actor->IndexInWorldArrayOfActors = -1;
            actor->ParentWorld = nullptr;

            // Remove actor from level array of actors
            FLevel * level = actor->Level;
            level->Actors[ actor->IndexInLevelArrayOfActors ] = level->Actors[ level->Actors.Length() - 1 ];
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
int FWorld::Serialize( FDocument & _Doc ) {
    int object = Super::Serialize( _Doc );

    if ( !Actors.IsEmpty() ) {
        int actors = _Doc.AddArray( object, "Actors" );

//        std::unordered_set< std::string > precacheStrs;

        for ( FActor * actor : Actors ) {
            if ( actor->IsPendingKill() ) {
                continue;
            }
            int actorObject = actor->Serialize( _Doc );
            _Doc.AddValueToField( actors, actorObject );

//            FClassMeta const & classMeta = actor->FinalClassMeta();

//            for ( FPrecacheMeta const * precache = classMeta.GetPrecacheList() ; precache ; precache = precache->Next() ) {
//                precacheStrs.insert( precache->GetResourcePath() );
//            }
//            // TODO: precache all objects, not only actors
        }

//        if ( !precacheStrs.empty() ) {
//            int precache = _Doc.AddArray( object, "Precache" );
//            for ( auto it : precacheStrs ) {
//                const std::string & s = it;

//                _Doc.AddValueToField( precache, _Doc.CreateStringValue( _Doc.ProxyBuffer.NewString( s.c_str() ).ToConstChar() ) );
//            }
//        }
    }

    return object;
}

void FWorld::RegisterStaticMesh( FStaticMeshComponent * _StaticMesh ) {
    if ( IntrusiveIsInList( _StaticMesh, Next, Prev, StaticMeshList, StaticMeshListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    IntrusiveAddToList( _StaticMesh, Next, Prev, StaticMeshList, StaticMeshListTail );
}

void FWorld::UnregisterStaticMesh( FStaticMeshComponent * _StaticMesh ) {
    IntrusiveRemoveFromList( _StaticMesh, Next, Prev, StaticMeshList, StaticMeshListTail );
}

void FWorld::RegisterTimer( FTimer * _Timer ) {
    if ( IntrusiveIsInList( _Timer, Next, Prev, TimerList, TimerListTail ) ) {
        AN_Assert( 0 );
        return;
    }

    IntrusiveAddToList( _Timer, Next, Prev, TimerList, TimerListTail );
}

void FWorld::UnregisterTimer( FTimer * _Timer ) {
    IntrusiveRemoveFromList( _Timer, Next, Prev, TimerList, TimerListTail );
}

void FWorld::UpdateDrawSurfAreas( FDrawSurf * _Surf ) {
//    const BvAxisAlignedBox & Bounds = _Surf->GetWorldBounds();
//    int NumAreas = Areas.Length();
//    FSpatialAreaComponent * Area;

//    // Remove drawsurf from any areas
//    RemoveDrawSurfFromAreas( _Surf );

//    // TODO: optimize it!
//    for ( int i = 0 ; i < NumAreas ; i++ ) {
//        Area = Areas[i];

//        if ( FMath::Intersects( Area->Bounds, Bounds ) ) {
//            Area->Movables.Append( _Surf );
//            FAreaLink & InArea = _Surf->InArea.Append();
//            InArea.AreaNum = i;
//            InArea.Index = Area->Movables.Length() - 1;
//        }
//    }
}

void FWorld::RemoveDrawSurfFromAreas( FDrawSurf * _Surf ) {
//#ifdef AN_DEBUG
//    int NumAreas = Areas.Length();
//#endif
//    FSpatialAreaComponent * Area;

//    // Remove renderables from any areas
//    for ( int i = 0 ; i < _Surf->InArea.Length() ; i++ ) {
//        FAreaLink & InArea = _Surf->InArea[ i ];

//        AN_Assert( InArea.AreaNum < NumAreas );
//        Area = Areas[ InArea.AreaNum ];

//        AN_Assert( Area->Movables[ InArea.Index ] == _Surf );

//        // Swap with last array element
//        Area->Movables.RemoveSwap( InArea.Index );

//        // Update swapped movable index
//        if ( InArea.Index < Area->Movables.Length() ) {
//            FDrawSurf * Movable = Area->Movables[ InArea.Index ];
//            for ( int j = 0 ; j < Movable->InArea.Length() ; j++ ) {
//                if ( Movable->InArea[ j ].AreaNum == InArea.AreaNum ) {
//                    Movable->InArea[ j ].Index = InArea.Index;

//                    AN_Assert( Area->Movables[ Movable->InArea[ j ].Index ] == Movable );
//                    break;
//                }
//            }
//        }
//    }
//    _Surf->InArea.Clear();
}

void FWorld::GenerateDebugDrawGeometry( FDebugDraw * _DebugDraw ) {

    FirstDebugDrawCommand = _DebugDraw->CommandsCount();

    _DebugDraw->SplitCommands();

    _DebugDraw->SetDepthTest( true );

    for ( FStaticMeshComponent * component = StaticMeshList
          ; component ; component = component->NextWorldMesh() ) {

        _DebugDraw->DrawAABB( component->GetWorldBounds() );
    }

    //TPodArray< FMeshVertex > Vertices;
    //TPodArray< unsigned int > Indices;
    //BvAxisAlignedBox  Bounds;
    //FCylinderShape::CreateMesh( Vertices, Indices, Bounds, 10, 20, 1, 16 );

    
    _DebugDraw->SetColor( 0,1,0,0.1f);
    //_DebugDraw->DrawTriangleSoup( &Vertices.ToPtr()->Position, Vertices.Length(), sizeof(FMeshVertex), Indices.ToPtr(), Indices.Length() );

    _DebugDraw->SetColor( 1,1,0,0.1f);
    //_DebugDraw->DrawPoints( &Vertices.ToPtr()->Position, Vertices.Length(), sizeof(FMeshVertex) );
    //_DebugDraw->DrawCircleFilled( Float3(0), Float3(0,1,0), 10.0f );

    _DebugDraw->SetColor( 0,0,1,1);
    //_DebugDraw->DrawCylinder( Float3(0), Float3x3::Identity(), 10, 20 );

    //_DebugDraw->SetColor( 1,0,0,1);
    //_DebugDraw->DrawDottedLine( Float3(0,0,0), Float3(100,0,0), 1.0f );
    //_DebugDraw->SetColor( 0,1,0,1);
    //_DebugDraw->DrawDottedLine( Float3(0,0,0), Float3(0,100,0), 1.0f );
    //_DebugDraw->SetColor( 0,0,1,1);
    //_DebugDraw->DrawDottedLine( Float3(0,0,0), Float3(0,0,100), 1.0f );

    DebugDrawCommandCount = _DebugDraw->CommandsCount() - FirstDebugDrawCommand;

    //GLogger.Printf( "DebugDrawCommandCount %d\n", DebugDrawCommandCount );
}
