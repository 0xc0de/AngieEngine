/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2021 Alexander Samusev.

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

#include <World/Public/Base/Resource.h>
#include <Core/Public/BV/BvAxisAlignedBox.h>
#include <Core/Public/Guid.h>

/**

SJoint

Joint properties

*/
struct SJoint
{
    /** Parent node index. For root = -1 */
    int32_t  Parent;

    /** Joint local transform */
    Float3x4 LocalTransform;

    /** Joint name */
    char     Name[64];

    void Write( IBinaryStream & _Stream ) const {
        _Stream.WriteInt32( Parent );
        _Stream.WriteObject( LocalTransform );
        _Stream.WriteCString( Name );
    }

    void Read( IBinaryStream & _Stream ) {
        Parent = _Stream.ReadInt32();
        _Stream.ReadObject( LocalTransform );
        _Stream.ReadCString( Name, sizeof( Name ) );
    }
};


/**

ASkeleton

Skeleton structure

*/
class ASkeleton : public AResource {
    AN_CLASS( ASkeleton, AResource )

public:
    enum { MAX_JOINTS = 256 };

    void Initialize( SJoint * _Joints, int _JointsCount, BvAxisAlignedBox const & _BindposeBounds );

    void Purge();

    int FindJoint( const char * _Name ) const;

    TPodVector< SJoint > const & GetJoints() const { return Joints; }

    BvAxisAlignedBox const & GetBindposeBounds() const { return BindposeBounds; }

protected:
    ASkeleton();
    ~ASkeleton();

    /** Load resource from file */
    bool LoadResource( IBinaryStream & Stream ) override;

    /** Create internal resource */
    void LoadInternalResource( const char * _Path ) override;

    const char * GetDefaultResourcePath() const override { return "/Default/Skeleton/Default"; }

private:
    TPodVector< SJoint > Joints;
    BvAxisAlignedBox BindposeBounds;
};
