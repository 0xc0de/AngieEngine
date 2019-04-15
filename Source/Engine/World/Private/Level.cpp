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


#include <Engine/World/Public/Level.h>
#include <Engine/World/Public/Actor.h>
#include <Engine/World/Public/Texture.h>
#include <Engine/Core/Public/BV/BvIntersect.h>

AN_CLASS_META_NO_ATTRIBS( FLevel )
AN_CLASS_META_NO_ATTRIBS( FLevelArea )
AN_CLASS_META_NO_ATTRIBS( FLevelPortal )

// TODO: Octree, KD-tree, BSP-tree, AABB-tree/sBVH-tree or something like that

FLevel::FLevel() {
    LevelBounds.Clear();

    OutdoorArea = NewObject< FLevelArea >();
    OutdoorArea->Extents = Float3( CONVEX_HULL_MAX_BOUNDS * 2 );
    OutdoorArea->ParentLevel = this;
    OutdoorArea->Bounds.Mins = -OutdoorArea->Extents * 0.5f;
    OutdoorArea->Bounds.Maxs = OutdoorArea->Extents * 0.5f;
}

FLevel::~FLevel() {
    ClearLightmaps();

    DeallocateBufferData( LightData );

    DestroyActors();

    DestroyPortalTree();

//    BSP.Planes.Free();
//    BSP.Nodes.Free();
//    BSP.Leafs.Free();
//    BSP.Surfaces.Free();
//    BSP.Marksurfaces.Free();
//    BSP.Vertices.Free();
//    BSP.Indices.Free();
}

void FLevel::SetLightData( const byte * _Data, int _Size ) {
    DeallocateBufferData( LightData );
    LightData = (byte *)AllocateBufferData( _Size );
    memcpy( LightData, _Data, _Size );
}

void FLevel::ClearLightmaps() {
    for ( FTexture * lightmap : Lightmaps ) {
        lightmap->RemoveRef();
    }
    Lightmaps.Free();
}

void FLevel::DestroyActors() {
    for ( FActor * actor : Actors ) {
        actor->Destroy();
    }
}

static int DrawSurfMarker = 0;

FBinarySpaceData::FBinarySpaceData() {
    Visdata = nullptr;

    NumVisSurfs = 0;
    ViewLeafCluster = -1;
}

FBinarySpaceData::~FBinarySpaceData() {
    DeallocateBufferData( Visdata );
}

int FBinarySpaceData::FindLeaf( const Float3 & _Position ) {
    FBinarySpaceNode * node;
    float d;
    FBinarySpacePlane * plane;
    int nodeIndex;

    if ( !Nodes.Length() ) {
        GLogger.Printf( "FQuakeBSP::FindLeaf: no nodes\n" );
        return -1;
    }

    node = Nodes.ToPtr();
    while ( 1 ) {
        plane = node->Plane;
        if ( plane->Type < 3 ) {
            d = _Position[ plane->Type ] * plane->Normal[ plane->Type ] + plane->D;
        } else {
            d = FMath::Dot( _Position, plane->Normal ) + plane->D;
        }

        nodeIndex = node->ChildrenIdx[ ( d <= 0 ) ];
        if ( nodeIndex == 0 ) {
            // solid
            return -1;
        }

        if ( nodeIndex < 0 ) {
            return -1 - nodeIndex;
        }

        node = Nodes.ToPtr() + nodeIndex;
    }
    return -1;
}

#define MAX_MAP_LEAFS 0x20000

class FEmptyVisData {
public:
    FEmptyVisData();

    byte Data[ MAX_MAP_LEAFS / 8 ];
};

FEmptyVisData::FEmptyVisData() {
    memset( Data, 0xff, sizeof( Data ) );
}

static FEmptyVisData EmptyVis;

byte const * FBinarySpaceData::DecompressVisdata( byte const * _Data ) {
    static byte Decompressed[ MAX_MAP_LEAFS / 8 ];
    int c;
    byte *out;
    int row;

    row = ( Leafs.Length() + 7 ) >> 3;
    out = Decompressed;

    if ( !_Data ) { // no vis info, so make all visible
        while ( row ) {
            *out++ = 0xff;
            row--;
        }
        return Decompressed;
    }

    do {
        if ( *_Data ) {
            *out++ = *_Data++;
            continue;
        }

        c = _Data[ 1 ];
        _Data += 2;
        while ( c ) {
            *out++ = 0;
            c--;
        }
    } while ( out - Decompressed < row );

    return Decompressed;
}

byte const * FBinarySpaceData::LeafPVS( FBinarySpaceLeaf const * _Leaf ) {
    if ( bCompressedVisData ) {
        if (_Leaf == Leafs.ToPtr() ) {
            return EmptyVis.Data;
        }
        return DecompressVisdata( _Leaf->Visdata );
    } else {
        if ( !_Leaf->Visdata ) {
            return EmptyVis.Data;
        }
        return _Leaf->Visdata;
    }
}

int FBinarySpaceData::MarkLeafs( int _ViewLeaf ) {
    //ViewLeaf = _ViewLeaf;

    if ( _ViewLeaf < 0 ) {
        return VisFrameCount;
    }

    FBinarySpaceLeaf * viewLeaf = &Leafs[ _ViewLeaf ];

    if ( ViewLeafCluster == viewLeaf->Cluster ) {
        return VisFrameCount;
    }

    VisFrameCount++;
    ViewLeafCluster = viewLeaf->Cluster;

    byte const * vis = LeafPVS( viewLeaf );

    int numLeafs = Leafs.Length();

    int cluster;
    for ( int i = 0 ; i < numLeafs ; i++ ) {
        FBinarySpaceLeaf * Leaf = &Leafs[ i ];

        cluster = Leaf->Cluster;
        if ( cluster < 0 || cluster >= NumVisClusters ) {
            continue;
        }

        if ( !( vis[ cluster >> 3 ] & ( 1 << ( cluster & 7 ) ) ) ) {
            continue;
        }

        // TODO: check for door connection here

        FBinarySpaceNode * parent = ( FBinarySpaceNode* )Leaf;
        do {
            if ( parent->VisFrame == VisFrameCount ) {
                break;
            }
            parent->VisFrame = VisFrameCount;
            parent = parent->Parent;
        } while ( parent );
    }

    return VisFrameCount;
}

static constexpr int CullIndices[ 8 ][ 6 ] = {
        { 0, 4, 5, 3, 1, 2 },
        { 3, 4, 5, 0, 1, 2 },
        { 0, 1, 5, 3, 4, 2 },
        { 3, 1, 5, 0, 4, 2 },
        { 0, 4, 2, 3, 1, 5 },
        { 3, 4, 2, 0, 1, 5 },
        { 0, 1, 2, 3, 4, 5 },
        { 3, 1, 2, 0, 4, 5 }
};

static bool CullNode( FFrustum const & Frustum, BvAxisAlignedBox const & _Bounds, int & _CullBits ) {
    Float3 p;

    Float const * pBounds = _Bounds.ToPtr();
    int const * pIndices;
    if ( _CullBits & 1 ) {
        pIndices = CullIndices[ Frustum[0].CachedSignBits ];

        p[ 0 ] = pBounds[ pIndices[ 0 ] ];
        p[ 1 ] = pBounds[ pIndices[ 1 ] ];
        p[ 2 ] = pBounds[ pIndices[ 2 ] ];

        if ( FMath::Dot( p, Frustum[ 0 ].Normal ) <= -Frustum[ 0 ].D ) {
            return true;
        }

        p[ 0 ] = pBounds[ pIndices[ 3 ] ];
        p[ 1 ] = pBounds[ pIndices[ 4 ] ];
        p[ 2 ] = pBounds[ pIndices[ 5 ] ];

        if ( FMath::Dot( p, Frustum[ 0 ].Normal ) >= -Frustum[ 0 ].D ) {
            _CullBits &= ~1;
        }
    }

    if ( _CullBits & 2 ) {
        pIndices = CullIndices[ Frustum[1].CachedSignBits ];

        p[ 0 ] = pBounds[ pIndices[ 0 ] ];
        p[ 1 ] = pBounds[ pIndices[ 1 ] ];
        p[ 2 ] = pBounds[ pIndices[ 2 ] ];

        if ( FMath::Dot( p, Frustum[ 1 ].Normal ) <= -Frustum[ 1 ].D ) {
            return true;
        }

        p[ 0 ] = pBounds[ pIndices[ 3 ] ];
        p[ 1 ] = pBounds[ pIndices[ 4 ] ];
        p[ 2 ] = pBounds[ pIndices[ 5 ] ];

        if ( FMath::Dot( p, Frustum[ 1 ].Normal ) >= -Frustum[ 1 ].D ) {
            _CullBits &= ~2;
        }
    }

    if ( _CullBits & 4 ) {
        pIndices = CullIndices[ Frustum[2].CachedSignBits ];

        p[ 0 ] = pBounds[ pIndices[ 0 ] ];
        p[ 1 ] = pBounds[ pIndices[ 1 ] ];
        p[ 2 ] = pBounds[ pIndices[ 2 ] ];

        if ( FMath::Dot( p, Frustum[ 2 ].Normal ) <= -Frustum[ 2 ].D ) {
            return true;
        }

        p[ 0 ] = pBounds[ pIndices[ 3 ] ];
        p[ 1 ] = pBounds[ pIndices[ 4 ] ];
        p[ 2 ] = pBounds[ pIndices[ 5 ] ];

        if ( FMath::Dot( p, Frustum[ 2 ].Normal ) >= -Frustum[ 2 ].D ) {
            _CullBits &= ~4;
        }
    }

    if ( _CullBits & 8 ) {
        pIndices = CullIndices[ Frustum[3].CachedSignBits ];

        p[ 0 ] = pBounds[ pIndices[ 0 ] ];
        p[ 1 ] = pBounds[ pIndices[ 1 ] ];
        p[ 2 ] = pBounds[ pIndices[ 2 ] ];

        if ( FMath::Dot( p, Frustum[ 3 ].Normal ) <= -Frustum[ 3 ].D ) {
            return true;
        }

        p[ 0 ] = pBounds[ pIndices[ 3 ] ];
        p[ 1 ] = pBounds[ pIndices[ 4 ] ];
        p[ 2 ] = pBounds[ pIndices[ 5 ] ];

        if ( FMath::Dot( p, Frustum[ 3 ].Normal ) >= -Frustum[ 3 ].D ) {
            _CullBits &= ~8;
        }
    }

    return false;
}

void FBinarySpaceData::PerformVSD( Float3 const & _ViewOrigin, FFrustum const & _Frustum, bool _SortLightmapGroup ) {

    ++DrawSurfMarker;

    ViewOrigin = _ViewOrigin;
    Frustum = &_Frustum;

    VisSurfs.ResizeInvalidate( Surfaces.Length() );

    NumVisSurfs = 0;

    int Leaf = FindLeaf( ViewOrigin );

    VisFrame = MarkLeafs( Leaf );

    Traverse_r( 0, 0xf );

    struct FSortFunction {
        bool operator() ( FSurfaceDef const * _A, FSurfaceDef * _B ) {
            return ( _A->LightmapGroup < _B->LightmapGroup );
        }
    } SortFunction;

    if ( _SortLightmapGroup && NumVisSurfs > 0 ) {
        StdSort( &VisSurfs[0], &VisSurfs[NumVisSurfs], SortFunction );
    }
}

void FBinarySpaceData::Traverse_r( int _NodeIndex, int _CullBits ) {
    int * mark;
    FBinarySpaceLeaf const * pleaf;
    int Count;
    FSurfaceDef * Surf;
    FNodeBase const * Node;

    while ( 1 ) {
        if ( _NodeIndex < 0 ) {
            Node = &Leafs[ -1 - _NodeIndex ]; //SpatialTree->Nodes.ToPtr() + ( -1 - _NodeIndex );
        } else {
            Node = Nodes.ToPtr() + _NodeIndex;
        }

        if ( Node->VisFrame != VisFrame )
            return;

        if ( CullNode( *Frustum, Node->Bounds, _CullBits ) ) {
            //TotalCulled++;
            return;
        }

        //if ( !Frustum.CheckAABB2( _Node->Bounds ) ) {
        //    CullMiss++;
        //}

        //if ( _Node->Contents < 0 ) {
        //	break; // leaf
        //}

        if ( _NodeIndex < 0 ) {
            // leaf
            break;
        }

        if ( _NodeIndex == 0 ) {
            // solid
            // FIXME: what to do?
            //return;
        }

        Traverse_r( ((FBinarySpaceNode *)Node)->ChildrenIdx[0], _CullBits );

        _NodeIndex = ((FBinarySpaceNode *)Node)->ChildrenIdx[1];
    }

    pleaf = ( FBinarySpaceLeaf const * )Node;

    mark = &Marksurfaces[ pleaf->FirstSurface ];

    Count = pleaf->NumSurfaces;
    while ( Count-- ) {

        Surf = &Surfaces[ *mark ];

//        Renderable = Surf;

        if ( Surf->Marker != DrawSurfMarker ) {
            Surf->Marker = DrawSurfMarker;

//        if ( Renderable->VisFrame[ RenderTargetStack ] != Proxy->DrawSerialIndex ) {
//            Renderable->VisFrame[ RenderTargetStack ] = Proxy->DrawSerialIndex;

            bool FaceCull = false;
            //if ( r_faceCull.GetBool() ) {
                const bool TwoSided = false;
                const bool FrontSided = true;
                const float EPS = 0.25f;
                switch ( Surf->Type ) {
                    case SURF_PLANAR:
                    {
                        if ( !TwoSided ) {
                            const PlaneF & Plane = Surf->Plane;
                            float d = FMath::Dot( ViewOrigin, Plane.Normal );

                            if ( FrontSided ) {
                                if ( d < -Plane.D - EPS ) {
                                    FaceCull = true;
                                }
                            } else {
                                if ( d > -Plane.D + EPS ) {
                                    FaceCull = true;
                                }
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            //}

            if ( !FaceCull ) {
                VisSurfs[NumVisSurfs++] = Surf;
            }
        } else {
            //GLogger.Printf( "Skip\n" );
        }
        mark++;
    }
}

void FLevel::OnAddLevelToWorld() {
    RemoveSurfaces();
    AddSurfaces();
}

void FLevel::OnRemoveLevelFromWorld() {
    RemoveSurfaces();
}

FLevelArea * FLevel::CreateArea( Float3 const & _Position, Float3 const & _Extents, Float3 const & _ReferencePoint ) {
    FLevelArea * area = NewObject< FLevelArea >();
    area->AddRef();
    area->Position = _Position;
    area->Extents = _Extents;
    area->ReferencePoint = _ReferencePoint;
    area->ParentLevel = this;
    Areas.Append( area );
    return area;
}

FLevelPortal * FLevel::CreatePortal( Float3 const * _HullPoints, int _NumHullPoints, FLevelArea * _Area1, FLevelArea * _Area2 ) {
    if ( _Area1 == _Area2 ) {
        return nullptr;
    }

    FLevelPortal * portal = NewObject< FLevelPortal >();
    portal->AddRef();
    portal->Hull = FConvexHull::CreateFromPoints( _HullPoints, _NumHullPoints );
    portal->Plane = portal->Hull->CalcPlane();
    portal->Area1 = _Area1 ? _Area1 : OutdoorArea;
    portal->Area2 = _Area2 ? _Area2 : OutdoorArea;
    portal->ParentLevel = this;
    Portals.Append( portal );
    return portal;
}

void FLevel::DestroyPortalTree() {
    PurgePortals();

    for ( FLevelArea * area : Areas ) {
        area->RemoveRef();
    }

    Areas.Clear();

    for ( FLevelPortal * portal : Portals ) {
        portal->RemoveRef();
    }

    Portals.Clear();

    LevelBounds.Clear();
}

#include <Engine/World/Public/World.h>
#include <Engine/World/Public/MeshComponent.h>
void FLevel::AddSurfaces() {
    FWorld * world = GetOwnerWorld();

    for ( FMeshComponent * mesh = world->GetMeshList() ; mesh ; mesh = mesh->GetNextMesh() ) {

        // TODO: if ( !mesh->IsMarkedDirtyArea() )
        AddSurfaceAreas( mesh );
    }
}

void FLevel::RemoveSurfaces() {
    for ( FLevelArea * area : Areas ) {
        while ( !area->Movables.IsEmpty() ) {
            RemoveSurfaceAreas( area->Movables[0] );
        }
    }

    while ( !OutdoorArea->Movables.IsEmpty() ) {
        RemoveSurfaceAreas( OutdoorArea->Movables[0] );
    }
}

void FLevel::PurgePortals() {
    RemoveSurfaces();

    for ( FAreaPortal & areaPortal : AreaPortals ) {
        FConvexHull::Destroy( areaPortal.Hull );
    }

    AreaPortals.Clear();
}

void FLevel::BuildPortals() {

    PurgePortals();

    LevelBounds.Clear();

    Float3 halfExtents;
    for ( FLevelArea * area : Areas ) {
        // Update area bounds
        halfExtents = area->Extents * 0.5f;
        for ( int i = 0 ; i < 3 ; i++ ) {
            area->Bounds.Mins[i] = area->Position[i] - halfExtents[i];
            area->Bounds.Maxs[i] = area->Position[i] + halfExtents[i];
        }

        LevelBounds.AddAABB( area->Bounds );

        // Clear area portals
        area->PortalList = NULL;
    }

    AreaPortals.ResizeInvalidate( Portals.Length() << 1 );

    int areaPortalId = 0;

    for ( FLevelPortal * portal : Portals ) {
        FLevelArea * a1 = portal->Area1;
        FLevelArea * a2 = portal->Area2;

        if ( a1 == OutdoorArea ) {
            std::swap( a1, a2 );
        }

        // Check area position relative to portal plane
        EPlaneSide offset = portal->Plane.SideOffset( a1->ReferencePoint, 0.0f );

        // If area position is on back side of plane, then reverse hull vertices and plane
        int id = offset == EPlaneSide::Back ? 1 : 0;

        FAreaPortal * areaPortal;

        areaPortal = &AreaPortals[ areaPortalId++ ];
        portal->Portals[id] = areaPortal;
        areaPortal->ToArea = a2;
        if ( id & 1 ) {
            areaPortal->Hull = portal->Hull->Reversed();
            areaPortal->Plane = -portal->Plane;
        } else {
            areaPortal->Hull = portal->Hull->Duplicate();
            areaPortal->Plane = portal->Plane;
        }
        areaPortal->Next = a1->PortalList;
        areaPortal->Owner = portal;
        a1->PortalList = areaPortal;

        id = ( id + 1 ) & 1;

        areaPortal = &AreaPortals[ areaPortalId++ ];
        portal->Portals[id] = areaPortal;
        areaPortal->ToArea = a1;
        if ( id & 1 ) {
            areaPortal->Hull = portal->Hull->Reversed();
            areaPortal->Plane = -portal->Plane;
        } else {
            areaPortal->Hull = portal->Hull->Duplicate();
            areaPortal->Plane = portal->Plane;
        }
        areaPortal->Next = a2->PortalList;
        areaPortal->Owner = portal;
        a2->PortalList = areaPortal;
    }

    AddSurfaces();
}

void FLevel::AddSurfaceToArea( int _AreaNum, FDrawSurf * _Surf ) {
    FLevelArea * area = _AreaNum >= 0 ? Areas[_AreaNum] : OutdoorArea;

    area->Movables.Append( _Surf );
    FAreaLink & areaLink = _Surf->InArea.Append();
    areaLink.AreaNum = _AreaNum;
    areaLink.Index = area->Movables.Length() - 1;
    areaLink.Level = this;
}

void FLevel::AddSurfaceAreas( FDrawSurf * _Surf ) {
    BvAxisAlignedBox const & bounds = _Surf->GetWorldBounds();
    int numAreas = Areas.Length();
    FLevelArea * area;

    if ( _Surf->IsOutdoorSurface() ) {
        // add to outdoor
        AddSurfaceToArea( -1, _Surf );
        return;
    }

    bool bHaveIntersection = false;
    if ( FMath::Intersects( LevelBounds, bounds ) ) {
        // TODO: optimize it!
        for ( int i = 0 ; i < numAreas ; i++ ) {
            area = Areas[i];

            if ( FMath::Intersects( area->Bounds, bounds ) ) {
                AddSurfaceToArea( i, _Surf );

                bHaveIntersection = true;
            }
        }
    }

    if ( !bHaveIntersection ) {
        AddSurfaceToArea( -1, _Surf );
    }
}

void FLevel::RemoveSurfaceAreas( FDrawSurf * _Surf ) {
    FLevelArea * area;

    // Remove renderables from any areas
    for ( int i = 0 ; i < _Surf->InArea.Length() ; ) {
        FAreaLink & InArea = _Surf->InArea[ i ];

        if ( InArea.Level != this ) {
            i++;
            continue;
        }

        AN_Assert( InArea.AreaNum < InArea.Level->Areas.Length() );
        area = InArea.AreaNum >= 0 ? InArea.Level->Areas[ InArea.AreaNum ] : OutdoorArea;

        AN_Assert( area->Movables[ InArea.Index ] == _Surf );

        // Swap with last array element
        area->Movables.RemoveSwap( InArea.Index );

        // Update swapped movable index
        if ( InArea.Index < area->Movables.Length() ) {
            FDrawSurf * surf = area->Movables[ InArea.Index ];
            for ( int j = 0 ; j < surf->InArea.Length() ; j++ ) {
                if ( surf->InArea[ j ].Level == this && surf->InArea[ j ].AreaNum == InArea.AreaNum ) {
                    surf->InArea[ j ].Index = InArea.Index;

                    AN_Assert( area->Movables[ surf->InArea[ j ].Index ] == surf );
                    break;
                }
            }
        }

        _Surf->InArea.RemoveSwap(i);
    }
}

void FLevel::DrawDebug( FDebugDraw * _DebugDraw ) {
    _DebugDraw->SetDepthTest( false );

    _DebugDraw->SetColor(0,1,0,1);
    for ( FLevelArea * area : Areas ) {
        _DebugDraw->DrawAABB( area->Bounds );
    }

    _DebugDraw->SetColor(1,0,0,1);
    for ( FLevelPortal * portal : Portals ) {
        _DebugDraw->DrawLine( portal->Hull->Points, portal->Hull->NumPoints, true );
    }

    //_DebugDraw->DrawAABB( LevelBounds );
}

int FLevel::FindArea( Float3 const & _Position ) {
    // TODO: ... binary tree

    if ( Areas.Length() == 0 ) {
        return -1;
    }

    for ( int i = 0 ; i < Areas.Length() ; i++ ) {
        if (    _Position.X >= Areas[i]->Bounds.Mins.X
             && _Position.Y >= Areas[i]->Bounds.Mins.Y
             && _Position.Z >= Areas[i]->Bounds.Mins.Z
             && _Position.X <  Areas[i]->Bounds.Maxs.X
             && _Position.Y <  Areas[i]->Bounds.Maxs.Y
             && _Position.Z <  Areas[i]->Bounds.Maxs.Z ) {
            return i;
        }
    }

    return -1;
}
