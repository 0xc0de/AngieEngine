/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2018 Alexander Samusev.

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

#include "BaseObject.h"
#include <Engine/Core/Public/BV/BvAxisAlignedBox.h>

class btCollisionShape;
class btCompoundShape;
class btBvhTriangleMeshShape;
class FCollisionBodyComposition;
struct FSubpart;

class FCollisionBody : public FBaseObject {
    AN_CLASS( FCollisionBody, FBaseObject )

    friend btCompoundShape * CreateCollisionShape( FCollisionBodyComposition const & BodyComposition/*, Float3 const & _Scale*/ );

public:
    Float3 Position;
    Quat Rotation;
    float Margin;

protected:
    FCollisionBody() {
        Rotation = Quat::Identity();
        Margin = 0.01f;
    }

    // Only CreateCollisionShape can call Create()
    virtual btCollisionShape * Create() { AN_Assert( 0 ); return nullptr; }
};

class FCollisionSphere : public FCollisionBody {
    AN_CLASS( FCollisionSphere, FCollisionBody )

public:
    float Radius = 1;
    bool bProportionalScale = true;

protected:
    FCollisionSphere() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionSphereRadii : public FCollisionBody {
    AN_CLASS( FCollisionSphereRadii, FCollisionBody )

public:
    Float3 Radius = Float3(1);

protected:
    FCollisionSphereRadii() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionBox : public FCollisionBody {
    AN_CLASS( FCollisionBox, FCollisionBody )

public:
    Float3 HalfExtents = Float3(1.0f);

protected:
    FCollisionBox() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionCylinder : public FCollisionBody {
    AN_CLASS( FCollisionCylinder, FCollisionBody )

public:
    Float3 HalfExtents = Float3(1.0f);

protected:
    FCollisionCylinder() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionCone : public FCollisionBody {
    AN_CLASS( FCollisionCone, FCollisionBody )

public:
    float Radius = 1;
    float Height = 1;

protected:
    FCollisionCone() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionCapsule : public FCollisionBody {
    AN_CLASS( FCollisionCapsule, FCollisionBody )

public:
    float Radius = 1;
    float Height = 1;

protected:
    FCollisionCapsule() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionPlane : public FCollisionBody {
    AN_CLASS( FCollisionPlane, FCollisionBody )

public:
    PlaneF Plane = PlaneF( Float3(0.0f,1.0f,0.0f), 0.0f );

protected:
    FCollisionPlane() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionConvexHull : public FCollisionBody {
    AN_CLASS( FCollisionConvexHull, FCollisionBody )

public:
    TPodArray< Float3 > Vertices;

protected:
    FCollisionConvexHull() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionConvexHullData : public FBaseObject {
    AN_CLASS( FCollisionConvexHullData, FBaseObject )

public:
    TPodArray< Float3 > Vertices;

protected:
    FCollisionConvexHullData() {}
};

class FCollisionSharedConvexHull : public FCollisionBody {
    AN_CLASS( FCollisionSharedConvexHull, FCollisionBody )

public:
    TRefHolder< FCollisionConvexHullData > HullData;

protected:
    FCollisionSharedConvexHull() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionTriangleSoupData : public FBaseObject {
    AN_CLASS( FCollisionTriangleSoupData, FBaseObject )

public:
    struct FSubpart {
        int BaseVertex;
        int VertexCount;
        int FirstIndex;
        int IndexCount;
    };

    TPodArray< Float3 > Vertices;
    TPodArray< unsigned int > Indices;
    TPodArray< FSubpart > Subparts;
    BvAxisAlignedBox BoundingBox;

    void Initialize( float const * _Vertices, int _VertexStride, int _VertexCount, unsigned int const * _Indices, int _IndexCount, ::FSubpart const * _Subparts, int _SubpartsCount );

protected:
    FCollisionTriangleSoupData() {}
};

class FCollisionTriangleSoupBVHData : public FBaseObject {
    AN_CLASS( FCollisionTriangleSoupBVHData, FBaseObject )

public:
    TRefHolder< FCollisionTriangleSoupData > TrisData;

    void BuildBVH( bool bForceQuantizedAabbCompression = false );
    //void BuildQuantizedBVH();

    bool UsedQuantizedAabbCompression() const;

    btBvhTriangleMeshShape * GetData() { return Data; }

protected:
    FCollisionTriangleSoupBVHData();
    ~FCollisionTriangleSoupBVHData();

private:
    btBvhTriangleMeshShape * Data; // TODO: Try btMultimaterialTriangleMeshShape
 
    class FStridingMeshInterface * Interface;

    bool bUsedQuantizedAabbCompression;
};

class FCollisionSharedTriangleSoupBVH : public FCollisionBody {
    AN_CLASS( FCollisionSharedTriangleSoupBVH, FCollisionBody )

public:
    TRefHolder< FCollisionTriangleSoupBVHData > BvhData;

protected:
    FCollisionSharedTriangleSoupBVH() {}

private:
    btCollisionShape * Create() override;
};

class FCollisionSharedTriangleSoupGimpact : public FCollisionBody {
    AN_CLASS( FCollisionSharedTriangleSoupGimpact, FCollisionBody )

public:
    TRefHolder< FCollisionTriangleSoupData > TrisData;

protected:
    FCollisionSharedTriangleSoupGimpact();
    ~FCollisionSharedTriangleSoupGimpact();

private:
    btCollisionShape * Create() override;

    class FStridingMeshInterface * Interface;
};

// TODO:
//class FCollisionTerrain : public FCollisionBody
//etc

class FCollisionBodyComposition {
    AN_FORBID_COPY( FCollisionBodyComposition )

public:

    FCollisionBodyComposition() {}

    ~FCollisionBodyComposition() {
        Clear();
    }

    void Clear() {
        for ( FCollisionBody * body : CollisionBodies ) {
            body->RemoveRef();
        }
        CollisionBodies.Clear();
    }

    template< typename T >
    T * NewCollisionBody() {
        T * body = static_cast< T * >( T::ClassMeta().CreateInstance() );
        AddCollisionBody( body );
        return body;
    }

    void AddCollisionBody( FCollisionBody * _Body ) {
        AN_Assert( CollisionBodies.Find( _Body ) == CollisionBodies.End() );
        CollisionBodies.Append( _Body );
        _Body->AddRef();
    }

    void RemoveCollisionBody( FCollisionBody * _Body ) {
        auto it = CollisionBodies.Find( _Body );
        if ( it == CollisionBodies.End() ) {
            return;
        }
        _Body->RemoveRef();
        CollisionBodies.Erase( it );
    }

    void Duplicate( FCollisionBodyComposition & _Composition ) const {
        _Composition.Clear();
        _Composition.CollisionBodies = CollisionBodies;
        for ( FCollisionBody * body : CollisionBodies ) {
            body->AddRef();
        }
    }

    TPodArray< FCollisionBody *, 2 > CollisionBodies;
};
