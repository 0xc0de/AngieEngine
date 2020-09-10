/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2020 Alexander Samusev.

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

#include "GPUSync.h"

using namespace RenderCore;

void AGPUSync::Wait()
{
    if ( !bCreated ) {
        bCreated = true;

        byte data[2*2*4];
        Core::Memset( data, 128, sizeof( data ) );

        GDevice->CreateTexture( MakeTexture( TEXTURE_FORMAT_RGBA8,
                                             STextureResolution2D( 2, 2 ),
                                             STextureMultisampleInfo(),
                                             STextureSwizzle(),
                                             2 ),
                                &Texture );

        Texture->Write( 0, FORMAT_UBYTE4, sizeof( data ), 1, data );


        GDevice->CreateTexture( MakeTexture( TEXTURE_FORMAT_RGBA8,
                                             STextureResolution2D( 1, 1 ),
                                             STextureMultisampleInfo(),
                                             STextureSwizzle(),
                                             1 ),
                                &Staging );
    }
    else {
        TextureCopy copy = {};

        copy.SrcRect.Offset.Lod = 1;
        copy.SrcRect.Offset.X = 0;
        copy.SrcRect.Offset.Y = 0;
        copy.SrcRect.Offset.Z = 0;
        copy.SrcRect.Dimension.X = 1;
        copy.SrcRect.Dimension.Y = 1;
        copy.SrcRect.Dimension.Z = 1;
        copy.DstOffset.Lod = 0;
        copy.DstOffset.X = 0;
        copy.DstOffset.Y = 0;
        copy.DstOffset.Z = 0;

        rcmd->CopyTextureRect( Texture, Staging, 1, &copy );

        byte data[4];
        Staging->Read( 0, FORMAT_UBYTE4, 4, 1, data );
    }
}

void AGPUSync::SetEvent()
{
    if ( bCreated ) {
        Texture->GenerateLods();
    }
}

void AGPUSync::Release()
{
    if ( bCreated ) {
        bCreated = false;

        Texture.Reset();
        Staging.Reset();
    }
}
