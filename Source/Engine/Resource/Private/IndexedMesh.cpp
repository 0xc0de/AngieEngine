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

#include <Engine/Resource/Public/IndexedMesh.h>
#include <Engine/Resource/Public/Asset.h>
#include <Engine/Resource/Public/ResourceManager.h>
#include <Engine/Resource/Public/GLTF.h>

#include <Engine/Runtime/Public/ScopedTimeCheck.h>

#include <Engine/Core/Public/Logger.h>
#include <Engine/Core/Public/IntrusiveLinkedListMacro.h>
#include <Engine/Core/Public/BV/BvIntersect.h>

AN_CLASS_META( AIndexedMesh )
AN_CLASS_META( AIndexedMeshSubpart )
AN_CLASS_META( ALightmapUV )
AN_CLASS_META( AVertexLight )
AN_CLASS_META( ATreeAABB )
AN_CLASS_META( ASocketDef )

ARuntimeVariable RVDrawIndexedMeshBVH( _CTS( "DrawIndexedMeshBVH" ), _CTS( "0" ), VAR_CHEAT );

///////////////////////////////////////////////////////////////////////////////////////////////////////

AIndexedMesh::AIndexedMesh() {
    VertexBufferGPU = GRenderBackend->CreateBuffer( this );
    IndexBufferGPU = GRenderBackend->CreateBuffer( this );
    WeightsBufferGPU = GRenderBackend->CreateBuffer( this );

    static TStaticResourceFinder< ASkeleton > SkeletonResource( _CTS( "/Default/Skeleton/Default" ) );
    Skeleton = SkeletonResource.GetObject();

    RaycastPrimitivesPerLeaf = 16;
}

AIndexedMesh::~AIndexedMesh() {
    GRenderBackend->DestroyBuffer( VertexBufferGPU );
    GRenderBackend->DestroyBuffer( IndexBufferGPU );
    GRenderBackend->DestroyBuffer( WeightsBufferGPU );

    Purge();
}

void AIndexedMesh::Initialize( int _NumVertices, int _NumIndices, int _NumSubparts, bool _SkinnedMesh, bool _DynamicStorage ) {
    Purge();

    bSkinnedMesh = _SkinnedMesh;
    bDynamicStorage = _DynamicStorage;
    bBoundingBoxDirty = true;
    BoundingBox.Clear();

    Vertices.ResizeInvalidate( _NumVertices );
    if ( bSkinnedMesh ) {
        Weights.ResizeInvalidate( _NumVertices );
    }
    Indices.ResizeInvalidate( _NumIndices );

    GRenderBackend->InitializeBuffer( VertexBufferGPU, Vertices.Size() * sizeof( SMeshVertex ), bDynamicStorage );
    GRenderBackend->InitializeBuffer( IndexBufferGPU, Indices.Size() * sizeof( unsigned int ), bDynamicStorage );

    if ( _SkinnedMesh ) {
        GRenderBackend->InitializeBuffer( WeightsBufferGPU, Weights.Size() * sizeof( SMeshVertexJoint ), bDynamicStorage );
    }

    //for ( ALightmapUV * channel : LightmapUVs ) {
    //    channel->OnInitialize( _NumVertices );
    //}

    //for ( AVertexLight * channel : VertexLightChannels ) {
    //    channel->OnInitialize( _NumVertices );
    //}

    if ( _NumSubparts <= 0 ) {
        _NumSubparts = 1;
    }

    Subparts.ResizeInvalidate( _NumSubparts );
    for ( int i = 0 ; i < _NumSubparts ; i++ ) {
        AIndexedMeshSubpart * subpart = NewObject< AIndexedMeshSubpart >();
        subpart->AddRef();
        subpart->OwnerMesh = this;
        Subparts[i] = subpart;
    }

    if ( _NumSubparts == 1 ) {
        AIndexedMeshSubpart * subpart = Subparts[0];
        subpart->BaseVertex = 0;
        subpart->FirstIndex = 0;
        subpart->VertexCount = Vertices.Size();
        subpart->IndexCount = Indices.Size();
    }
}

void AIndexedMesh::Purge() {
    for ( AIndexedMeshSubpart * subpart : Subparts ) {
        subpart->OwnerMesh = nullptr;
        subpart->RemoveRef();
    }

    Subparts.Clear();

    for ( ALightmapUV * channel : LightmapUVs ) {
        channel->OwnerMesh = nullptr;
        channel->IndexInArrayOfUVs = -1;
    }

    LightmapUVs.Clear();

    for ( AVertexLight * channel : VertexLightChannels ) {
        channel->OwnerMesh = nullptr;
        channel->IndexInArrayOfChannels = -1;
    }

    VertexLightChannels.Clear();

    for ( ASocketDef * socket : Sockets ) {
        socket->RemoveRef();
    }

    Sockets.Clear();

    Skin.JointIndices.Clear();
    Skin.OffsetMatrices.Clear();

    BodyComposition.Clear();
}

static AIndexedMeshSubpart * ReadIndexedMeshSubpart( IStreamBase & f ) {
    AString Name;
    int32_t BaseVertex;
    uint32_t FirstIndex;
    uint32_t VertexCount;
    uint32_t IndexCount;
    AString MaterialInstance;
    BvAxisAlignedBox BoundingBox;

    f.ReadString( Name );
    BaseVertex = f.ReadInt32();
    FirstIndex = f.ReadUInt32();
    VertexCount = f.ReadUInt32();
    IndexCount = f.ReadUInt32();
    f.ReadString( MaterialInstance );
    f.ReadObject( BoundingBox );

    AIndexedMeshSubpart * Subpart = CreateInstanceOf< AIndexedMeshSubpart >();
    Subpart->AddRef();
    Subpart->SetObjectName( Name );
    Subpart->SetBaseVertex( BaseVertex );
    Subpart->SetFirstIndex( FirstIndex );
    Subpart->SetVertexCount( VertexCount );
    Subpart->SetIndexCount( IndexCount );
    Subpart->SetMaterialInstance( GetOrCreateResource< AMaterialInstance >( MaterialInstance.CStr() ) );
    Subpart->SetBoundingBox( BoundingBox );
    return Subpart;
}

static ASocketDef * ReadSocket( IStreamBase & f ) {
    AString Name;
    uint32_t JointIndex;

    f.ReadString( Name );
    JointIndex = f.ReadUInt32();

    ASocketDef * Socket = CreateInstanceOf< ASocketDef >();
    Socket->AddRef();
    Socket->SetObjectName( Name );
    Socket->JointIndex = JointIndex;

    f.ReadObject( Socket->Position );
    f.ReadObject( Socket->Scale );
    f.ReadObject( Socket->Rotation );

    return Socket;
}

bool AIndexedMesh::LoadResource( AString const & _Path ) {
    AScopedTimeCheck ScopedTime( _Path.CStr() );

#if 0
    int i = _Path.FindExt();
    if ( !AString::Icmp( &_Path[i], ".gltf" ) ) {
        SMeshAsset asset;

        if ( !LoadGeometryGLTF( _Path.CStr(), asset ) ) {
            return false;
        }

        bool bSkinned = asset.Weights.Size() == asset.Vertices.Size();

        static TStaticResourceFinder< AMaterial > MaterialResource( _CTS( "/Default/Materials/PBRMetallicRoughness" ) );

        Initialize( asset.Vertices.Size(), asset.Indices.Size(), asset.Subparts.size(), bSkinned, false );
        WriteVertexData( asset.Vertices.ToPtr(), asset.Vertices.Size(), 0 );
        WriteIndexData( asset.Indices.ToPtr(), asset.Indices.Size(), 0 );
        if ( bSkinned ) {
            WriteJointWeights( asset.Weights.ToPtr(), asset.Weights.Size(), 0 );
        }

        TPodArray< AMaterialInstance * > matInstances;
        matInstances.Resize( asset.Materials.Size() );
        for ( int j = 0; j < asset.Materials.Size(); j++ ) {
            AMaterialInstance * matInst = NewObject< AMaterialInstance >();
            matInstances[j] = matInst;
            matInst->SetMaterial( MaterialResource.GetObject() );
            SMeshMaterial const & material = asset.Materials[j];
            for ( int n = 0; n < material.NumTextures; n++ ) {
                SMaterialTexture const & texture = asset.Textures[material.Textures[n]];
                ATexture * texObj = GetOrCreateResource< ATexture >( texture.FileName.CStr() );
                matInst->SetTexture( n, texObj );
            }
        }

        for ( int j = 0; j < GetSubparts().Size(); j++ ) {
            SSubpart const & s = asset.Subparts[j];
            AIndexedMeshSubpart * subpart = GetSubpart( j );
            subpart->SetObjectName( s.Name );
            subpart->SetBaseVertex( s.BaseVertex );
            subpart->SetFirstIndex( s.FirstIndex );
            subpart->SetVertexCount( s.VertexCount );
            subpart->SetIndexCount( s.IndexCount );
            subpart->SetBoundingBox( s.BoundingBox );
            if ( s.Material < matInstances.Size() ) {
                subpart->SetMaterialInstance( matInstances[s.Material] );
            }
        }

        GenerateRigidbodyCollisions();
        GenerateBVH();

        return true;
    }
#endif

    AFileStream f;

    if ( !f.OpenRead( _Path ) ) {
        return false;
    }

    uint32_t fileFormat = f.ReadUInt32();

    if ( fileFormat != FMT_FILE_TYPE_MESH ) {
        GLogger.Printf( "Expected file format %d\n", FMT_FILE_TYPE_MESH );
        return false;
    }

    uint32_t fileVersion = f.ReadUInt32();

    if ( fileVersion != FMT_VERSION_MESH ) {
        GLogger.Printf( "Expected file version %d\n", FMT_VERSION_MESH );
        return false;
    }

    Purge();

    bool bRaycastBVH;

    AString guidStr;
    f.ReadString( guidStr );

    bSkinnedMesh = f.ReadBool();
    bDynamicStorage = f.ReadBool();
    f.ReadObject( BoundingBox );
    f.ReadArrayUInt32( Indices );
    f.ReadArrayOfStructs( Vertices );
    f.ReadArrayOfStructs( Weights );
    bRaycastBVH = f.ReadBool();
    RaycastPrimitivesPerLeaf = f.ReadUInt16();

    uint32_t subpartsCount = f.ReadUInt32();
    Subparts.ResizeInvalidate( subpartsCount );
    for ( int i = 0 ; i < Subparts.Size() ; i++ ) {
        Subparts[i] = ReadIndexedMeshSubpart( f );
    }

    if ( bRaycastBVH ) {
        for ( AIndexedMeshSubpart * subpart : Subparts ) {
            ATreeAABB * bvh = NewObject< ATreeAABB >();

            bvh->Read( f );

            subpart->SetBVH( bvh );
        }
    }

    uint32_t socketsCount = f.ReadUInt32();
    Sockets.ResizeInvalidate( socketsCount );
    for ( int i = 0 ; i < Sockets.Size() ; i++ ) {
        Sockets[i] = ReadSocket( f );
    }

    AString skeleton;
    f.ReadString( skeleton );

    if ( bSkinnedMesh ) {
        f.ReadArrayInt32( Skin.JointIndices );
        f.ReadArrayOfStructs( Skin.OffsetMatrices );
    }

    for ( AIndexedMeshSubpart * subpart : Subparts ) {
        subpart->OwnerMesh = this;
    }

    SetSkeleton( GetOrCreateResource< ASkeleton >( skeleton.CStr() ) );

    GRenderBackend->InitializeBuffer( VertexBufferGPU, Vertices.Size() * sizeof( SMeshVertex ), bDynamicStorage );
    GRenderBackend->InitializeBuffer( IndexBufferGPU, Indices.Size() * sizeof( unsigned int ), bDynamicStorage );

    if ( bSkinnedMesh ) {
        GRenderBackend->InitializeBuffer( WeightsBufferGPU, Weights.Size() * sizeof( SMeshVertexJoint ), bDynamicStorage );
    }

    SendVertexDataToGPU( Vertices.Size(), 0 );
    SendIndexDataToGPU( Indices.Size(), 0 );
    if ( bSkinnedMesh ) {
        SendJointWeightsToGPU( Weights.Size(), 0 );
    }

    bBoundingBoxDirty = false;

    if ( !bSkinnedMesh ) {
        GenerateRigidbodyCollisions();   // TODO: load collision from file
    }

    return true;
}

ALightmapUV * AIndexedMesh::CreateLightmapUVChannel() {
    ALightmapUV * channel = NewObject< ALightmapUV >();

    channel->OwnerMesh = this;
    channel->IndexInArrayOfUVs = LightmapUVs.Size();

    LightmapUVs.Append( channel );

    channel->OnInitialize( Vertices.Size() );

    return channel;
}

AVertexLight * AIndexedMesh::CreateVertexLightChannel() {
    AVertexLight * channel = NewObject< AVertexLight >();

    channel->OwnerMesh = this;
    channel->IndexInArrayOfChannels = VertexLightChannels.Size();

    VertexLightChannels.Append( channel );

    channel->OnInitialize( Vertices.Size() );

    return channel;
}

void AIndexedMesh::AddSocket( ASocketDef * _Socket ) {
    _Socket->AddRef();
    Sockets.Append( _Socket );
}

ASocketDef * AIndexedMesh::FindSocket( const char * _Name ) {
    for ( ASocketDef * socket : Sockets ) {
        if ( socket->GetObjectName().Icmp( _Name ) ) {
            return socket;
        }
    }
    return nullptr;
}

void AIndexedMesh::GenerateBVH( unsigned int PrimitivesPerLeaf ) {
    AScopedTimeCheck ScopedTime( "GenerateBVH" );

    if ( bSkinnedMesh ) {
        GLogger.Printf( "AIndexedMesh::GenerateBVH: called for skinned mesh\n" );
        return;
    }

    const unsigned int MaxPrimitivesPerLeaf = 1024;

    // Don't allow to generate large leafs
    if ( PrimitivesPerLeaf > MaxPrimitivesPerLeaf ) {
        PrimitivesPerLeaf = MaxPrimitivesPerLeaf;
    }

    for ( AIndexedMeshSubpart * subpart : Subparts ) {
        subpart->GenerateBVH( PrimitivesPerLeaf );
    }

    RaycastPrimitivesPerLeaf = PrimitivesPerLeaf;
}

void AIndexedMesh::SetSkeleton( ASkeleton * _Skeleton ) {
    Skeleton = _Skeleton;

    if ( !Skeleton ) {
        static TStaticResourceFinder< ASkeleton > SkeletonResource( _CTS( "/Default/Skeleton/Default" ) );
        Skeleton = SkeletonResource.GetObject();
    }
}

void AIndexedMesh::SetSkin( int32_t const * _JointIndices, Float3x4 const * _OffsetMatrices, int _JointsCount ) {
    Skin.JointIndices.ResizeInvalidate( _JointsCount );
    Skin.OffsetMatrices.ResizeInvalidate( _JointsCount );

    memcpy( Skin.JointIndices.ToPtr(), _JointIndices, _JointsCount * sizeof(*_JointIndices) );
    memcpy( Skin.OffsetMatrices.ToPtr(), _OffsetMatrices, _JointsCount * sizeof(*_OffsetMatrices) );
}

void AIndexedMesh::SetMaterialInstance( int _SubpartIndex, AMaterialInstance * _MaterialInstance ) {
    if ( _SubpartIndex < 0 || _SubpartIndex >= Subparts.Size() ) {
        return;
    }
    Subparts[_SubpartIndex]->SetMaterialInstance( _MaterialInstance );
}

void AIndexedMesh::SetBoundingBox( int _SubpartIndex, BvAxisAlignedBox const & _BoundingBox ) {
    if ( _SubpartIndex < 0 || _SubpartIndex >= Subparts.Size() ) {
        return;
    }
    Subparts[_SubpartIndex]->SetBoundingBox( _BoundingBox );
}

AIndexedMeshSubpart * AIndexedMesh::GetSubpart( int _SubpartIndex ) {
    if ( _SubpartIndex < 0 || _SubpartIndex >= Subparts.Size() ) {
        return nullptr;
    }
    return Subparts[ _SubpartIndex ];
}

bool AIndexedMesh::SendVertexDataToGPU( int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "AIndexedMesh::SendVertexDataToGPU: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    GRenderBackend->WriteBuffer( VertexBufferGPU, _StartVertexLocation * sizeof( SMeshVertex ), _VerticesCount * sizeof( SMeshVertex ), Vertices.ToPtr() + _StartVertexLocation );

    return true;
}

bool AIndexedMesh::WriteVertexData( SMeshVertex const * _Vertices, int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "AIndexedMesh::WriteVertexData: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    memcpy( Vertices.ToPtr() + _StartVertexLocation, _Vertices, _VerticesCount * sizeof( SMeshVertex ) );

    for ( AIndexedMeshSubpart * subpart : Subparts ) {
        subpart->bAABBTreeDirty = true;
    }

    return SendVertexDataToGPU( _VerticesCount, _StartVertexLocation );
}

bool AIndexedMesh::SendJointWeightsToGPU( int _VerticesCount, int _StartVertexLocation ) {
    if ( !bSkinnedMesh ) {
        GLogger.Printf( "AIndexedMesh::SendJointWeightsToGPU: Cannot write joint weights for static mesh\n" );
        return false;
    }

    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Weights.Size() ) {
        GLogger.Printf( "AIndexedMesh::SendJointWeightsToGPU: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    GRenderBackend->WriteBuffer( WeightsBufferGPU, _StartVertexLocation * sizeof( SMeshVertexJoint ), _VerticesCount * sizeof( SMeshVertexJoint ), Weights.ToPtr() + _StartVertexLocation );

    return true;
}

bool AIndexedMesh::WriteJointWeights( SMeshVertexJoint const * _Vertices, int _VerticesCount, int _StartVertexLocation ) {
    if ( !bSkinnedMesh ) {
        GLogger.Printf( "AIndexedMesh::WriteJointWeights: Cannot write joint weights for static mesh\n" );
        return false;
    }

    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Weights.Size() ) {
        GLogger.Printf( "AIndexedMesh::WriteJointWeights: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    memcpy( Weights.ToPtr() + _StartVertexLocation, _Vertices, _VerticesCount * sizeof( SMeshVertexJoint ) );

    return SendJointWeightsToGPU( _VerticesCount, _StartVertexLocation );
}

bool AIndexedMesh::SendIndexDataToGPU( int _IndexCount, int _StartIndexLocation ) {
    if ( !_IndexCount ) {
        return true;
    }

    if ( _StartIndexLocation + _IndexCount > Indices.Size() ) {
        GLogger.Printf( "AIndexedMesh::SendIndexDataToGPU: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    GRenderBackend->WriteBuffer( IndexBufferGPU, _StartIndexLocation * sizeof( unsigned int ), _IndexCount * sizeof( unsigned int ), Indices.ToPtr() + _StartIndexLocation );

    return true;
}

bool AIndexedMesh::WriteIndexData( unsigned int const * _Indices, int _IndexCount, int _StartIndexLocation ) {
    if ( !_IndexCount ) {
        return true;
    }

    if ( _StartIndexLocation + _IndexCount > Indices.Size() ) {
        GLogger.Printf( "AIndexedMesh::WriteIndexData: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    memcpy( Indices.ToPtr() + _StartIndexLocation, _Indices, _IndexCount * sizeof( unsigned int ) );

    for ( AIndexedMeshSubpart * subpart : Subparts ) {
        if ( _StartIndexLocation >= subpart->FirstIndex
            && _StartIndexLocation + _IndexCount <= subpart->FirstIndex + subpart->IndexCount ) {
            subpart->bAABBTreeDirty = true;
        }
    }

    return SendIndexDataToGPU( _IndexCount, _StartIndexLocation );
}

void AIndexedMesh::UpdateBoundingBox() {
    BoundingBox.Clear();
    for ( AIndexedMeshSubpart const * subpart : Subparts ) {
        BoundingBox.AddAABB( subpart->GetBoundingBox() );
    }
    bBoundingBoxDirty = false;
}

BvAxisAlignedBox const & AIndexedMesh::GetBoundingBox() const {
    if ( bBoundingBoxDirty ) {
        const_cast< ThisClass * >( this )->UpdateBoundingBox();
    }

    return BoundingBox;
}

void AIndexedMesh::InitializeBoxMesh( const Float3 & _Size, float _TexCoordScale ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreateBoxMesh( vertices, indices, bounds, _Size, _TexCoordScale );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializeSphereMesh( float _Radius, float _TexCoordScale, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;
    
    CreateSphereMesh( vertices, indices, bounds, _Radius, _TexCoordScale, _NumVerticalSubdivs, _NumHorizontalSubdivs );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializePlaneMesh( float _Width, float _Height, float _TexCoordScale ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreatePlaneMesh( vertices, indices, bounds, _Width, _Height, _TexCoordScale );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializePatchMesh( Float3 const & Corner00, Float3 const & Corner10, Float3 const & Corner01, Float3 const & Corner11, float _TexCoordScale, bool _TwoSided, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreatePatchMesh( vertices, indices, bounds,
        Corner00, Corner10, Corner01, Corner11, _TexCoordScale, _TwoSided, _NumVerticalSubdivs, _NumHorizontalSubdivs );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializeCylinderMesh( float _Radius, float _Height, float _TexCoordScale, int _NumSubdivs ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreateCylinderMesh( vertices, indices, bounds, _Radius, _Height, _TexCoordScale, _NumSubdivs );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializeConeMesh( float _Radius, float _Height, float _TexCoordScale, int _NumSubdivs ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreateConeMesh( vertices, indices, bounds, _Radius, _Height, _TexCoordScale, _NumSubdivs );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::InitializeCapsuleMesh( float _Radius, float _Height, float _TexCoordScale, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {
    TPodArray< SMeshVertex > vertices;
    TPodArray< unsigned int > indices;
    BvAxisAlignedBox bounds;

    CreateCapsuleMesh( vertices, indices, bounds, _Radius, _Height, _TexCoordScale, _NumVerticalSubdivs, _NumHorizontalSubdivs );

    Initialize( vertices.Size(), indices.Size(), 1 );
    WriteVertexData( vertices.ToPtr(), vertices.Size(), 0 );
    WriteIndexData( indices.ToPtr(), indices.Size(), 0 );

    Subparts[ 0 ]->BoundingBox = bounds;
}

void AIndexedMesh::LoadInternalResource( const char * _Path ) {

    if ( !AString::Icmp( _Path, "/Default/Meshes/Box" ) ) {
        InitializeBoxMesh( Float3(1), 1 );
        ACollisionBox * collisionBody = BodyComposition.AddCollisionBody< ACollisionBox >();
        collisionBody->HalfExtents = Float3(0.5f);
        return;
    }

    if ( !AString::Icmp( _Path, "/Default/Meshes/Sphere" ) ) {
        InitializeSphereMesh( 0.5f, 1 );
        ACollisionSphere * collisionBody = BodyComposition.AddCollisionBody< ACollisionSphere >();
        collisionBody->Radius = 0.5f;
        return;
    }

    if ( !AString::Icmp( _Path, "/Default/Meshes/Cylinder" ) ) {
        InitializeCylinderMesh( 0.5f, 1, 1 );
        ACollisionCylinder * collisionBody = BodyComposition.AddCollisionBody< ACollisionCylinder >();
        collisionBody->HalfExtents = Float3(0.5f);
        return;
    }

    if ( !AString::Icmp( _Path, "/Default/Meshes/Cone" ) ) {
        InitializeConeMesh( 0.5f, 1, 1 );
        ACollisionCone * collisionBody = BodyComposition.AddCollisionBody< ACollisionCone >();
        collisionBody->Radius = 0.5f;
        collisionBody->Height = 1.0f;
        return;
    }

    if ( !AString::Icmp( _Path, "/Default/Meshes/Capsule" ) ) {
        InitializeCapsuleMesh( 0.5f, 1.0f, 1 );
        ACollisionCapsule * collisionBody = BodyComposition.AddCollisionBody< ACollisionCapsule >();
        collisionBody->Radius = 0.5f;
        collisionBody->Height = 1;
        return;
    }

    if ( !AString::Icmp( _Path, "/Default/Meshes/Plane") ) {
        InitializePlaneMesh( 256, 256, 256 );
        ACollisionBox * box = BodyComposition.AddCollisionBody< ACollisionBox >();
        box->HalfExtents.X = 128;
        box->HalfExtents.Y = 0.1f;
        box->HalfExtents.Z = 128;
        box->Position.Y -= box->HalfExtents.Y;
        return;
    }

    GLogger.Printf( "Unknown internal mesh %s\n", _Path );

    LoadInternalResource( "/Default/Meshes/Box" );
}


///////////////////////////////////////////////////////////////////////////////////////////////////////

AIndexedMeshSubpart::AIndexedMeshSubpart() {
    BoundingBox.Clear();

    static TStaticResourceFinder< AMaterialInstance > DefaultMaterialInstance( _CTS( "/Default/MaterialInstance/Default" ) );
    MaterialInstance = DefaultMaterialInstance.GetObject();
}

AIndexedMeshSubpart::~AIndexedMeshSubpart() {
}

void AIndexedMeshSubpart::SetBaseVertex( int _BaseVertex ) {
    BaseVertex = _BaseVertex;
    bAABBTreeDirty = true;
}

void AIndexedMeshSubpart::SetFirstIndex( int _FirstIndex ) {
    FirstIndex = _FirstIndex;
    bAABBTreeDirty = true;
}

void AIndexedMeshSubpart::SetVertexCount( int _VertexCount ) {
    VertexCount = _VertexCount;
}

void AIndexedMeshSubpart::SetIndexCount( int _IndexCount ) {
    IndexCount = _IndexCount;
    bAABBTreeDirty = true;
}

void AIndexedMeshSubpart::SetMaterialInstance( AMaterialInstance * _MaterialInstance ) {
    MaterialInstance = _MaterialInstance;

    if ( !MaterialInstance ) {
        static TStaticResourceFinder< AMaterialInstance > DefaultMaterialInstance( _CTS( "/Default/MaterialInstance/Default" ) );
        MaterialInstance = DefaultMaterialInstance.GetObject();
    }
}

void AIndexedMeshSubpart::SetBoundingBox( BvAxisAlignedBox const & _BoundingBox ) {
    BoundingBox = _BoundingBox;

    if ( OwnerMesh ) {
        OwnerMesh->bBoundingBoxDirty = true;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

ALightmapUV::ALightmapUV() {
    VertexBufferGPU = GRenderBackend->CreateBuffer( this );
}

ALightmapUV::~ALightmapUV() {
    GRenderBackend->DestroyBuffer( VertexBufferGPU );

    if ( OwnerMesh ) {
        OwnerMesh->LightmapUVs[ IndexInArrayOfUVs ] = OwnerMesh->LightmapUVs[ OwnerMesh->LightmapUVs.Size() - 1 ];
        OwnerMesh->LightmapUVs[ IndexInArrayOfUVs ]->IndexInArrayOfUVs = IndexInArrayOfUVs;
        IndexInArrayOfUVs = -1;
        OwnerMesh->LightmapUVs.RemoveLast();
    }
}

void ALightmapUV::OnInitialize( int _NumVertices ) {
    if ( Vertices.Size() == _NumVertices && bDynamicStorage == OwnerMesh->bDynamicStorage ) {
        return;
    }

    bDynamicStorage = OwnerMesh->bDynamicStorage;

    Vertices.ResizeInvalidate( _NumVertices );

    GRenderBackend->InitializeBuffer( VertexBufferGPU, Vertices.Size() * sizeof( SMeshLightmapUV ), bDynamicStorage );
}

bool ALightmapUV::SendVertexDataToGPU( int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "ALightmapUV::SendVertexDataToGPU: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    GRenderBackend->WriteBuffer( VertexBufferGPU, _StartVertexLocation * sizeof( SMeshLightmapUV ), _VerticesCount * sizeof( SMeshLightmapUV ), Vertices.ToPtr() + _StartVertexLocation );

    return true;
}

bool ALightmapUV::WriteVertexData( SMeshLightmapUV const * _Vertices, int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "ALightmapUV::WriteVertexData: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    memcpy( Vertices.ToPtr() + _StartVertexLocation, _Vertices, _VerticesCount * sizeof( SMeshLightmapUV ) );

    return SendVertexDataToGPU( _VerticesCount, _StartVertexLocation );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

AVertexLight::AVertexLight() {
    VertexBufferGPU = GRenderBackend->CreateBuffer( this );
}

AVertexLight::~AVertexLight() {
    GRenderBackend->DestroyBuffer( VertexBufferGPU );

    if ( OwnerMesh ) {
        OwnerMesh->VertexLightChannels[ IndexInArrayOfChannels ] = OwnerMesh->VertexLightChannels[ OwnerMesh->VertexLightChannels.Size() - 1 ];
        OwnerMesh->VertexLightChannels[ IndexInArrayOfChannels ]->IndexInArrayOfChannels = IndexInArrayOfChannels;
        IndexInArrayOfChannels = -1;
        OwnerMesh->VertexLightChannels.RemoveLast();
    }
}

void AVertexLight::OnInitialize( int _NumVertices ) {
    if ( Vertices.Size() == _NumVertices && bDynamicStorage == OwnerMesh->bDynamicStorage ) {
        return;
    }

    bDynamicStorage = OwnerMesh->bDynamicStorage;

    Vertices.ResizeInvalidate( _NumVertices );

    GRenderBackend->InitializeBuffer( VertexBufferGPU, Vertices.Size() * sizeof( SMeshVertexLight ), bDynamicStorage );
}

bool AVertexLight::SendVertexDataToGPU( int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "AVertexLight::SendVertexDataToGPU: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    GRenderBackend->WriteBuffer( VertexBufferGPU, _StartVertexLocation * sizeof( SMeshVertexLight ), _VerticesCount * sizeof( SMeshVertexLight ), Vertices.ToPtr() + _StartVertexLocation );

    return true;
}

bool AVertexLight::WriteVertexData( SMeshVertexLight const * _Vertices, int _VerticesCount, int _StartVertexLocation ) {
    if ( !_VerticesCount ) {
        return true;
    }

    if ( _StartVertexLocation + _VerticesCount > Vertices.Size() ) {
        GLogger.Printf( "AVertexLight::WriteVertexData: Referencing outside of buffer (%s)\n", GetObjectNameConstChar() );
        return false;
    }

    memcpy( Vertices.ToPtr() + _StartVertexLocation, _Vertices, _VerticesCount * sizeof( SMeshVertexLight ) );

    return SendVertexDataToGPU( _VerticesCount, _StartVertexLocation );
}

void AIndexedMesh::GenerateRigidbodyCollisions() {
    AScopedTimeCheck ScopedTime( "GenerateRigidbodyCollisions" );

    ACollisionTriangleSoupData * tris = NewObject< ACollisionTriangleSoupData >();
    tris->Initialize( (float *)&Vertices.ToPtr()->Position, sizeof( Vertices[0] ), Vertices.Size(),
        Indices.ToPtr(), Indices.Size(), Subparts.ToPtr(), Subparts.Size() );

    ACollisionTriangleSoupBVHData * bvh = NewObject< ACollisionTriangleSoupBVHData >();
    bvh->TrisData = tris;
    bvh->BuildBVH();

    BodyComposition.Clear();
    ACollisionTriangleSoupBVH * CollisionBody = BodyComposition.AddCollisionBody< ACollisionTriangleSoupBVH >();
    CollisionBody->BvhData = bvh;
}

void AIndexedMesh::GenerateSoftbodyFacesFromMeshIndices() {
    AScopedTimeCheck ScopedTime( "GenerateSoftbodyFacesFromMeshIndices" );

    int totalIndices = 0;

    for ( AIndexedMeshSubpart const * subpart : Subparts ) {
        totalIndices += subpart->IndexCount;
    }

    SoftbodyFaces.ResizeInvalidate( totalIndices / 3 );

    int faceIndex = 0;

    unsigned int const * indices = Indices.ToPtr();

    for ( AIndexedMeshSubpart const * subpart : Subparts ) {
        for ( int i = 0; i < subpart->IndexCount; i += 3 ) {
            SSoftbodyFace & face = SoftbodyFaces[ faceIndex++ ];

            face.Indices[ 0 ] = subpart->BaseVertex + indices[ subpart->FirstIndex + i ];
            face.Indices[ 1 ] = subpart->BaseVertex + indices[ subpart->FirstIndex + i + 1 ];
            face.Indices[ 2 ] = subpart->BaseVertex + indices[ subpart->FirstIndex + i + 2 ];
        }
    }
}

void AIndexedMesh::GenerateSoftbodyLinksFromFaces() {
    AScopedTimeCheck ScopedTime( "GenerateSoftbodyLinksFromFaces" );

    TPodArray< bool > checks;
    unsigned int * indices;

    checks.Resize( Vertices.Size()*Vertices.Size() );
    checks.ZeroMem();

    SoftbodyLinks.Clear();

    for ( SSoftbodyFace & face : SoftbodyFaces ) {
        indices = face.Indices;

        for ( int j = 2, k = 0; k < 3; j = k++ ) {

            unsigned int index_j_k = indices[ j ] + indices[ k ]*Vertices.Size();

            // Check if link not exists
            if ( !checks[ index_j_k ] ) {

                unsigned int index_k_j = indices[ k ] + indices[ j ]*Vertices.Size();

                // Mark link exits
                checks[ index_j_k ] = checks[ index_k_j ] = true;

                // Append link
                SSoftbodyLink & link = SoftbodyLinks.Append();
                link.Indices[0] = indices[ j ];
                link.Indices[1] = indices[ k ];
            }
        }
        #undef IDX
    }
}


void AIndexedMeshSubpart::GenerateBVH( unsigned int PrimitivesPerLeaf ) {
    // TODO: Try KD-tree

    if ( OwnerMesh )
    {
        AABBTree = NewObject< ATreeAABB >();
        AABBTree->Initialize( OwnerMesh->Vertices.ToPtr(), OwnerMesh->Indices.ToPtr() + FirstIndex, IndexCount, BaseVertex, PrimitivesPerLeaf );
        bAABBTreeDirty = false;
    }
}

void AIndexedMeshSubpart::SetBVH( ATreeAABB * BVH ) {
    AABBTree = BVH;
    bAABBTreeDirty = false;
}

bool AIndexedMeshSubpart::Raycast( Float3 const & _RayStart, Float3 const & _RayDir, float _Distance, TPodArray< STriangleHitResult > & _HitResult ) const {
    bool ret = false;
    float d, u, v;
    unsigned int const * indices = OwnerMesh->GetIndices() + FirstIndex;
    SMeshVertex const * vertices = OwnerMesh->GetVertices();

    if ( _Distance < 0.0001f ) {
        return false;
    }

    if ( AABBTree ) {
        if ( bAABBTreeDirty ) {
            GLogger.Printf( "AIndexedMeshSubpart::Raycast: bvh is outdated\n" );
            return false;
        }

        Float3 invRayDir;
        invRayDir.X = 1.0f / _RayDir.X;
        invRayDir.Y = 1.0f / _RayDir.Y;
        invRayDir.Z = 1.0f / _RayDir.Z;

        TPodArray< SNodeAABB > const & nodes = AABBTree->GetNodes();
        unsigned int const * indirection = AABBTree->GetIndirection();

        float hitMin, hitMax;

        for ( int nodeIndex = 0; nodeIndex < nodes.Size(); ) {
            SNodeAABB const * node = &nodes[nodeIndex];

            const bool bOverlap = BvRayIntersectBox( _RayStart, invRayDir, node->Bounds, hitMin, hitMax ) && hitMin <= _Distance;
            const bool bLeaf = node->IsLeaf();

            if ( bLeaf && bOverlap ) {
                for ( int t = 0; t < node->PrimitiveCount; t++ ) {
                    const int triangleNum = node->Index + t;
                    const unsigned int baseInd = indirection[triangleNum];
                    const unsigned int i0 = BaseVertex + indices[baseInd + 0];
                    const unsigned int i1 = BaseVertex + indices[baseInd + 1];
                    const unsigned int i2 = BaseVertex + indices[baseInd + 2];
                    Float3 const & v0 = vertices[i0].Position;
                    Float3 const & v1 = vertices[i1].Position;
                    Float3 const & v2 = vertices[i2].Position;
                    if ( BvRayIntersectTriangle( _RayStart, _RayDir, v0, v1, v2, d, u, v ) ) {
                        if ( _Distance > d ) {
                            STriangleHitResult & hitResult = _HitResult.Append();
                            hitResult.Location = _RayStart + _RayDir * d;
                            hitResult.Normal = (v1 - v0).Cross( v2-v0 ).Normalized();
                            hitResult.Distance = d;
                            hitResult.UV.X = u;
                            hitResult.UV.Y = v;
                            hitResult.Indices[0] = i0;
                            hitResult.Indices[1] = i1;
                            hitResult.Indices[2] = i2;
                            hitResult.Material = MaterialInstance;
                            ret = true;
                        }
                    }
                }
            }

            nodeIndex += (bOverlap || bLeaf) ? 1 : (-node->Index);
        }
    } else {
        // TODO: check subpart AABB

        const int primCount = IndexCount / 3;

        for ( int tri = 0 ; tri < primCount; tri++, indices+=3 ) {
            const unsigned int i0 = BaseVertex + indices[ 0 ];
            const unsigned int i1 = BaseVertex + indices[ 1 ];
            const unsigned int i2 = BaseVertex + indices[ 2 ];

            Float3 const & v0 = vertices[i0].Position;
            Float3 const & v1 = vertices[i1].Position;
            Float3 const & v2 = vertices[i2].Position;

            if ( BvRayIntersectTriangle( _RayStart, _RayDir, v0, v1, v2, d, u, v ) ) {
                if ( _Distance > d ) {
                    STriangleHitResult & hitResult = _HitResult.Append();
                    hitResult.Location = _RayStart + _RayDir * d;
                    hitResult.Normal = ( v1 - v0 ).Cross( v2-v0 ).Normalized();
                    hitResult.Distance = d;
                    hitResult.UV.X = u;
                    hitResult.UV.Y = v;
                    hitResult.Indices[0] = i0;
                    hitResult.Indices[1] = i1;
                    hitResult.Indices[2] = i2;
                    hitResult.Material = MaterialInstance;
                    ret = true;
                }
            }
        }
    }
    return ret;
}

bool AIndexedMeshSubpart::RaycastClosest( Float3 const & _RayStart, Float3 const & _RayDir, float _Distance, Float3 & _HitLocation, Float2 & _HitUV, float & _HitDistance, unsigned int _Indices[3] ) const {
    bool ret = false;
    float d, u, v;
    unsigned int const * indices = OwnerMesh->GetIndices() + FirstIndex;
    SMeshVertex const * vertices = OwnerMesh->GetVertices();

    if ( _Distance < 0.0001f ) {
        return false;
    }

    float minDist = _Distance;

    if ( AABBTree ) {
        if ( bAABBTreeDirty ) {
            GLogger.Printf( "AIndexedMeshSubpart::RaycastClosest: bvh is outdated\n" );
            return false;
        }

        Float3 invRayDir;
        invRayDir.X = 1.0f / _RayDir.X;
        invRayDir.Y = 1.0f / _RayDir.Y;
        invRayDir.Z = 1.0f / _RayDir.Z;

        TPodArray< SNodeAABB > const & nodes = AABBTree->GetNodes();
        unsigned int const * indirection = AABBTree->GetIndirection();

        float hitMin, hitMax;

        for ( int nodeIndex = 0; nodeIndex < nodes.Size(); ) {
            SNodeAABB const * node = &nodes[nodeIndex];

            const bool bOverlap = BvRayIntersectBox( _RayStart, invRayDir, node->Bounds, hitMin, hitMax ) && hitMin <= _Distance;
            const bool bLeaf = node->IsLeaf();

            if ( bLeaf && bOverlap ) {
                for ( int t = 0; t < node->PrimitiveCount; t++ ) {
                    const int triangleNum = node->Index + t;
                    const unsigned int baseInd = indirection[triangleNum];
                    const unsigned int i0 = BaseVertex + indices[baseInd + 0];
                    const unsigned int i1 = BaseVertex + indices[baseInd + 1];
                    const unsigned int i2 = BaseVertex + indices[baseInd + 2];
                    Float3 const & v0 = vertices[i0].Position;
                    Float3 const & v1 = vertices[i1].Position;
                    Float3 const & v2 = vertices[i2].Position;
                    if ( BvRayIntersectTriangle( _RayStart, _RayDir, v0, v1, v2, d, u, v ) ) {
                        if ( minDist > d ) {
                            minDist = d;
                            _HitLocation = _RayStart + _RayDir * d;
                            _HitDistance = d;
                            _HitUV.X = u;
                            _HitUV.Y = v;
                            _Indices[0] = i0;
                            _Indices[1] = i1;
                            _Indices[2] = i2;
                            ret = true;
                        }
                    }
                }
            }

            nodeIndex += (bOverlap || bLeaf) ? 1 : (-node->Index);
        }
    } else {
        // TODO: check subpart AABB

        const int primCount = IndexCount / 3;

        for ( int tri = 0 ; tri < primCount; tri++, indices+=3 ) {
            const unsigned int i0 = BaseVertex + indices[ 0 ];
            const unsigned int i1 = BaseVertex + indices[ 1 ];
            const unsigned int i2 = BaseVertex + indices[ 2 ];

            Float3 const & v0 = vertices[i0].Position;
            Float3 const & v1 = vertices[i1].Position;
            Float3 const & v2 = vertices[i2].Position;

            if ( BvRayIntersectTriangle( _RayStart, _RayDir, v0, v1, v2, d, u, v ) ) {
                if ( minDist > d ) {
                    minDist = d;
                    _HitLocation = _RayStart + _RayDir * d;
                    _HitDistance = d;
                    _HitUV.X = u;
                    _HitUV.Y = v;
                    _Indices[0] = i0;
                    _Indices[1] = i1;
                    _Indices[2] = i2;
                    ret = true;
                }
            }
        }
    }
    return ret;
}

bool AIndexedMesh::Raycast( Float3 const & _RayStart, Float3 const & _RayDir, float _Distance, TPodArray< STriangleHitResult > & _HitResult ) const {
    bool ret = false;

    // TODO: check mesh AABB?

    for ( int i = 0 ; i < Subparts.Size() ; i++ ) {
        AIndexedMeshSubpart * subpart = Subparts[i];
        ret |= subpart->Raycast( _RayStart, _RayDir, _Distance, _HitResult );
    }
    return ret;
}

bool AIndexedMesh::RaycastClosest( Float3 const & _RayStart, Float3 const & _RayDir, float _Distance, Float3 & _HitLocation, Float2 & _HitUV, float & _HitDistance, unsigned int _Indices[3], TRef< AMaterialInstance > & _Material ) const {
    bool ret = false;

    // TODO: check mesh AABB?

    for ( int i = 0 ; i < Subparts.Size() ; i++ ) {
        AIndexedMeshSubpart * subpart = Subparts[i];
        if ( subpart->RaycastClosest( _RayStart, _RayDir, _Distance, _HitLocation, _HitUV, _HitDistance, _Indices ) ) {
            _Material = subpart->MaterialInstance;
            _Distance = _HitDistance;
            ret = true;
        }
    }

    return ret;
}

void AIndexedMesh::DrawDebug( ADebugDraw * _DebugDraw ) {
    if ( RVDrawIndexedMeshBVH ) {
        for ( AIndexedMeshSubpart * subpart : Subparts ) {
            subpart->DrawBVH( _DebugDraw );
        }
    }
}

void AIndexedMeshSubpart::DrawBVH( ADebugDraw * _DebugDraw ) {
    if ( !AABBTree ) {
        return;
    }

    _DebugDraw->SetDepthTest( false );
    _DebugDraw->SetColor( AColor4::White() );

    for ( SNodeAABB const & n : AABBTree->GetNodes() ) {
        if ( n.IsLeaf() ) {
            _DebugDraw->DrawAABB( n.Bounds );
        }
    }
}

void CalcTangentSpace( SMeshVertex * _VertexArray, unsigned int _NumVerts, unsigned int const * _IndexArray, unsigned int _NumIndices ) {
    Float3 binormal, tangent;

    TPodArray< Float3 > binormals;
    binormals.ResizeInvalidate( _NumVerts );

    for ( int i = 0; i < _NumVerts; i++ ) {
        _VertexArray[ i ].Tangent = Float3( 0 );
        binormals[ i ] = Float3( 0 );
    }

    for ( unsigned int i = 0; i < _NumIndices; i += 3 ) {
        const unsigned int a = _IndexArray[ i ];
        const unsigned int b = _IndexArray[ i + 1 ];
        const unsigned int c = _IndexArray[ i + 2 ];

        Float3 e1 = _VertexArray[ b ].Position - _VertexArray[ a ].Position;
        Float3 e2 = _VertexArray[ c ].Position - _VertexArray[ a ].Position;
        Float2 et1 = _VertexArray[ b ].TexCoord - _VertexArray[ a ].TexCoord;
        Float2 et2 = _VertexArray[ c ].TexCoord - _VertexArray[ a ].TexCoord;

        const float denom = et1.X*et2.Y - et1.Y*et2.X;
        const float scale = ( fabsf( denom ) < 0.0001f ) ? 1.0f : ( 1.0 / denom );
        tangent = ( e1 * et2.Y - e2 * et1.Y ) * scale;
        binormal = ( e2 * et1.X - e1 * et2.X ) * scale;

        _VertexArray[ a ].Tangent += tangent;
        _VertexArray[ b ].Tangent += tangent;
        _VertexArray[ c ].Tangent += tangent;

        binormals[ a ] += binormal;
        binormals[ b ] += binormal;
        binormals[ c ] += binormal;
    }

    for ( int i = 0; i < _NumVerts; i++ ) {
        const Float3 & n = _VertexArray[ i ].Normal;
        const Float3 & t = _VertexArray[ i ].Tangent;
        _VertexArray[ i ].Tangent = ( t - n * Math::Dot( n, t ) ).Normalized();
        _VertexArray[ i ].Handedness = CalcHandedness( t, binormals[ i ].Normalized(), n );
    }
}


BvAxisAlignedBox CalcBindposeBounds( SMeshVertex const * InVertices,
                                     SMeshVertexJoint const * InWeights,
                                     int InVertexCount,
                                     ASkin const * InSkin,
                                     SJoint * InJoints,
                                     int InJointsCount )
{
    Float3x4 absoluteTransforms[ASkeleton::MAX_JOINTS+1];
    Float3x4 vertexTransforms[ASkeleton::MAX_JOINTS];

    BvAxisAlignedBox BindposeBounds;

    BindposeBounds.Clear();

    absoluteTransforms[0].SetIdentity();
    for ( unsigned int j = 0 ; j < InJointsCount ; j++ ) {
        SJoint const & joint = InJoints[ j ];

        absoluteTransforms[ j + 1 ] = absoluteTransforms[ joint.Parent + 1 ] * joint.LocalTransform;
    }

    for ( unsigned int j = 0 ; j < InSkin->JointIndices.Size() ; j++ ) {
        int jointIndex = InSkin->JointIndices[j];

        vertexTransforms[ j ] = absoluteTransforms[ jointIndex + 1 ] * InSkin->OffsetMatrices[j];
    }

    for ( int v = 0 ; v < InVertexCount ; v++ ) {
        Float4 const position = Float4( InVertices[v].Position, 1.0f );
        SMeshVertexJoint const & w = InWeights[v];

        const float weights[4] = { w.JointWeights[0] / 255.0f, w.JointWeights[1] / 255.0f, w.JointWeights[2] / 255.0f, w.JointWeights[3] / 255.0f };

        Float4 const * t = &vertexTransforms[0][0];

        BindposeBounds.AddPoint(
                    ( t[ w.JointIndices[0] * 3 + 0 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 0 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 0 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 0 ] * weights[3] ).Dot( position ),

                    ( t[ w.JointIndices[0] * 3 + 1 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 1 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 1 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 1 ] * weights[3] ).Dot( position ),

                    ( t[ w.JointIndices[0] * 3 + 2 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 2 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 2 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 2 ] * weights[3] ).Dot( position ) );
    }

    return BindposeBounds;
}

void CalcBoundingBoxes( SMeshVertex const * InVertices,
                        SMeshVertexJoint const * InWeights,
                        int InVertexCount,
                        ASkin const * InSkin,
                        SJoint const *  InJoints,
                        int InNumJoints,
                        uint32_t FrameCount,
                        SAnimationChannel const * InChannels,
                        int InChannelsCount,
                        ATransform const * InTransforms,
                        TPodArray< BvAxisAlignedBox > & Bounds )
{
    Float3x4 absoluteTransforms[ASkeleton::MAX_JOINTS+1];
    TPodArray< Float3x4 > relativeTransforms[ASkeleton::MAX_JOINTS];
    Float3x4 vertexTransforms[ASkeleton::MAX_JOINTS];

    Bounds.ResizeInvalidate( FrameCount );

    for ( int i = 0 ; i < InChannelsCount ; i++ ) {
        SAnimationChannel const & anim = InChannels[i];

        relativeTransforms[anim.JointIndex].ResizeInvalidate( FrameCount );

        for ( int frameNum = 0; frameNum < FrameCount ; frameNum++ ) {

            ATransform const & transform = InTransforms[ anim.TransformOffset + frameNum ];

            transform.ComputeTransformMatrix( relativeTransforms[anim.JointIndex][frameNum] );
        }
    }

    for ( int frameNum = 0 ; frameNum < FrameCount ; frameNum++ ) {

        BvAxisAlignedBox & bounds = Bounds[frameNum];

        bounds.Clear();

        absoluteTransforms[0].SetIdentity();
        for ( unsigned int j = 0 ; j < InNumJoints ; j++ ) {
            SJoint const & joint = InJoints[ j ];

            Float3x4 const & parentTransform = absoluteTransforms[ joint.Parent + 1 ];

            if ( relativeTransforms[j].IsEmpty() ) {
                absoluteTransforms[ j + 1 ] = parentTransform * joint.LocalTransform;
            } else {
                absoluteTransforms[ j + 1 ] = parentTransform * relativeTransforms[j][ frameNum ];
            }
        }

        for ( unsigned int j = 0 ; j < InSkin->JointIndices.Size() ; j++ ) {
            int jointIndex = InSkin->JointIndices[j];

            vertexTransforms[ j ] = absoluteTransforms[ jointIndex + 1 ] * InSkin->OffsetMatrices[j];
        }

        for ( int v = 0 ; v < InVertexCount ; v++ ) {
            Float4 const position = Float4( InVertices[v].Position, 1.0f );
            SMeshVertexJoint const & w = InWeights[v];

            const float weights[4] = { w.JointWeights[0] / 255.0f, w.JointWeights[1] / 255.0f, w.JointWeights[2] / 255.0f, w.JointWeights[3] / 255.0f };

            Float4 const * t = &vertexTransforms[0][0];

            bounds.AddPoint(
                    ( t[ w.JointIndices[0] * 3 + 0 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 0 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 0 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 0 ] * weights[3] ).Dot( position ),

                    ( t[ w.JointIndices[0] * 3 + 1 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 1 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 1 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 1 ] * weights[3] ).Dot( position ),

                    ( t[ w.JointIndices[0] * 3 + 2 ] * weights[0]
                    + t[ w.JointIndices[1] * 3 + 2 ] * weights[1]
                    + t[ w.JointIndices[2] * 3 + 2 ] * weights[2]
                    + t[ w.JointIndices[3] * 3 + 2 ] * weights[3] ).Dot( position ) );
        }
    }
}

void CreateBoxMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, const Float3 & _Size, float _TexCoordScale ) {
    constexpr unsigned int indices[ 6 * 6 ] =
    {
        0, 1, 2, 2, 3, 0, // front face
        4, 5, 6, 6, 7, 4, // back face
        5 + 8 * 1, 0 + 8 * 1, 3 + 8 * 1, 3 + 8 * 1, 6 + 8 * 1, 5 + 8 * 1, // left face
        1 + 8 * 1, 4 + 8 * 1, 7 + 8 * 1, 7 + 8 * 1, 2 + 8 * 1, 1 + 8 * 1, // right face
        3 + 8 * 2, 2 + 8 * 2, 7 + 8 * 2, 7 + 8 * 2, 6 + 8 * 2, 3 + 8 * 2, // top face
        1 + 8 * 2, 0 + 8 * 2, 5 + 8 * 2, 5 + 8 * 2, 4 + 8 * 2, 1 + 8 * 2, // bottom face
    };

    _Vertices.ResizeInvalidate( 24 );
    _Indices.ResizeInvalidate( 36 );

    memcpy( _Indices.ToPtr(), indices, sizeof( indices ) );

    const Float3 halfSize = _Size * 0.5f;

    _Bounds.Mins = -halfSize;
    _Bounds.Maxs = halfSize;

    const Float3 & mins = _Bounds.Mins;
    const Float3 & maxs = _Bounds.Maxs;

    SMeshVertex * pVerts = _Vertices.ToPtr();

    pVerts[ 0 + 8 * 0 ].Position = Float3( mins.X, mins.Y, maxs.Z ); // 0
    pVerts[ 0 + 8 * 0 ].Normal = Float3( 0, 0, 1 );
    pVerts[ 0 + 8 * 0 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 1 + 8 * 0 ].Position = Float3( maxs.X, mins.Y, maxs.Z ); // 1
    pVerts[ 1 + 8 * 0 ].Normal = Float3( 0, 0, 1 );
    pVerts[ 1 + 8 * 0 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;

    pVerts[ 2 + 8 * 0 ].Position = Float3( maxs.X, maxs.Y, maxs.Z ); // 2
    pVerts[ 2 + 8 * 0 ].Normal = Float3( 0, 0, 1 );
    pVerts[ 2 + 8 * 0 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;

    pVerts[ 3 + 8 * 0 ].Position = Float3( mins.X, maxs.Y, maxs.Z ); // 3
    pVerts[ 3 + 8 * 0 ].Normal = Float3( 0, 0, 1 );
    pVerts[ 3 + 8 * 0 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;


    pVerts[ 4 + 8 * 0 ].Position = Float3( maxs.X, mins.Y, mins.Z ); // 4
    pVerts[ 4 + 8 * 0 ].Normal = Float3( 0, 0, -1 );
    pVerts[ 4 + 8 * 0 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 5 + 8 * 0 ].Position = Float3( mins.X, mins.Y, mins.Z ); // 5
    pVerts[ 5 + 8 * 0 ].Normal = Float3( 0, 0, -1 );
    pVerts[ 5 + 8 * 0 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;

    pVerts[ 6 + 8 * 0 ].Position = Float3( mins.X, maxs.Y, mins.Z ); // 6
    pVerts[ 6 + 8 * 0 ].Normal = Float3( 0, 0, -1 );
    pVerts[ 6 + 8 * 0 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;

    pVerts[ 7 + 8 * 0 ].Position = Float3( maxs.X, maxs.Y, mins.Z ); // 7
    pVerts[ 7 + 8 * 0 ].Normal = Float3( 0, 0, -1 );
    pVerts[ 7 + 8 * 0 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;


    pVerts[ 0 + 8 * 1 ].Position = Float3( mins.X, mins.Y, maxs.Z ); // 0
    pVerts[ 0 + 8 * 1 ].Normal = Float3( -1, 0, 0 );
    pVerts[ 0 + 8 * 1 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;

    pVerts[ 1 + 8 * 1 ].Position = Float3( maxs.X, mins.Y, maxs.Z ); // 1
    pVerts[ 1 + 8 * 1 ].Normal = Float3( 1, 0, 0 );
    pVerts[ 1 + 8 * 1 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 2 + 8 * 1 ].Position = Float3( maxs.X, maxs.Y, maxs.Z ); // 2
    pVerts[ 2 + 8 * 1 ].Normal = Float3( 1, 0, 0 );
    pVerts[ 2 + 8 * 1 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;

    pVerts[ 3 + 8 * 1 ].Position = Float3( mins.X, maxs.Y, maxs.Z ); // 3
    pVerts[ 3 + 8 * 1 ].Normal = Float3( -1, 0, 0 );
    pVerts[ 3 + 8 * 1 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;


    pVerts[ 4 + 8 * 1 ].Position = Float3( maxs.X, mins.Y, mins.Z ); // 4
    pVerts[ 4 + 8 * 1 ].Normal = Float3( 1, 0, 0 );
    pVerts[ 4 + 8 * 1 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;

    pVerts[ 5 + 8 * 1 ].Position = Float3( mins.X, mins.Y, mins.Z ); // 5
    pVerts[ 5 + 8 * 1 ].Normal = Float3( -1, 0, 0 );
    pVerts[ 5 + 8 * 1 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 6 + 8 * 1 ].Position = Float3( mins.X, maxs.Y, mins.Z ); // 6
    pVerts[ 6 + 8 * 1 ].Normal = Float3( -1, 0, 0 );
    pVerts[ 6 + 8 * 1 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;

    pVerts[ 7 + 8 * 1 ].Position = Float3( maxs.X, maxs.Y, mins.Z ); // 7
    pVerts[ 7 + 8 * 1 ].Normal = Float3( 1, 0, 0 );
    pVerts[ 7 + 8 * 1 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;


    pVerts[ 1 + 8 * 2 ].Position = Float3( maxs.X, mins.Y, maxs.Z ); // 1
    pVerts[ 1 + 8 * 2 ].Normal = Float3( 0, -1, 0 );
    pVerts[ 1 + 8 * 2 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;

    pVerts[ 0 + 8 * 2 ].Position = Float3( mins.X, mins.Y, maxs.Z ); // 0
    pVerts[ 0 + 8 * 2 ].Normal = Float3( 0, -1, 0 );
    pVerts[ 0 + 8 * 2 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;

    pVerts[ 5 + 8 * 2 ].Position = Float3( mins.X, mins.Y, mins.Z ); // 5
    pVerts[ 5 + 8 * 2 ].Normal = Float3( 0, -1, 0 );
    pVerts[ 5 + 8 * 2 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 4 + 8 * 2 ].Position = Float3( maxs.X, mins.Y, mins.Z ); // 4
    pVerts[ 4 + 8 * 2 ].Normal = Float3( 0, -1, 0 );
    pVerts[ 4 + 8 * 2 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;


    pVerts[ 3 + 8 * 2 ].Position = Float3( mins.X, maxs.Y, maxs.Z ); // 3
    pVerts[ 3 + 8 * 2 ].Normal = Float3( 0, 1, 0 );
    pVerts[ 3 + 8 * 2 ].TexCoord = Float2( 0, 1 )*_TexCoordScale;

    pVerts[ 2 + 8 * 2 ].Position = Float3( maxs.X, maxs.Y, maxs.Z ); // 2
    pVerts[ 2 + 8 * 2 ].Normal = Float3( 0, 1, 0 );
    pVerts[ 2 + 8 * 2 ].TexCoord = Float2( 1, 1 )*_TexCoordScale;

    pVerts[ 7 + 8 * 2 ].Position = Float3( maxs.X, maxs.Y, mins.Z ); // 7
    pVerts[ 7 + 8 * 2 ].Normal = Float3( 0, 1, 0 );
    pVerts[ 7 + 8 * 2 ].TexCoord = Float2( 1, 0 )*_TexCoordScale;

    pVerts[ 6 + 8 * 2 ].Position = Float3( mins.X, maxs.Y, mins.Z ); // 6
    pVerts[ 6 + 8 * 2 ].Normal = Float3( 0, 1, 0 );
    pVerts[ 6 + 8 * 2 ].TexCoord = Float2( 0, 0 )*_TexCoordScale;

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );
}

void CreateSphereMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, float _Radius, float _TexCoordScale, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {
    int x, y;
    float verticalAngle, horizontalAngle;
    unsigned int quad[ 4 ];

    _NumVerticalSubdivs = Math::Max( _NumVerticalSubdivs, 4 );
    _NumHorizontalSubdivs = Math::Max( _NumHorizontalSubdivs, 4 );

    _Vertices.ResizeInvalidate( ( _NumHorizontalSubdivs + 1 )*( _NumVerticalSubdivs + 1 ) );
    _Indices.ResizeInvalidate( _NumHorizontalSubdivs * _NumVerticalSubdivs * 6 );

    _Bounds.Mins.X = _Bounds.Mins.Y = _Bounds.Mins.Z = -_Radius;
    _Bounds.Maxs.X = _Bounds.Maxs.Y = _Bounds.Maxs.Z = _Radius;

    SMeshVertex * pVert = _Vertices.ToPtr();

    const float verticalStep = Math::_PI / _NumVerticalSubdivs;
    const float horizontalStep = Math::_2PI / _NumHorizontalSubdivs;
    const float verticalScale = 1.0f / _NumVerticalSubdivs;
    const float horizontalScale = 1.0f / _NumHorizontalSubdivs;

    for ( y = 0, verticalAngle = -Math::_HALF_PI; y <= _NumVerticalSubdivs; y++ ) {
        float h, r;
        Math::RadSinCos( verticalAngle, h, r );
        const float scaledH = h * _Radius;
        const float scaledR = r * _Radius;
        for ( x = 0, horizontalAngle = 0; x <= _NumHorizontalSubdivs; x++ ) {
            float s, c;
            Math::RadSinCos( horizontalAngle, s, c );
            pVert->Position = Float3( scaledR*c, scaledH, scaledR*s );
            pVert->TexCoord = Float2( 1.0f - static_cast< float >( x ) * horizontalScale, 1.0f - static_cast< float >( y ) * verticalScale ) * _TexCoordScale;
            pVert->Normal.X = r*c;
            pVert->Normal.Y = h;
            pVert->Normal.Z = r*s;
            pVert++;
            horizontalAngle += horizontalStep;
        }
        verticalAngle += verticalStep;
    }

    unsigned int * pIndices = _Indices.ToPtr();
    for ( y = 0; y < _NumVerticalSubdivs; y++ ) {
        const int y2 = y + 1;
        for ( x = 0; x < _NumHorizontalSubdivs; x++ ) {
            const int x2 = x + 1;

            quad[ 0 ] = y  * ( _NumHorizontalSubdivs + 1 ) + x;
            quad[ 1 ] = y2 * ( _NumHorizontalSubdivs + 1 ) + x;
            quad[ 2 ] = y2 * ( _NumHorizontalSubdivs + 1 ) + x2;
            quad[ 3 ] = y  * ( _NumHorizontalSubdivs + 1 ) + x2;

            *pIndices++ = quad[ 0 ];
            *pIndices++ = quad[ 1 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 3 ];
            *pIndices++ = quad[ 0 ];
        }
    }

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );
}

void CreatePlaneMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, float _Width, float _Height, float _TexCoordScale ) {
    _Vertices.ResizeInvalidate( 4 );
    _Indices.ResizeInvalidate( 6 );

    const float halfWidth = _Width * 0.5f;
    const float halfHeight = _Height * 0.5f;

    const SMeshVertex Verts[ 4 ] = {
        { Float3( -halfWidth,0,-halfHeight ), Float2( 0,0 ), Float3( 0,0,1 ), 1.0f, Float3( 0,1,0 ) },
        { Float3( -halfWidth,0,halfHeight ), Float2( 0,_TexCoordScale ), Float3( 0,0,1 ), 1.0f, Float3( 0,1,0 ) },
        { Float3( halfWidth,0,halfHeight ), Float2( _TexCoordScale,_TexCoordScale ), Float3( 0,0,1 ), 1.0f, Float3( 0,1,0 ) },
        { Float3( halfWidth,0,-halfHeight ), Float2( _TexCoordScale,0 ), Float3( 0,0,1 ), 1.0f, Float3( 0,1,0 ) }
    };

    memcpy( _Vertices.ToPtr(), &Verts, 4 * sizeof( SMeshVertex ) );

    constexpr unsigned int indices[ 6 ] = { 0,1,2,2,3,0 };
    memcpy( _Indices.ToPtr(), &indices, sizeof( indices ) );

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );

    _Bounds.Mins.X = -halfWidth;
    _Bounds.Mins.Y = 0.0f;
    _Bounds.Mins.Z = -halfHeight;
    _Bounds.Maxs.X = halfWidth;
    _Bounds.Maxs.Y = 0.0f;
    _Bounds.Maxs.Z = halfHeight;
}

void CreatePatchMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds,
    Float3 const & Corner00, Float3 const & Corner10, Float3 const & Corner01, Float3 const & Corner11,
    float _TexCoordScale, bool _TwoSided, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {

    _NumVerticalSubdivs = Math::Max( _NumVerticalSubdivs, 2 );
    _NumHorizontalSubdivs = Math::Max( _NumHorizontalSubdivs, 2 );

    const float scaleX = 1.0f / ( float )( _NumHorizontalSubdivs - 1 );
    const float scaleY = 1.0f / ( float )( _NumVerticalSubdivs - 1 );

    const int vertexCount = _NumHorizontalSubdivs * _NumVerticalSubdivs;
    const int indexCount = ( _NumHorizontalSubdivs - 1 ) * ( _NumVerticalSubdivs - 1 ) * 6;

    Float3 normal = ( Corner10 - Corner00 ).Cross( Corner01 - Corner00 ).Normalized();

    _Vertices.ResizeInvalidate( _TwoSided ? vertexCount << 1 : vertexCount );
    _Indices.ResizeInvalidate( _TwoSided ? indexCount << 1 : indexCount );

    SMeshVertex * pVert = _Vertices.ToPtr();
    unsigned int * pIndices = _Indices.ToPtr();

    for ( int y = 0; y < _NumVerticalSubdivs; ++y ) {
        const float lerpY = y * scaleY;
        const Float3 py0 = Corner00.Lerp( Corner01, lerpY );
        const Float3 py1 = Corner10.Lerp( Corner11, lerpY );
        const float ty = lerpY * _TexCoordScale;

        for ( int x = 0; x < _NumHorizontalSubdivs; ++x ) {
            const float lerpX = x * scaleX;

            pVert->Position = py0.Lerp( py1, lerpX );
            pVert->TexCoord.X = lerpX * _TexCoordScale;
            pVert->TexCoord.Y = ty;
            pVert->Normal = normal;

            ++pVert;
        }
    }

    if ( _TwoSided ) {
        normal = -normal;

        for ( int y = 0; y < _NumVerticalSubdivs; ++y ) {
            const float lerpY = y * scaleY;
            const Float3 py0 = Corner00.Lerp( Corner01, lerpY );
            const Float3 py1 = Corner10.Lerp( Corner11, lerpY );
            const float ty = lerpY * _TexCoordScale;

            for ( int x = 0; x < _NumHorizontalSubdivs; ++x ) {
                const float lerpX = x * scaleX;

                pVert->Position = py0.Lerp( py1, lerpX );
                pVert->TexCoord.X = lerpX * _TexCoordScale;
                pVert->TexCoord.Y = ty;
                pVert->Normal = normal;

                ++pVert;
            }
        }
    }

    for ( int y = 0; y < _NumVerticalSubdivs; ++y ) {

        const int index0 = y*_NumHorizontalSubdivs;
        const int index1 = ( y + 1 )*_NumHorizontalSubdivs;

        for ( int x = 0; x < _NumHorizontalSubdivs; ++x ) {
            const int quad00 = index0 + x;
            const int quad01 = index0 + x + 1;
            const int quad10 = index1 + x;
            const int quad11 = index1 + x + 1;

            if ( ( x + 1 ) < _NumHorizontalSubdivs && ( y + 1 ) < _NumVerticalSubdivs ) {
                *pIndices++ = quad00;
                *pIndices++ = quad10;
                *pIndices++ = quad11;
                *pIndices++ = quad11;
                *pIndices++ = quad01;
                *pIndices++ = quad00;
            }
        }
    }

    if ( _TwoSided ) {
        for ( int y = 0; y < _NumVerticalSubdivs; ++y ) {

            const int index0 = vertexCount + y*_NumHorizontalSubdivs;
            const int index1 = vertexCount + ( y + 1 )*_NumHorizontalSubdivs;

            for ( int x = 0; x < _NumHorizontalSubdivs; ++x ) {
                const int quad00 = index0 + x;
                const int quad01 = index0 + x + 1;
                const int quad10 = index1 + x;
                const int quad11 = index1 + x + 1;

                if ( ( x + 1 ) < _NumHorizontalSubdivs && ( y + 1 ) < _NumVerticalSubdivs ) {
                    *pIndices++ = quad00;
                    *pIndices++ = quad01;
                    *pIndices++ = quad11;
                    *pIndices++ = quad11;
                    *pIndices++ = quad10;
                    *pIndices++ = quad00;
                }
            }
        }
    }

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );

    _Bounds.Clear();
    _Bounds.AddPoint( Corner00 );
    _Bounds.AddPoint( Corner01 );
    _Bounds.AddPoint( Corner10 );
    _Bounds.AddPoint( Corner11 );
}

void CreateCylinderMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, float _Radius, float _Height, float _TexCoordScale, int _NumSubdivs ) {
    int i, j;
    float angle;
    unsigned int quad[ 4 ];

    _NumSubdivs = Math::Max( _NumSubdivs, 4 );

    const float invSubdivs = 1.0f / _NumSubdivs;
    const float angleStep = Math::_2PI * invSubdivs;
    const float halfHeight = _Height * 0.5f;

    _Vertices.ResizeInvalidate( 6 * ( _NumSubdivs + 1 ) );
    _Indices.ResizeInvalidate( 3 * _NumSubdivs * 6 );

    _Bounds.Mins.X = -_Radius;
    _Bounds.Mins.Z = -_Radius;
    _Bounds.Mins.Y = -halfHeight;

    _Bounds.Maxs.X = _Radius;
    _Bounds.Maxs.Z = _Radius;
    _Bounds.Maxs.Y = halfHeight;

    SMeshVertex * pVerts = _Vertices.ToPtr();

    int firstVertex = 0;

    for ( j = 0; j <= _NumSubdivs; j++ ) {
        pVerts[ firstVertex + j ].Position = Float3( 0.0f, -halfHeight, 0.0f );
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 0.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, -1.0f, 0 );
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, -halfHeight, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 1.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, -1.0f, 0 );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, -halfHeight, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( 1.0f - j * invSubdivs, 1.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( c, 0.0f, s );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, halfHeight, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( 1.0f - j * invSubdivs, 0.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( c, 0.0f, s );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, halfHeight, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 0.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, 1.0f, 0 );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0; j <= _NumSubdivs; j++ ) {
        pVerts[ firstVertex + j ].Position = Float3( 0.0f, halfHeight, 0.0f );
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 1.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, 1.0f, 0 );
    }
    firstVertex += _NumSubdivs + 1;

    // generate indices

    unsigned int * pIndices = _Indices.ToPtr();

    firstVertex = 0;
    for ( i = 0; i < 3; i++ ) {
        for ( j = 0; j < _NumSubdivs; j++ ) {
            quad[ 3 ] = firstVertex + j;
            quad[ 2 ] = firstVertex + j + 1;
            quad[ 1 ] = firstVertex + j + 1 + ( _NumSubdivs + 1 );
            quad[ 0 ] = firstVertex + j + ( _NumSubdivs + 1 );

            *pIndices++ = quad[ 0 ];
            *pIndices++ = quad[ 1 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 3 ];
            *pIndices++ = quad[ 0 ];
        }
        firstVertex += ( _NumSubdivs + 1 ) * 2;
    }

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );
}

void CreateConeMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, float _Radius, float _Height, float _TexCoordScale, int _NumSubdivs ) {
    int i, j;
    float angle;
    unsigned int quad[ 4 ];

    _NumSubdivs = Math::Max( _NumSubdivs, 4 );

    const float invSubdivs = 1.0f / _NumSubdivs;
    const float angleStep = Math::_2PI * invSubdivs;

    _Vertices.ResizeInvalidate( 4 * ( _NumSubdivs + 1 ) );
    _Indices.ResizeInvalidate( 2 * _NumSubdivs * 6 );

    _Bounds.Mins.X = -_Radius;
    _Bounds.Mins.Z = -_Radius;
    _Bounds.Mins.Y = 0;

    _Bounds.Maxs.X = _Radius;
    _Bounds.Maxs.Z = _Radius;
    _Bounds.Maxs.Y = _Height;

    SMeshVertex * pVerts = _Vertices.ToPtr();

    int firstVertex = 0;

    for ( j = 0; j <= _NumSubdivs; j++ ) {
        pVerts[ firstVertex + j ].Position = Float3::Zero();
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 0.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, -1.0f, 0 );
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, 0.0f, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( j * invSubdivs, 1.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( 0, -1.0f, 0 );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( _Radius*c, 0.0f, _Radius*s );
        pVerts[ firstVertex + j ].TexCoord = Float2( 1.0f - j * invSubdivs, 1.0f ) * _TexCoordScale;
        pVerts[ firstVertex + j ].Normal = Float3( c, 0.0f, s );
        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    Float3 vx;
    Float3 vy( 0, _Height, 0);
    Float3 v;
    for ( j = 0, angle = 0; j <= _NumSubdivs; j++ ) {
        float s, c;
        Math::RadSinCos( angle, s, c );
        pVerts[ firstVertex + j ].Position = Float3( 0, _Height, 0 );
        pVerts[ firstVertex + j ].TexCoord = Float2( 1.0f - j * invSubdivs, 0.0f ) * _TexCoordScale;

        vx = Float3( c, 0.0f, s );
        v = vy - vx;
        pVerts[ firstVertex + j ].Normal = Math::Cross( Math::Cross( v, vx ), v ).Normalized();

        angle += angleStep;
    }
    firstVertex += _NumSubdivs + 1;

    AN_Assert( firstVertex == _Vertices.Size() );

    // generate indices

    unsigned int * pIndices = _Indices.ToPtr();

    firstVertex = 0;
    for ( i = 0; i < 2; i++ ) {
        for ( j = 0; j < _NumSubdivs; j++ ) {
            quad[ 3 ] = firstVertex + j;
            quad[ 2 ] = firstVertex + j + 1;
            quad[ 1 ] = firstVertex + j + 1 + ( _NumSubdivs + 1 );
            quad[ 0 ] = firstVertex + j + ( _NumSubdivs + 1 );

            *pIndices++ = quad[ 0 ];
            *pIndices++ = quad[ 1 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 3 ];
            *pIndices++ = quad[ 0 ];
        }
        firstVertex += ( _NumSubdivs + 1 ) * 2;
    }

    AN_Assert( pIndices == _Indices.ToPtr() + _Vertices.Size() );

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );
}

void CreateCapsuleMesh( TPodArray< SMeshVertex > & _Vertices, TPodArray< unsigned int > & _Indices, BvAxisAlignedBox & _Bounds, float _Radius, float _Height, float _TexCoordScale, int _NumVerticalSubdivs, int _NumHorizontalSubdivs ) {
    int x, y, tcY;
    float verticalAngle, horizontalAngle;
    const float halfHeight = _Height * 0.5f;
    unsigned int quad[ 4 ];

    _NumVerticalSubdivs = Math::Max( _NumVerticalSubdivs, 4 );
    _NumHorizontalSubdivs = Math::Max( _NumHorizontalSubdivs, 4 );

    const int halfVerticalSubdivs = _NumVerticalSubdivs >> 1;

    _Vertices.ResizeInvalidate( ( _NumHorizontalSubdivs + 1 ) * ( _NumVerticalSubdivs + 1 ) * 2 );
    _Indices.ResizeInvalidate( _NumHorizontalSubdivs * ( _NumVerticalSubdivs + 1 ) * 6 );

    _Bounds.Mins.X = _Bounds.Mins.Z = -_Radius;
    _Bounds.Mins.Y =  -_Radius - halfHeight;
    _Bounds.Maxs.X = _Bounds.Maxs.Z = _Radius;
    _Bounds.Maxs.Y = _Radius + halfHeight;

    SMeshVertex * pVert = _Vertices.ToPtr();

    const float verticalStep = Math::_PI / _NumVerticalSubdivs;
    const float horizontalStep = Math::_2PI / _NumHorizontalSubdivs;
    const float verticalScale = 1.0f / ( _NumVerticalSubdivs + 1 );
    const float horizontalScale = 1.0f / _NumHorizontalSubdivs;

    tcY = 0;

    for ( y = 0, verticalAngle = -Math::_HALF_PI; y <= halfVerticalSubdivs; y++, tcY++ ) {
        float h, r;
        Math::RadSinCos( verticalAngle, h, r );
        const float scaledH = h * _Radius;
        const float scaledR = r * _Radius;
        const float posY = scaledH - halfHeight;
        for ( x = 0, horizontalAngle = 0; x <= _NumHorizontalSubdivs; x++ ) {
            float s, c;
            Math::RadSinCos( horizontalAngle, s, c );
            pVert->Position.X = scaledR * c;
            pVert->Position.Y = posY;
            pVert->Position.Z = scaledR * s;
            pVert->TexCoord.X = ( 1.0f - static_cast< float >( x ) * horizontalScale ) * _TexCoordScale;
            pVert->TexCoord.Y = ( 1.0f - static_cast< float >( tcY ) * verticalScale ) * _TexCoordScale;
            pVert->Normal.X = r * c;
            pVert->Normal.Y = h;
            pVert->Normal.Z = r * s;
            pVert++;
            horizontalAngle += horizontalStep;
        }
        verticalAngle += verticalStep;
    }

    for ( y = 0, verticalAngle = 0; y <= halfVerticalSubdivs; y++, tcY++ ) {
        float h, r;
        Math::RadSinCos( verticalAngle, h, r );
        const float scaledH = h * _Radius;
        const float scaledR = r * _Radius;
        const float posY = scaledH + halfHeight;
        for ( x = 0, horizontalAngle = 0; x <= _NumHorizontalSubdivs; x++ ) {
            float s, c;
            Math::RadSinCos( horizontalAngle, s, c );
            pVert->Position.X = scaledR * c;
            pVert->Position.Y = posY;
            pVert->Position.Z = scaledR * s;
            pVert->TexCoord.X = ( 1.0f - static_cast< float >( x ) * horizontalScale ) * _TexCoordScale;
            pVert->TexCoord.Y = ( 1.0f - static_cast< float >( tcY ) * verticalScale ) * _TexCoordScale;
            pVert->Normal.X = r * c;
            pVert->Normal.Y = h;
            pVert->Normal.Z = r * s;
            pVert++;
            horizontalAngle += horizontalStep;
        }
        verticalAngle += verticalStep;
    }

    unsigned int * pIndices = _Indices.ToPtr();
    for ( y = 0; y <= _NumVerticalSubdivs; y++ ) {
        const int y2 = y + 1;
        for ( x = 0; x < _NumHorizontalSubdivs; x++ ) {
            const int x2 = x + 1;

            quad[ 0 ] = y  * ( _NumHorizontalSubdivs + 1 ) + x;
            quad[ 1 ] = y2 * ( _NumHorizontalSubdivs + 1 ) + x;
            quad[ 2 ] = y2 * ( _NumHorizontalSubdivs + 1 ) + x2;
            quad[ 3 ] = y  * ( _NumHorizontalSubdivs + 1 ) + x2;

            *pIndices++ = quad[ 0 ];
            *pIndices++ = quad[ 1 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 2 ];
            *pIndices++ = quad[ 3 ];
            *pIndices++ = quad[ 0 ];
        }
    }

    CalcTangentSpace( _Vertices.ToPtr(), _Vertices.Size(), _Indices.ToPtr(), _Indices.Size() );
}

struct SPrimitiveBounds {
    BvAxisAlignedBox Bounds;
    int PrimitiveIndex;
};

struct SBestSplitResult {
    int Axis;
    int PrimitiveIndex;
};

struct SAABBTreeBuild {
    TPodArray< BvAxisAlignedBox > RightBounds;
    TPodArray< SPrimitiveBounds > Primitives[3];
};

static void CalcNodeBounds( SPrimitiveBounds const * _Primitives, int _PrimCount, BvAxisAlignedBox & _Bounds ) {
    AN_Assert( _PrimCount > 0 );

    SPrimitiveBounds const * primitive = _Primitives;

    _Bounds = primitive->Bounds;

    primitive++;

    for ( ; primitive < &_Primitives[_PrimCount]; primitive++ ) {
        _Bounds.AddAABB( primitive->Bounds );
    }
}

static float CalcAABBVolume( BvAxisAlignedBox const & _Bounds ) {
    Float3 extents = _Bounds.Maxs - _Bounds.Mins;
    return extents.X * extents.Y * extents.Z;
}

static SBestSplitResult FindBestSplitPrimitive( SAABBTreeBuild & _Build, int _Axis, int _FirstPrimitive, int _PrimCount ) {
    struct CompareBoundsMax {
        CompareBoundsMax( const int _Axis ) : Axis( _Axis ) {}
        bool operator()( SPrimitiveBounds const & a, SPrimitiveBounds const & b ) const {
            return ( a.Bounds.Maxs[ Axis ] < b.Bounds.Maxs[ Axis ] );
        }
        const int Axis;
    };

    SPrimitiveBounds * primitives[ 3 ] = {
        _Build.Primitives[ 0 ].ToPtr() + _FirstPrimitive,
        _Build.Primitives[ 1 ].ToPtr() + _FirstPrimitive,
        _Build.Primitives[ 2 ].ToPtr() + _FirstPrimitive
    };

    for ( int i = 0; i < 3; i++ ) {
        if ( i != _Axis ) {
            memcpy( primitives[ i ], primitives[ _Axis ], sizeof( SPrimitiveBounds ) * _PrimCount );
        }
    }

    BvAxisAlignedBox right;
    BvAxisAlignedBox left;

    SBestSplitResult result;
    result.Axis = -1;

    float bestSAH = Float::MaxValue(); // Surface area heuristic

    const float emptyCost = 1.0f;

    for ( int axis = 0; axis < 3; axis++ ) {
        SPrimitiveBounds * primBounds = primitives[ axis ];

        StdSort( primBounds, primBounds + _PrimCount, CompareBoundsMax( axis ) );

        right.Clear();
        for ( size_t i = _PrimCount - 1; i > 0; i-- ) {
            right.AddAABB( primBounds[ i ].Bounds );
            _Build.RightBounds[ i - 1 ] = right;
        }

        left.Clear();
        for ( size_t i = 1; i < _PrimCount; i++ ) {
            left.AddAABB( primBounds[ i - 1 ].Bounds );

            float sah = emptyCost + CalcAABBVolume( left ) * i + CalcAABBVolume( _Build.RightBounds[ i - 1 ] ) * ( _PrimCount - i );
            if ( bestSAH > sah ) {
                bestSAH = sah;
                result.Axis = axis;
                result.PrimitiveIndex = i;
            }
        }
    }

    AN_Assert( ( result.Axis != -1 ) && ( bestSAH < Float::MaxValue() ) );

    return result;
}

void ATreeAABB::Subdivide( SAABBTreeBuild & _Build, int _Axis, int _FirstPrimitive, int _MaxPrimitive, unsigned int _PrimitivesPerLeaf,
    int & _PrimitiveIndex, const unsigned int * _Indices )
{
    int primCount = _MaxPrimitive - _FirstPrimitive;
    int curNodeInex = Nodes.Size();

    SPrimitiveBounds * pPrimitives = _Build.Primitives[_Axis].ToPtr() + _FirstPrimitive;

    SNodeAABB & node = Nodes.Append();

    CalcNodeBounds( pPrimitives, primCount, node.Bounds );

    if ( primCount <= _PrimitivesPerLeaf ) {
        // Leaf

        node.Index = _PrimitiveIndex;
        node.PrimitiveCount = primCount;

        for ( int i = 0; i < primCount; ++i ) {
            Indirection[ _PrimitiveIndex + i ] = pPrimitives[ i ].PrimitiveIndex * 3;
        }

        _PrimitiveIndex += primCount;

    } else {
        // Node
        SBestSplitResult s = FindBestSplitPrimitive( _Build, _Axis, _FirstPrimitive, primCount );

        int mid = _FirstPrimitive + s.PrimitiveIndex;

        Subdivide( _Build, s.Axis, _FirstPrimitive, mid, _PrimitivesPerLeaf, _PrimitiveIndex, _Indices );
        Subdivide( _Build, s.Axis, mid, _MaxPrimitive, _PrimitivesPerLeaf, _PrimitiveIndex, _Indices );

        int nextNode = Nodes.Size() - curNodeInex;
        node.Index = -nextNode;
    }
}

ATreeAABB::ATreeAABB() {
    BoundingBox.Clear();
}

void ATreeAABB::Initialize( SMeshVertex const * _Vertices, unsigned int const * _Indices, unsigned int _IndexCount, int _BaseVertex, unsigned int _PrimitivesPerLeaf ) {
    Purge();

    _PrimitivesPerLeaf = Math::Max( _PrimitivesPerLeaf, 16u );

    int primCount = _IndexCount / 3;

    int numLeafs = ( primCount + _PrimitivesPerLeaf - 1 ) / _PrimitivesPerLeaf;

    Nodes.Clear();
    Nodes.ReserveInvalidate( numLeafs * 4 );

    Indirection.ResizeInvalidate( primCount );

    SAABBTreeBuild build;
    build.RightBounds.ResizeInvalidate( primCount );
    build.Primitives[0].ResizeInvalidate( primCount );
    build.Primitives[1].ResizeInvalidate( primCount );
    build.Primitives[2].ResizeInvalidate( primCount );

    int primitiveIndex = 0;
    for ( unsigned int i = 0; i < _IndexCount; i += 3, primitiveIndex++ ) {
        const size_t i0 = _Indices[ i ];
        const size_t i1 = _Indices[ i + 1 ];
        const size_t i2 = _Indices[ i + 2 ];

        Float3 const & v0 = _Vertices[ _BaseVertex + i0 ].Position;
        Float3 const & v1 = _Vertices[ _BaseVertex + i1 ].Position;
        Float3 const & v2 = _Vertices[ _BaseVertex + i2 ].Position;

        SPrimitiveBounds & primitive = build.Primitives[ 0 ][ primitiveIndex ];
        primitive.PrimitiveIndex = primitiveIndex;

        primitive.Bounds.Mins.X = Math::Min( v0.X, v1.X, v2.X );
        primitive.Bounds.Mins.Y = Math::Min( v0.Y, v1.Y, v2.Y );
        primitive.Bounds.Mins.Z = Math::Min( v0.Z, v1.Z, v2.Z );

        primitive.Bounds.Maxs.X = Math::Max( v0.X, v1.X, v2.X );
        primitive.Bounds.Maxs.Y = Math::Max( v0.Y, v1.Y, v2.Y );
        primitive.Bounds.Maxs.Z = Math::Max( v0.Z, v1.Z, v2.Z );
    }

    primitiveIndex = 0;
    Subdivide( build, 0, 0, primCount, _PrimitivesPerLeaf, primitiveIndex, _Indices );
    Nodes.ShrinkToFit();

    BoundingBox = Nodes[0].Bounds;

    //size_t sz = Nodes.Size() * sizeof( Nodes[ 0 ] )
    //    + Indirection.Size() * sizeof( Indirection[ 0 ] )
    //    + sizeof( *this );

    //size_t sz2 = Nodes.Reserved() * sizeof( Nodes[0] )
    //    + Indirection.Reserved() * sizeof( Indirection[0] )
    //    + sizeof( *this );
    
    //GLogger.Printf( "AABBTree memory usage: %i  %i\n", sz, sz2 );
}

void ATreeAABB::Purge() {
    Nodes.Free();
    Indirection.Free();
}

int ATreeAABB::MarkBoxOverlappingLeafs( BvAxisAlignedBox const & _Bounds, unsigned int * _MarkLeafs, int _MaxLeafs ) const {
    if ( !_MaxLeafs ) {
        return 0;
    }
    int n = 0;
    for ( int nodeIndex = 0; nodeIndex < Nodes.Size(); ) {
        SNodeAABB const * node = &Nodes[ nodeIndex ];

        const bool bOverlap = BvBoxOverlapBox( _Bounds, node->Bounds );
        const bool bLeaf = node->IsLeaf();

        if ( bLeaf && bOverlap ) {
            _MarkLeafs[ n++ ] = nodeIndex;
            if ( n == _MaxLeafs ) {
                return n;
            }
        }
        nodeIndex += ( bOverlap || bLeaf ) ? 1 : ( -node->Index );
    }
    return n;
}

int ATreeAABB::MarkRayOverlappingLeafs( Float3 const & _RayStart, Float3 const & _RayEnd, unsigned int * _MarkLeafs, int _MaxLeafs ) const {
    if ( !_MaxLeafs ) {
        return 0;
    }

    Float3 rayDir = _RayEnd - _RayStart;
    Float3 invRayDir;

    float rayLength = rayDir.Length();

    if ( rayLength < 0.0001f ) {
        return 0;
    }

    //rayDir = rayDir / rayLength;

    invRayDir.X = 1.0f / rayDir.X;
    invRayDir.Y = 1.0f / rayDir.Y;
    invRayDir.Z = 1.0f / rayDir.Z;

    float hitMin, hitMax;

    int n = 0;
    for ( int nodeIndex = 0; nodeIndex < Nodes.Size(); ) {
        SNodeAABB const * node = &Nodes[ nodeIndex ];

        const bool bOverlap = BvRayIntersectBox( _RayStart, invRayDir, node->Bounds, hitMin, hitMax ) && hitMin <= 1.0f;// rayLength;
        const bool bLeaf = node->IsLeaf();

        if ( bLeaf && bOverlap ) {
            _MarkLeafs[ n++ ] = nodeIndex;
            if ( n == _MaxLeafs ) {
                return n;
            }
        }
        nodeIndex += ( bOverlap || bLeaf ) ? 1 : ( -node->Index );
    }

    return n;
}

void ATreeAABB::Read( IStreamBase & _Stream ) {
    _Stream.ReadArrayOfStructs( Nodes );
    _Stream.ReadArrayUInt32( Indirection );
    _Stream.ReadObject( BoundingBox );
}

void ATreeAABB::Write( IStreamBase & _Stream ) const {
    _Stream.WriteArrayOfStructs( Nodes );
    _Stream.WriteArrayUInt32( Indirection );
    _Stream.WriteObject( BoundingBox );
}
