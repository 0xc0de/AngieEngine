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

#include <World/Public/Resource/Texture.h>
#include <World/Public/Resource/Asset.h>
#include <Runtime/Public/ScopedTimeCheck.h>
#include <Runtime/Public/Runtime.h>
#include <Core/Public/Logger.h>
#include <Core/Public/IntrusiveLinkedListMacro.h>
#include <Core/Public/Image.h>

static const char * TextureTypeName[] =
{
    "TEXTURE_1D",
    "TEXTURE_1D_ARRAY",
    "TEXTURE_2D",
    "TEXTURE_2D_ARRAY",
    "TEXTURE_3D",
    "TEXTURE_CUBEMAP",
    "TEXTURE_CUBEMAP_ARRAY",
};

struct STextureFormatMapper
{
    RenderCore::DATA_FORMAT PixelFormatTable[256];
    RenderCore::TEXTURE_FORMAT InternalPixelFormatTable[256];

    STextureFormatMapper()
    {
        using namespace RenderCore;

        Core::ZeroMem( PixelFormatTable, sizeof( PixelFormatTable ) );
        Core::ZeroMem( InternalPixelFormatTable, sizeof( InternalPixelFormatTable ) );

        PixelFormatTable[TEXTURE_PF_R8_SNORM] = FORMAT_BYTE1;
        PixelFormatTable[TEXTURE_PF_RG8_SNORM] = FORMAT_BYTE2;
        PixelFormatTable[TEXTURE_PF_BGR8_SNORM] = FORMAT_BYTE3;
        PixelFormatTable[TEXTURE_PF_BGRA8_SNORM] = FORMAT_BYTE4;

        PixelFormatTable[TEXTURE_PF_R8_UNORM] = FORMAT_UBYTE1;
        PixelFormatTable[TEXTURE_PF_RG8_UNORM] = FORMAT_UBYTE2;
        PixelFormatTable[TEXTURE_PF_BGR8_UNORM] = FORMAT_UBYTE3;
        PixelFormatTable[TEXTURE_PF_BGRA8_UNORM] = FORMAT_UBYTE4;

        PixelFormatTable[TEXTURE_PF_BGR8_SRGB] = FORMAT_UBYTE3;
        PixelFormatTable[TEXTURE_PF_BGRA8_SRGB] = FORMAT_UBYTE4;

        PixelFormatTable[TEXTURE_PF_R16I] = FORMAT_SHORT1;
        PixelFormatTable[TEXTURE_PF_RG16I] = FORMAT_SHORT2;
        PixelFormatTable[TEXTURE_PF_BGR16I] = FORMAT_SHORT3;
        PixelFormatTable[TEXTURE_PF_BGRA16I] = FORMAT_SHORT4;

        PixelFormatTable[TEXTURE_PF_R16UI] = FORMAT_USHORT1;
        PixelFormatTable[TEXTURE_PF_RG16UI] = FORMAT_USHORT2;
        PixelFormatTable[TEXTURE_PF_BGR16UI] = FORMAT_USHORT3;
        PixelFormatTable[TEXTURE_PF_BGRA16UI] = FORMAT_USHORT4;

        PixelFormatTable[TEXTURE_PF_R32I] = FORMAT_INT1;
        PixelFormatTable[TEXTURE_PF_RG32I] = FORMAT_INT2;
        PixelFormatTable[TEXTURE_PF_BGR32I] = FORMAT_INT3;
        PixelFormatTable[TEXTURE_PF_BGRA32I] = FORMAT_INT4;

        PixelFormatTable[TEXTURE_PF_R32I] = FORMAT_UINT1;
        PixelFormatTable[TEXTURE_PF_RG32UI] = FORMAT_UINT2;
        PixelFormatTable[TEXTURE_PF_BGR32UI] = FORMAT_UINT3;
        PixelFormatTable[TEXTURE_PF_BGRA32UI] = FORMAT_UINT4;

        PixelFormatTable[TEXTURE_PF_R16F] = FORMAT_HALF1;
        PixelFormatTable[TEXTURE_PF_RG16F] = FORMAT_HALF2;
        PixelFormatTable[TEXTURE_PF_BGR16F] = FORMAT_HALF3;
        PixelFormatTable[TEXTURE_PF_BGRA16F] = FORMAT_HALF4;

        PixelFormatTable[TEXTURE_PF_R32F] = FORMAT_FLOAT1;
        PixelFormatTable[TEXTURE_PF_RG32F] = FORMAT_FLOAT2;
        PixelFormatTable[TEXTURE_PF_BGR32F] = FORMAT_FLOAT3;
        PixelFormatTable[TEXTURE_PF_BGRA32F] = FORMAT_FLOAT4;

        PixelFormatTable[TEXTURE_PF_R11F_G11F_B10F] = FORMAT_FLOAT3;

        InternalPixelFormatTable[TEXTURE_PF_R8_SNORM] = TEXTURE_FORMAT_R8_SNORM;
        InternalPixelFormatTable[TEXTURE_PF_RG8_SNORM] = TEXTURE_FORMAT_RG8_SNORM;
        InternalPixelFormatTable[TEXTURE_PF_BGR8_SNORM] = TEXTURE_FORMAT_RGB8_SNORM;
        InternalPixelFormatTable[TEXTURE_PF_BGRA8_SNORM] = TEXTURE_FORMAT_RGBA8_SNORM;

        InternalPixelFormatTable[TEXTURE_PF_R8_UNORM] = TEXTURE_FORMAT_R8;
        InternalPixelFormatTable[TEXTURE_PF_RG8_UNORM] = TEXTURE_FORMAT_RG8;
        InternalPixelFormatTable[TEXTURE_PF_BGR8_UNORM] = TEXTURE_FORMAT_RGB8;
        InternalPixelFormatTable[TEXTURE_PF_BGRA8_UNORM] = TEXTURE_FORMAT_RGBA8;

        InternalPixelFormatTable[TEXTURE_PF_BGR8_SRGB] = TEXTURE_FORMAT_SRGB8;
        InternalPixelFormatTable[TEXTURE_PF_BGRA8_SRGB] = TEXTURE_FORMAT_SRGB8_ALPHA8;

        InternalPixelFormatTable[TEXTURE_PF_R16I] = TEXTURE_FORMAT_R16I;
        InternalPixelFormatTable[TEXTURE_PF_RG16I] = TEXTURE_FORMAT_RG16I;
        InternalPixelFormatTable[TEXTURE_PF_BGR16I] = TEXTURE_FORMAT_RGB16I;
        InternalPixelFormatTable[TEXTURE_PF_BGRA16I] = TEXTURE_FORMAT_RGBA16I;

        InternalPixelFormatTable[TEXTURE_PF_R16UI] = TEXTURE_FORMAT_R16UI;
        InternalPixelFormatTable[TEXTURE_PF_RG16UI] = TEXTURE_FORMAT_RG16UI;
        InternalPixelFormatTable[TEXTURE_PF_BGR16UI] = TEXTURE_FORMAT_RGB16UI;
        InternalPixelFormatTable[TEXTURE_PF_BGRA16UI] = TEXTURE_FORMAT_RGBA16UI;

        InternalPixelFormatTable[TEXTURE_PF_R32I] = TEXTURE_FORMAT_R32I;
        InternalPixelFormatTable[TEXTURE_PF_RG32I] = TEXTURE_FORMAT_RG32I;
        InternalPixelFormatTable[TEXTURE_PF_BGR32I] = TEXTURE_FORMAT_RGB32I;
        InternalPixelFormatTable[TEXTURE_PF_BGRA32I] = TEXTURE_FORMAT_RGBA32I;

        InternalPixelFormatTable[TEXTURE_PF_R32I] = TEXTURE_FORMAT_R32UI;
        InternalPixelFormatTable[TEXTURE_PF_RG32UI] = TEXTURE_FORMAT_RG32UI;
        InternalPixelFormatTable[TEXTURE_PF_BGR32UI] = TEXTURE_FORMAT_RGB32UI;
        InternalPixelFormatTable[TEXTURE_PF_BGRA32UI] = TEXTURE_FORMAT_RGBA32UI;

        InternalPixelFormatTable[TEXTURE_PF_R16F] = TEXTURE_FORMAT_R16F;
        InternalPixelFormatTable[TEXTURE_PF_RG16F] = TEXTURE_FORMAT_RG16F;
        InternalPixelFormatTable[TEXTURE_PF_BGR16F] = TEXTURE_FORMAT_RGB16F;
        InternalPixelFormatTable[TEXTURE_PF_BGRA16F] = TEXTURE_FORMAT_RGBA16F;

        InternalPixelFormatTable[TEXTURE_PF_R32F] = TEXTURE_FORMAT_R32F;
        InternalPixelFormatTable[TEXTURE_PF_RG32F] = TEXTURE_FORMAT_RG32F;
        InternalPixelFormatTable[TEXTURE_PF_BGR32F] = TEXTURE_FORMAT_RGB32F;
        InternalPixelFormatTable[TEXTURE_PF_BGRA32F] = TEXTURE_FORMAT_RGBA32F;

        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC1_RGB]        = TEXTURE_FORMAT_COMPRESSED_BC1_RGB;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC1_SRGB]       = TEXTURE_FORMAT_COMPRESSED_BC1_SRGB;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC2_RGBA]       = TEXTURE_FORMAT_COMPRESSED_BC2_RGBA;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC2_SRGB_ALPHA] = TEXTURE_FORMAT_COMPRESSED_BC2_SRGB_ALPHA;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC3_RGBA]       = TEXTURE_FORMAT_COMPRESSED_BC3_RGBA;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC3_SRGB_ALPHA] = TEXTURE_FORMAT_COMPRESSED_BC3_SRGB_ALPHA;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC4_R]          = TEXTURE_FORMAT_COMPRESSED_BC4_R;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC4_R_SIGNED]   = TEXTURE_FORMAT_COMPRESSED_BC4_R_SIGNED;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC5_RG]         = TEXTURE_FORMAT_COMPRESSED_BC5_RG;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC5_RG_SIGNED]  = TEXTURE_FORMAT_COMPRESSED_BC5_RG_SIGNED;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC6H]           = TEXTURE_FORMAT_COMPRESSED_BC6H;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC6H_SIGNED]    = TEXTURE_FORMAT_COMPRESSED_BC6H_SIGNED;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC7_RGBA]       = TEXTURE_FORMAT_COMPRESSED_BC7_RGBA;
        InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_BC7_SRGB_ALPHA] = TEXTURE_FORMAT_COMPRESSED_BC7_SRGB_ALPHA;

        InternalPixelFormatTable[TEXTURE_PF_R11F_G11F_B10F] = TEXTURE_FORMAT_R11F_G11F_B10F;
    }
};

static STextureFormatMapper TextureFormatMapper;

AN_CLASS_META( ATexture )

ATexture::ATexture() {
}

ATexture::~ATexture() {
}

void ATexture::Purge() {

}

bool ATexture::InitializeFromImage( AImage const & _Image ) {
    if ( !_Image.GetData() ) {
        GLogger.Printf( "ATexture::InitializeFromImage: empty image data\n" );
        return false;
    }

    STexturePixelFormat pixelFormat;

    if ( !STexturePixelFormat::GetAppropriatePixelFormat( _Image.GetPixelFormat(), pixelFormat ) ) {
        return false;
    }

    Initialize2D( pixelFormat, _Image.GetNumLods(), _Image.GetWidth(), _Image.GetHeight() );

    byte * pSrc = ( byte * )_Image.GetData();
    int w, h, stride;
    int pixelSizeInBytes = pixelFormat.SizeInBytesUncompressed();

    for ( int lod = 0 ; lod < _Image.GetNumLods() ; lod++ ) {
        w = Math::Max( 1, _Image.GetWidth() >> lod );
        h = Math::Max( 1, _Image.GetHeight() >> lod );

        stride = w * h * pixelSizeInBytes;

        WriteTextureData2D( 0, 0, w, h, lod, pSrc );

        pSrc += stride;
    }

    return true;
}

bool ATexture::InitializeCubemapFromImages( TArray< AImage const *, 6 > const & _Faces ) {
    const void * faces[6];

    int width = _Faces[0]->GetWidth();

    for ( int i = 0 ; i < 6 ; i++ ) {

        if ( !_Faces[i]->GetData() ) {
            GLogger.Printf( "ATexture::InitializeCubemapFromImages: empty image data\n" );
            return false;
        }

        if ( _Faces[i]->GetWidth() != width
             || _Faces[i]->GetHeight() != width ) {
            GLogger.Printf( "ATexture::InitializeCubemapFromImages: faces with different sizes\n" );
            return false;
        }

        faces[i] = _Faces[i]->GetData();
    }

    STexturePixelFormat pixelFormat;

    if ( !STexturePixelFormat::GetAppropriatePixelFormat( _Faces[0]->GetPixelFormat(), pixelFormat ) ) {
        return false;
    }

    for ( AImage const * faceImage : _Faces ) {
        STexturePixelFormat facePF;

        if ( !STexturePixelFormat::GetAppropriatePixelFormat( faceImage->GetPixelFormat(), facePF ) ) {
            return false;
        }

        if ( pixelFormat != facePF ) {
            GLogger.Printf( "ATexture::InitializeCubemapFromImages: faces with different pixel formats\n" );
            return false;
        }
    }

    InitializeCubemap( pixelFormat, 1, width );

    for ( int face = 0 ; face < 6 ; face++ ) {
        WriteTextureDataCubemap( 0, 0, width, width, face, 0, (byte *)faces[face] );
    }

    return true;
}

void ATexture::LoadInternalResource( const char * _Path ) {
    if ( !Core::Stricmp( _Path, "/Default/Textures/White" ) ) {
        byte data[ 1 * 1 * 3 ];
        Core::Memset( data, 0xff, sizeof( data ) );

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/Black" ) ) {
        byte data[ 1 * 1 * 3 ];
        Core::ZeroMem( data, sizeof( data ) );

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/Gray" ) ) {
        byte data[ 1 * 1 * 3 ];
        Core::Memset( data, 127, sizeof( data ) );

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/BaseColorWhite" )
         || !Core::Stricmp( _Path, "/Default/Textures/Default2D" ) ) {
        byte data[ 1 * 1 * 3 ];
        Core::Memset( data, 240, sizeof( data ) );

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/BaseColorBlack" ) ) {
        byte data[ 1 * 1 * 3 ];
        Core::Memset( data, 30, sizeof( data ) );

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/Normal" ) ) {
        byte data[ 1 * 1 * 3 ];
        data[ 0 ] = 255; // z
        data[ 1 ] = 127; // y
        data[ 2 ] = 127; // x

        Initialize2D( TEXTURE_PF_BGR8_UNORM, 1, 1, 1 );
        WriteTextureData2D( 0, 0, 1, 1, 0, data );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/DefaultCubemap" ) ) {
        constexpr Float3 dirs[6] = {
            Float3( 1,0,0 ),
            Float3( -1,0,0 ),
            Float3( 0,1,0 ),
            Float3( 0,-1,0 ),
            Float3( 0,0,1 ),
            Float3( 0,0,-1 )
        };

        byte data[6][3];
        for ( int i = 0 ; i < 6 ; i++ ) {
            data[i][0] = (dirs[i].Z + 1.0f) * 127.5f;
            data[i][1] = (dirs[i].Y + 1.0f) * 127.5f;
            data[i][2] = (dirs[i].X + 1.0f) * 127.5f;
        }

        InitializeCubemap( TEXTURE_PF_BGR8_UNORM, 1, 1 );

        for ( int face = 0 ; face < 6 ; face++ ) {
            WriteTextureDataCubemap( 0, 0, 1, 1, face, 0, data[face] );
        }
        return;
    }

#if 0
    if ( !Core::Stricmp( _Path, "/Default/Textures/BlackCubemap" ) ) {
        byte data[1] = {};

        InitializeCubemap( TEXTURE_PF_R8, 1, 1 );

        for ( int face = 0 ; face < 6 ; face++ ) {
            WriteTextureDataCubemap( 0, 0, 1, 1, face, 0, data );
        }
        return;
    }
#endif

    if ( !Core::Stricmp( _Path, "/Default/Textures/LUT1" )
        || !Core::Stricmp( _Path, "/Default/Textures/Default3D" ) ) {

        constexpr SColorGradingPreset ColorGradingPreset1 = {
            Float3( 0.5f ),   // Gain
            Float3( 0.5f ),   // Gamma
            Float3( 0.5f ),   // Lift
            Float3( 1.0f ),   // Presaturation
            Float3( 0.0f ),   // Color temperature strength
            6500.0f,          // Color temperature (in K)
            0.0f              // Color temperature brightness normalization factor
        };

        InitializeColorGradingLUT( ColorGradingPreset1 );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/LUT2" ) ) {
        constexpr SColorGradingPreset ColorGradingPreset2 = {
            Float3( 0.5f ),   // Gain
            Float3( 0.5f ),   // Gamma
            Float3( 0.5f ),   // Lift
            Float3( 1.0f ),   // Presaturation
            Float3( 1.0f ),   // Color temperature strength
            3500.0f,          // Color temperature (in K)
            1.0f              // Color temperature brightness normalization factor
        };

        InitializeColorGradingLUT( ColorGradingPreset2 );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/LUT3" ) ) {
        constexpr SColorGradingPreset ColorGradingPreset3 = {
            Float3( 0.51f, 0.55f, 0.53f ), // Gain
            Float3( 0.45f, 0.57f, 0.55f ), // Gamma
            Float3( 0.5f,  0.4f,  0.6f ),  // Lift
            Float3( 1.0f,  0.9f,  0.8f ),  // Presaturation
            Float3( 1.0f,  1.0f,  1.0f ),  // Color temperature strength
            6500.0,                        // Color temperature (in K)
            0.0                            // Color temperature brightness normalization factor
        };

        InitializeColorGradingLUT( ColorGradingPreset3 );

        return;
    }

    if ( !Core::Stricmp( _Path, "/Default/Textures/LUT_Luminance" ) ) {
        byte data[ 16 ][ 16 ][ 16 ][ 3 ];
        for ( int z = 0 ; z < 16 ; z++ ) {
            for ( int y = 0 ; y < 16 ; y++ ) {
                for ( int x = 0 ; x < 16 ; x++ ) {
                    data[ z ][ y ][ x ][ 0 ] =
                    data[ z ][ y ][ x ][ 1 ] =
                    data[ z ][ y ][ x ][ 2 ] = Math::Clamp( x * ( 0.2126f / 15.0f * 255.0f ) + y * ( 0.7152f / 15.0f * 255.0f ) + z * ( 0.0722f / 15.0f * 255.0f ), 0.0f, 255.0f );
                }
            }
        }
        Initialize3D( TEXTURE_PF_BGR8_SRGB, 1, 16, 16, 16 );
        WriteArbitraryData( 0, 0, 0, 16, 16, 16, 0, data );

        return;
    }

    GLogger.Printf( "Unknown internal texture %s\n", _Path );

    LoadInternalResource( "/Default/Textures/Default2D" );
}

bool ATexture::LoadResource( IBinaryStream & Stream ) {
    const char * fn = Stream.GetFileName();

    AScopedTimeCheck ScopedTime( fn );

    AImage image;

    int i = Core::FindExt( fn );
    if ( !Core::Stricmp( &fn[i], ".jpg" )
         || !Core::Stricmp( &fn[i], ".jpeg" )
         || !Core::Stricmp( &fn[i], ".png" )
         || !Core::Stricmp( &fn[i], ".tga" )
         || !Core::Stricmp( &fn[i], ".psd" )
         || !Core::Stricmp( &fn[i], ".gif" )
         || !Core::Stricmp( &fn[i], ".hdr" )
         || !Core::Stricmp( &fn[i], ".exr" )
         || !Core::Stricmp( &fn[i], ".pic" )
         || !Core::Stricmp( &fn[i], ".pnm" )
         || !Core::Stricmp( &fn[i], ".ppm" )
         || !Core::Stricmp( &fn[i], ".pgm" ) ) {


        //AN_ASSERT( 0 ); 

        SImageMipmapConfig mipmapGen;
        mipmapGen.EdgeMode = MIPMAP_EDGE_WRAP;
        mipmapGen.Filter = MIPMAP_FILTER_MITCHELL;
        mipmapGen.bPremultipliedAlpha = false;

        if ( !Core::Stricmp( &fn[i], ".hdr" ) || !Core::Stricmp( &fn[i], ".exr" ) ) {
            if ( !image.Load( Stream, &mipmapGen, IMAGE_PF_AUTO_16F ) ) {
                return false;
            }
        } else {
            if ( !image.Load( Stream, &mipmapGen, IMAGE_PF_AUTO_GAMMA2 ) ) {
                return false;
            }
        }

        if ( !InitializeFromImage( image ) ) {
            return false;
        }
    } else {

        uint32_t fileFormat;
        uint32_t fileVersion;

        fileFormat = Stream.ReadUInt32();

        if ( fileFormat != FMT_FILE_TYPE_TEXTURE ) {
            GLogger.Printf( "Expected file format %d\n", FMT_FILE_TYPE_TEXTURE );
            return false;
        }

        fileVersion = Stream.ReadUInt32();

        if ( fileVersion != FMT_VERSION_TEXTURE ) {
            GLogger.Printf( "Expected file version %d\n", FMT_VERSION_TEXTURE );
            return false;
        }

        AString guid;
        uint32_t textureType;
        STexturePixelFormat texturePixelFormat;
        uint32_t w, h, d, numLods;

        Stream.ReadObject( guid );
        textureType = Stream.ReadUInt32();
        Stream.ReadObject( texturePixelFormat );
        w = Stream.ReadUInt32();
        h = Stream.ReadUInt32();
        d = Stream.ReadUInt32();
        numLods = Stream.ReadUInt32();

        switch ( textureType ) {
        case TEXTURE_1D:
            Initialize1D( texturePixelFormat, numLods, w );
            break;
        case TEXTURE_1D_ARRAY:
            Initialize1DArray( texturePixelFormat, numLods, w, h );
            break;
        case TEXTURE_2D:
            Initialize2D( texturePixelFormat, numLods, w, h );
            break;
        case TEXTURE_2D_ARRAY:
            Initialize2DArray( texturePixelFormat, numLods, w, h, d );
            break;
        case TEXTURE_3D:
            Initialize3D( texturePixelFormat, numLods, w, h, d );
            break;
        case TEXTURE_CUBEMAP:
            InitializeCubemap( texturePixelFormat, numLods, w );
            break;
        case TEXTURE_CUBEMAP_ARRAY:
            InitializeCubemapArray( texturePixelFormat, numLods, w, d );
            break;
        default:
            GLogger.Printf( "ATexture::LoadResource: Unknown texture type %d\n", textureType );
            return false;
        }

        uint32_t lodWidth, lodHeight, lodDepth;
        size_t pixelSize = texturePixelFormat.SizeInBytesUncompressed();
        size_t maxSize = w * h * d * pixelSize;
        //byte * lodData = (byte *)GHunkMemory.HunkMemory( maxSize, 1 );
        byte * lodData = (byte *)GHeapMemory.Alloc( maxSize, 1 );

        //int numLayers = 1;

        //if ( textureType == TEXTURE_CUBEMAP ) {
        //    numLayers = 6;
        //} else if ( textureType == TEXTURE_CUBEMAP_ARRAY ) {
        //    numLayers = d * 6;
        //}
        //
        //for ( int layerNum = 0 ; layerNum < numLayers ; layerNum++ ) {
            for ( int n = 0 ; n < numLods ; n++ ) {
                lodWidth = Stream.ReadUInt32();
                lodHeight = Stream.ReadUInt32();
                lodDepth = Stream.ReadUInt32();

                size_t size = lodWidth * lodHeight * lodDepth * pixelSize;

                if ( size > maxSize ) {
                    GLogger.Printf( "ATexture: invalid image %s\n", fn );
                    break;
                }

                Stream.ReadBuffer( lodData, size );

                WriteArbitraryData( 0, 0, /*layerNum*/0, lodWidth, lodHeight, lodDepth, n, lodData );
            }
        //}

        //GHunkMemory.ClearLastHunk();
        GHeapMemory.Free( lodData );

#if 0
        byte * buf = (byte *)GHeapMemory.Alloc( size );

        Stream.Read( buf, size );

        AMemoryStream ms;

        if ( !ms.OpenRead( _Path, buf, size ) ) {
            GHeapMemory.HeapFree( buf );
            return false;
        }

        if ( !image.LoadLDRI( ms, bSRGB, true ) ) {
            GHeapMemory.HeapFree( buf );
            return false;
        }

        GHeapMemory.HeapFree( buf );

        if ( !InitializeFromImage( image ) ) {
            return false;
        }
#endif
    }

    return true;
}

bool ATexture::IsCubemap() const {
    return TextureType == TEXTURE_CUBEMAP || TextureType == TEXTURE_CUBEMAP_ARRAY;
}

size_t ATexture::TextureSizeInBytes1D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    if ( _PixelFormat.IsCompressed() ) {
        // TODO
        AN_ASSERT(0);
        return 0;
    } else {
        size_t sum = 0;
        for ( int i = 0 ; i < _NumLods ; i++ ) {
            sum += Math::Max( 1, _Width );
            _Width >>= 1;
        }
        return _PixelFormat.SizeInBytesUncompressed() * sum * Math::Max( _ArraySize, 1 );
    }
}

size_t ATexture::TextureSizeInBytes2D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _ArraySize ) {
    if ( _PixelFormat.IsCompressed() ) {
        // TODO
        AN_ASSERT(0);
        return 0;
    } else {
        size_t sum = 0;
        for ( int i = 0 ; i < _NumLods ; i++ ) {
            sum += Math::Max( 1, _Width ) * Math::Max( 1, _Height );
            _Width >>= 1;
            _Height >>= 1;
        }
        return _PixelFormat.SizeInBytesUncompressed() * sum * Math::Max( _ArraySize, 1 );
    }
}

size_t ATexture::TextureSizeInBytes3D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _Depth ) {
    if ( _PixelFormat.IsCompressed() ) {
        // TODO
        AN_ASSERT(0);
        return 0;
    } else {
        size_t sum = 0;
        for ( int i = 0 ; i < _NumLods ; i++ ) {
            sum += Math::Max( 1, _Width ) * Math::Max( 1, _Height ) * Math::Max( 1, _Depth );
            _Width >>= 1;
            _Height >>= 1;
            _Depth >>= 1;
        }
        return _PixelFormat.SizeInBytesUncompressed() * sum;
    }
}

size_t ATexture::TextureSizeInBytesCubemap( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    if ( _PixelFormat.IsCompressed() ) {
        // TODO
        AN_ASSERT(0);
        return 0;
    } else {
        size_t sum = 0;
        for ( int i = 0 ; i < _NumLods ; i++ ) {
            sum += Math::Max( 1, _Width ) * Math::Max( 1, _Width );
            _Width >>= 1;
        }
        return _PixelFormat.SizeInBytesUncompressed() * sum * 6 * Math::Max( _ArraySize, 1 );
    }
}

static void SetTextureSwizzle( STexturePixelFormat const & _PixelFormat, RenderCore::STextureSwizzle & _Swizzle )
{
    switch ( _PixelFormat.NumComponents() ) {
    case 1:
        // Apply texture swizzle for single channel textures
        _Swizzle.R = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.G = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.B = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.A = RenderCore::TEXTURE_SWIZZLE_R;
        break;
#if 0
    case 2:
        // Apply texture swizzle for two channel textures
        _Swizzle.R = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.G = RenderCore::TEXTURE_SWIZZLE_G;
        _Swizzle.B = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.A = RenderCore::TEXTURE_SWIZZLE_G;
        break;
    case 3:
        // Apply texture swizzle for three channel textures
        _Swizzle.R = RenderCore::TEXTURE_SWIZZLE_R;
        _Swizzle.G = RenderCore::TEXTURE_SWIZZLE_G;
        _Swizzle.B = RenderCore::TEXTURE_SWIZZLE_B;
        _Swizzle.A = RenderCore::TEXTURE_SWIZZLE_ONE;
        break;
#endif
    }
}

void ATexture::Initialize1D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width ) {
    Purge();

    TextureType = TEXTURE_1D;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = 1;
    Depth = 1;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_1D;
    textureCI.Resolution.Tex1D.Width = _Width;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::Initialize1DArray( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    Purge();

    TextureType = TEXTURE_1D_ARRAY;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _ArraySize;
    Depth = 1;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_1D_ARRAY;
    textureCI.Resolution.Tex1DArray.Width = _Width;
    textureCI.Resolution.Tex1DArray.NumLayers = _ArraySize;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::Initialize2D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height ) {
    Purge();

    TextureType = TEXTURE_2D;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _Height;
    Depth = 1;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_2D;
    textureCI.Resolution.Tex2D.Width = _Width;
    textureCI.Resolution.Tex2D.Height = _Height;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::Initialize2DArray( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _ArraySize ) {
    Purge();

    TextureType = TEXTURE_2D_ARRAY;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _Height;
    Depth = _ArraySize;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_2D_ARRAY;
    textureCI.Resolution.Tex2DArray.Width = _Width;
    textureCI.Resolution.Tex2DArray.Height = _Height;
    textureCI.Resolution.Tex2DArray.NumLayers = _ArraySize;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::Initialize3D( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _Depth ) {
    Purge();

    TextureType = TEXTURE_3D;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _Height;
    Depth = _Depth;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_3D;
    textureCI.Resolution.Tex3D.Width = _Width;
    textureCI.Resolution.Tex3D.Height = _Height;
    textureCI.Resolution.Tex3D.Depth = _Depth;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::InitializeColorGradingLUT( const char * _Path ) {
    AImage image;

    if ( image.Load( _Path, nullptr, IMAGE_PF_BGR_GAMMA2 ) ) {
        byte data[ 16 ][ 16 ][ 16 ][ 3 ];

        const byte * p = static_cast< const byte * >(image.GetData());

        for ( int y = 0 ; y < 16 ; y++ ) {
           for ( int z = 0 ; z < 16 ; z++ ) {
                Core::Memcpy( &data[ z ][ y ][ 0 ][ 0 ], p, 16 * 3 );
                p += 16 * 3;
            }
        }

        Initialize3D( TEXTURE_PF_BGR8_SRGB, 1, 16, 16, 16 );
        WriteArbitraryData( 0, 0, 0, 16, 16, 16, 0, data );

        return;
    }

    LoadInternalResource( "/Default/Textures/LUT_Luminance" );
}

static Float3 ApplyColorGrading( SColorGradingPreset const & p, AColor4 const & _Color ) {
    float lum = _Color.GetLuminance();

    AColor4 mult;

    mult.SetTemperature( Math::Clamp( p.ColorTemperature, 1000.0f, 40000.0f ) );

    AColor4 c = Math::Lerp( _Color.GetRGB(), _Color.GetRGB() * mult.GetRGB(), p.ColorTemperatureStrength );

    float newLum = c.GetLuminance();

    c *= Math::Lerp( 1.0f, ( newLum > 1e-6 ) ? ( lum / newLum ) : 1.0f, p.ColorTemperatureBrightnessNormalization );

    c = Math::Lerp( Float3( c.GetLuminance() ), c.GetRGB(), p.Presaturation );

    Float3 t = ( p.Gain * 2.0f ) * ( c.GetRGB() + ( ( p.Lift * 2.0f - 1.0 ) * ( Float3( 1.0 ) - c.GetRGB() ) ) );

    t.X = Math::Pow( t.X, 0.5f / p.Gamma.X );
    t.Y = Math::Pow( t.Y, 0.5f / p.Gamma.Y );
    t.Z = Math::Pow( t.Z, 0.5f / p.Gamma.Z );

    return t;
}

void ATexture::InitializeColorGradingLUT( SColorGradingPreset const & _Preset ) {
    byte data[ 16 ][ 16 ][ 16 ][ 3 ];
    AColor4 color;
    Float3 result;

    Initialize3D( TEXTURE_PF_BGR8_SRGB, 1, 16, 16, 16 );

    const float scale = 1.0f / 15.0f;

    for ( int z = 0 ; z < 16 ; z++ ) {
        color.Z = scale * z;

        for ( int y = 0 ; y < 16 ; y++ ) {
            color.Y = scale * y;

            for ( int x = 0 ; x < 16 ; x++ ) {
                color.X = scale * x;

                result = ApplyColorGrading( _Preset, color ) * 255.0f;

                data[ z ][ y ][ x ][ 0 ] = Math::Clamp( result.Z, 0.0f, 255.0f );
                data[ z ][ y ][ x ][ 1 ] = Math::Clamp( result.Y, 0.0f, 255.0f );
                data[ z ][ y ][ x ][ 2 ] = Math::Clamp( result.X, 0.0f, 255.0f );
            }
        }
    }

    WriteArbitraryData( 0, 0, 0, 16, 16, 16, 0, data );
}

void ATexture::InitializeCubemap( STexturePixelFormat _PixelFormat, int _NumLods, int _Width ) {
    Purge();

    TextureType = TEXTURE_CUBEMAP;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _Width;
    Depth = 1;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_CUBE_MAP;
    textureCI.Resolution.TexCubemap.Width = _Width;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

void ATexture::InitializeCubemapArray( STexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    Purge();

    TextureType = TEXTURE_CUBEMAP_ARRAY;
    PixelFormat = _PixelFormat;
    Width = _Width;
    Height = _Width;
    Depth = _ArraySize;
    NumLods = _NumLods;

    RenderCore::STextureCreateInfo textureCI = {};
    textureCI.Type = RenderCore::TEXTURE_CUBE_MAP_ARRAY;
    textureCI.Resolution.TexCubemapArray.Width = _Width;
    textureCI.Resolution.TexCubemapArray.NumLayers = _ArraySize;
    textureCI.Format = TextureFormatMapper.InternalPixelFormatTable[_PixelFormat.Data];
    textureCI.NumLods = _NumLods;

    SetTextureSwizzle( _PixelFormat, textureCI.Swizzle );

    GRuntime->GetRenderDevice()->CreateTexture( textureCI, &TextureGPU );
}

size_t ATexture::GetSizeInBytes() const {
    switch ( TextureType ) {
    case TEXTURE_1D:
        return ATexture::TextureSizeInBytes1D( PixelFormat, NumLods, Width, 1 );
    case TEXTURE_1D_ARRAY:
        return ATexture::TextureSizeInBytes1D( PixelFormat, NumLods, Width, GetArraySize() );
    case TEXTURE_2D:
        return ATexture::TextureSizeInBytes2D( PixelFormat, NumLods, Width, Height, 1 );
    case TEXTURE_2D_ARRAY:
        return ATexture::TextureSizeInBytes2D( PixelFormat, NumLods, Width, Height, GetArraySize() );
    case TEXTURE_3D:
        return ATexture::TextureSizeInBytes3D( PixelFormat, NumLods, Width, Height, Depth );
    case TEXTURE_CUBEMAP:
        return ATexture::TextureSizeInBytesCubemap( PixelFormat, NumLods, Width, 1 );
    case TEXTURE_CUBEMAP_ARRAY:
        return ATexture::TextureSizeInBytesCubemap( PixelFormat, NumLods, Width, GetArraySize() );
    }
    return 0;
}

int ATexture::GetArraySize() const {
    switch ( TextureType ) {
    case TEXTURE_1D_ARRAY:
        return Height;
    case TEXTURE_2D_ARRAY:
    case TEXTURE_CUBEMAP_ARRAY:
        return Depth;
    }
    return 1;
}

bool ATexture::WriteTextureData1D( int _LocationX, int _Width, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_1D && TextureType != TEXTURE_1D_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureData1D: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, 0, 0, _Width, 1, 1, _Lod, _SysMem );
}

bool ATexture::WriteTextureData1DArray( int _LocationX, int _Width, int _ArrayLayer, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_1D_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureData1DArray: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _ArrayLayer, 0, _Width, 1, 1, _Lod, _SysMem );
}

bool ATexture::WriteTextureData2D( int _LocationX, int _LocationY, int _Width, int _Height, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_2D && TextureType != TEXTURE_2D_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureData2D: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _LocationY, 0, _Width, _Height, 1, _Lod, _SysMem );
}

bool ATexture::WriteTextureData2DArray( int _LocationX, int _LocationY, int _Width, int _Height, int _ArrayLayer, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_2D_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureData2DArray: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _LocationY, _ArrayLayer, _Width, _Height, 1, _Lod, _SysMem );
}

bool ATexture::WriteTextureData3D( int _LocationX, int _LocationY, int _LocationZ, int _Width, int _Height, int _Depth, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_3D ) {
        GLogger.Printf( "ATexture::WriteTextureData3D: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _LocationY, _LocationZ, _Width, _Height, _Depth, _Lod, _SysMem );
}

bool ATexture::WriteTextureDataCubemap( int _LocationX, int _LocationY, int _Width, int _Height, int _FaceIndex, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_CUBEMAP && TextureType != TEXTURE_CUBEMAP_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureDataCubemap: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _LocationY, _FaceIndex, _Width, _Height, 1, _Lod, _SysMem );
}

bool ATexture::WriteTextureDataCubemapArray( int _LocationX, int _LocationY, int _Width, int _Height, int _FaceIndex, int _ArrayLayer, int _Lod, const void * _SysMem ) {
    if ( TextureType != TEXTURE_CUBEMAP_ARRAY ) {
        GLogger.Printf( "ATexture::WriteTextureDataCubemapArray: called for %s\n", TextureTypeName[TextureType] );
        return false;
    }
    return WriteArbitraryData( _LocationX, _LocationY, _ArrayLayer*6 + _FaceIndex, _Width, _Height, 1, _Lod, _SysMem );
}

bool ATexture::WriteArbitraryData( int _LocationX, int _LocationY, int _LocationZ, int _Width, int _Height, int _Depth, int _Lod, const void * _SysMem ) {
    if ( !Width ) {
        GLogger.Printf( "ATexture::WriteArbitraryData: texture is not initialized\n" );
        return false;
    }

    size_t sizeInBytes = _Width * _Height * _Depth;

    if ( IsCompressed() ) {
        // TODO
        AN_ASSERT( 0 );
        return false;
    } else {
        sizeInBytes *= SizeInBytesUncompressed();
    }

    // TODO: bounds check?

    RenderCore::STextureRect rect;
    rect.Offset.X = _LocationX;
    rect.Offset.Y = _LocationY;
    rect.Offset.Z = _LocationZ;
    rect.Offset.Lod = _Lod;
    rect.Dimension.X = _Width;
    rect.Dimension.Y = _Height;
    rect.Dimension.Z = _Depth;

    TextureGPU->WriteRect( rect, TextureFormatMapper.PixelFormatTable[PixelFormat.Data], sizeInBytes, 1, _SysMem );

    return true;
}
