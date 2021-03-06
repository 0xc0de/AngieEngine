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

#include <Core/Public/Core.h>
#include <Core/Public/WindowsDefs.h>
#include <Core/Public/CoreMath.h>
#include <Core/Public/Logger.h>

#include "DeviceGLImpl.h"
#include "ImmediateContextGLImpl.h"
#include "BufferGLImpl.h"
#include "BufferViewGLImpl.h"
#include "TextureGLImpl.h"
#include "SparseTextureGLImpl.h"
#include "SamplerGLImpl.h"
#include "TransformFeedbackGLImpl.h"
#include "QueryGLImpl.h"
#include "RenderPassGLImpl.h"
#include "PipelineGLImpl.h"
#include "ShaderModuleGLImpl.h"

#include "LUT.h"

#include "GL/glew.h"
#ifdef AN_OS_WIN32
#include "GL/wglew.h"
#endif
#ifdef AN_OS_LINUX
#include "GL/glxew.h"
#endif

#include <SDL.h>

namespace RenderCore {

struct SamplerInfo
{
    SSamplerInfo CreateInfo;
    unsigned int Id;
};

static const char * FeatureName[] =
{
    "FEATURE_HALF_FLOAT_VERTEX",
    "FEATURE_HALF_FLOAT_PIXEL",
    "FEATURE_TEXTURE_ANISOTROPY",
    "FEATURE_SPARSE_TEXTURES",
    "FEATURE_BINDLESS_TEXTURE",
    "FEATURE_SWAP_CONTROL",
    "FEATURE_SWAP_CONTROL_TEAR",
    "FEATURE_GPU_MEMORY_INFO",
    "FEATURE_SPIR_V"
};

static const char * DeviceCapName[] =
{
    "DEVICE_CAPS_BUFFER_VIEW_MAX_SIZE",
    "DEVICE_CAPS_BUFFER_VIEW_OFFSET_ALIGNMENT",

    "DEVICE_CAPS_CONSTANT_BUFFER_OFFSET_ALIGNMENT",
    "DEVICE_CAPS_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT",

    "DEVICE_CAPS_MAX_TEXTURE_SIZE",
    "DEVICE_CAPS_MAX_TEXTURE_LAYERS",
    "DEVICE_CAPS_MAX_SPARSE_TEXTURE_LAYERS",
    "DEVICE_CAPS_MAX_TEXTURE_ANISOTROPY",
    "DEVICE_CAPS_MAX_PATCH_VERTICES",
    "DEVICE_CAPS_MAX_VERTEX_BUFFER_SLOTS",
    "DEVICE_CAPS_MAX_VERTEX_ATTRIB_STRIDE",
    "DEVICE_CAPS_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET",
    "DEVICE_CAPS_MAX_CONSTANT_BUFFER_BINDINGS",
    "DEVICE_CAPS_MAX_SHADER_STORAGE_BUFFER_BINDINGS",
    "DEVICE_CAPS_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS",
    "DEVICE_CAPS_MAX_TRANSFORM_FEEDBACK_BUFFERS",
    "DEVICE_CAPS_CONSTANT_BUFFER_MAX_BLOCK_SIZE"
};

static int GL_GetInteger( GLenum pname )
{
    GLint i = 0;
    glGetIntegerv( pname, &i );
    return i;
}

static int64_t GL_GetInteger64( GLenum pname )
{
    if ( glGetInteger64v ) {
        int64_t i = 0;
        glGetInteger64v( pname, &i );
        return i;
    }

    return GL_GetInteger( pname );
}


static float GL_GetFloat( GLenum pname )
{
    float f;
    glGetFloatv( pname, &f );
    return f;
}

static bool FindExtension( const char * _Extension )
{
    GLint numExtensions = 0;

    glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
    for ( int i = 0 ; i < numExtensions ; i++ ) {
        const char * ExtI = ( const char * )glGetStringi( GL_EXTENSIONS, i );
        if ( ExtI && Core::Strcmp( ExtI, _Extension ) == 0 ) {
            return true;
        }
    }
    return false;
}

static void * Allocate( size_t _BytesCount )
{
    return GZoneMemory.Alloc(_BytesCount );
}

static void Deallocate( void * _Bytes )
{
    GZoneMemory.Free( _Bytes );
}

static constexpr SAllocatorCallback DefaultAllocator =
{
    Allocate, Deallocate
};

static int SDBM_Hash( const unsigned char * _Data, int _Size )
{
    int hash = 0;
    while ( _Size-- > 0 ) {
        hash = *_Data++ + ( hash << 6 ) + ( hash << 16 ) - hash;
    }
    return hash;
}

static void DebugMessageCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * userParam )
{
    const char * sourceStr;
    switch ( source ) {
    case GL_DEBUG_SOURCE_API:
        sourceStr = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = "WINDOW SYSTEM";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = "SHADER COMPILER";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = "THIRD PARTY";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = "APPLICATION";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        sourceStr = "OTHER";
        break;
    default:
        sourceStr = "UNKNOWN";
        break;
    }

    const char * typeStr;
    switch ( type ) {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "DEPRECATED BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "UNDEFINED BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        typeStr = "MISC";
        break;
    case GL_DEBUG_TYPE_MARKER:
        typeStr = "MARKER";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        typeStr = "PUSH GROUP";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        typeStr = "POP GROUP";
        break;
    default:
        typeStr = "UNKNOWN";
        break;
    }

    const char * severityStr;
    switch ( severity ) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        severityStr = "NOTIFICATION";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "HIGH";
        break;
    default:
        severityStr = "UNKNOWN";
        break;
    }

    if ( type == GL_DEBUG_TYPE_OTHER && severity == GL_DEBUG_SEVERITY_NOTIFICATION ) {
        // Do not print annoying notifications
        return;
    }

    GLogger.Printf( "-----------------------------------\n"
                    "%s %s\n"
                    "%s: %s (Id %d)\n"
                    "-----------------------------------\n",
                    sourceStr, typeStr, severityStr, message, id );
}

void CreateLogicalDevice( SImmediateContextCreateInfo const & _CreateInfo,
                          SAllocatorCallback const * _Allocator,
                          HashCallback _Hash,
                          TRef< IDevice > * ppDevice )
{
    *ppDevice = MakeRef< ADeviceGLImpl >( _CreateInfo, _Allocator, _Hash );
}

ADeviceGLImpl::ADeviceGLImpl( SImmediateContextCreateInfo const & _CreateInfo,
                              SAllocatorCallback const * _Allocator,
                              HashCallback _Hash )
{
    TotalContexts = 0;
    TotalBuffers = 0;
    TotalTextures = 0;
    TotalShaderModules = 0;
    TotalPipelines = 0;
    TotalRenderPasses = 0;
    TotalFramebuffers = 0;
    TotalTransformFeedbacks = 0;
    TotalQueryPools = 0;
    BufferMemoryAllocated = 0;
    TextureMemoryAllocated = 0;

    SDL_GLContext windowCtx = SDL_GL_CreateContext( _CreateInfo.Window );
    if ( !windowCtx ) {
        CriticalError( "Failed to initialize OpenGL context\n" );
    }

    SDL_GL_MakeCurrent( _CreateInfo.Window, windowCtx );

    // Set glewExperimental=true to allow extension entry points to be loaded even if extension isn't present
    // in the driver's extensions string
    glewExperimental = true;

    GLenum result = glewInit();
    if ( result != GLEW_OK ) {
        CriticalError( "Failed to load OpenGL functions\n" );
    }

    // GLEW has a long-existing bug where calling glewInit() always sets the GL_INVALID_ENUM error flag and
    // thus the first glGetError will always return an error code which can throw you completely off guard.
    // To fix this it's advised to simply call glGetError after glewInit to clear the flag.
    (void)glGetError();

#ifdef AN_DEBUG
    GLint contextFlags;
    glGetIntegerv( GL_CONTEXT_FLAGS, &contextFlags );
    if ( contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT ) {
        glEnable( GL_DEBUG_OUTPUT );
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
        glDebugMessageCallback( DebugMessageCallback, NULL );
    }
#endif

    const char * vendorString = (const char *)glGetString( GL_VENDOR );
    vendorString = vendorString ? vendorString : "Unknown";

    const char * adapterString = (const char *)glGetString( GL_RENDERER );
    adapterString = adapterString ? adapterString : "Unknown";

    const char * driverVersion = (const char *)glGetString( GL_VERSION );
    driverVersion = driverVersion ? driverVersion : "Unknown";

    GLogger.Printf( "Graphics vendor: %s\n", vendorString );
    GLogger.Printf( "Graphics adapter: %s\n", adapterString );
    GLogger.Printf( "Driver version: %s\n", driverVersion );

    if ( Core::SubstringIcmp( vendorString, "NVIDIA" ) != -1 ) {
        GraphicsVendor = VENDOR_NVIDIA;
    }
    else if ( Core::SubstringIcmp( vendorString, "ATI" ) != -1 ) {
        GraphicsVendor = VENDOR_ATI;
    }
    else if ( Core::SubstringIcmp( vendorString, "Intel" ) != -1 ) {
        GraphicsVendor = VENDOR_INTEL;
    }

    int numExtensions = 0;
    glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );

#if 0
    int w, h;
    SDL_GetWindowSize( _CreateInfo.Window, &w, &h );
    if ( w < 1024 ) {
        for ( int i = 0 ; i < numExtensions ; i++ ) {
            GLogger.Printf( "\t%s\n", (const char *)glGetStringi( GL_EXTENSIONS, i ) );
        }
    }
    else
#endif
    {
        char str[128];
        const int MaxExtensionLength = 40;
        for ( int i = 0 ; i < numExtensions ; ) {
            const char * extName1 = (const char *)glGetStringi( GL_EXTENSIONS, i );

            if ( i + 1 < numExtensions ) {
                const char * extName2 = (const char *)glGetStringi( GL_EXTENSIONS, i+1 );

                int strLen1 = Core::Strlen( extName1 );
                int strLen2 = Core::Strlen( extName2 );

                if ( strLen1 < MaxExtensionLength && strLen2 < MaxExtensionLength ) {
                    Core::Memset( &str[strLen1], ' ', MaxExtensionLength-strLen1 );
                    Core::Memcpy( str, extName1, strLen1 );
                    Core::Memcpy( &str[MaxExtensionLength], extName2, strLen2 );
                    str[ MaxExtensionLength + strLen2 ] = '\0';

                    GLogger.Printf( " %s\n", str );

                    i += 2;
                    continue;
                }

                // long extension name
            }

            GLogger.Printf( " %s\n", extName1 );
            i += 1;
        }
    }

#if 0
    SMemoryInfo gpuMemoryInfo = GetGPUMemoryInfo();
    if ( gpuMemoryInfo.TotalAvailableMegabytes > 0 && gpuMemoryInfo.CurrentAvailableMegabytes > 0 ) {
        GLogger.Printf( "Total available GPU memory: %d Megs\n", gpuMemoryInfo.TotalAvailableMegabytes );
        GLogger.Printf( "Current available GPU memory: %d Megs\n", gpuMemoryInfo.CurrentAvailableMegabytes );
    }
#endif

    FeatureSupport[FEATURE_HALF_FLOAT_VERTEX] = FindExtension( "GL_ARB_half_float_vertex" );
    FeatureSupport[FEATURE_HALF_FLOAT_PIXEL] = FindExtension( "GL_ARB_half_float_pixel" );
    FeatureSupport[FEATURE_TEXTURE_ANISOTROPY] = FindExtension( "GL_ARB_texture_filter_anisotropic" ) || FindExtension( "GL_EXT_texture_filter_anisotropic" );
    FeatureSupport[FEATURE_SPARSE_TEXTURES] = FindExtension( "GL_ARB_sparse_texture" );// && FindExtension( "GL_ARB_sparse_texture2" );
    FeatureSupport[FEATURE_BINDLESS_TEXTURE] = FindExtension( "GL_ARB_bindless_texture" );

#if defined( AN_OS_WIN32 )
    FeatureSupport[FEATURE_SWAP_CONTROL] = !!WGLEW_EXT_swap_control;
    FeatureSupport[FEATURE_SWAP_CONTROL_TEAR] = !!WGLEW_EXT_swap_control_tear;
#elif defined( AN_OS_LINUX )
    FeatureSupport[FEATURE_SWAP_CONTROL] = !!GLXEW_EXT_swap_control || !!GLXEW_MESA_swap_control || !!GLXEW_SGI_swap_control;
    FeatureSupport[FEATURE_SWAP_CONTROL_TEAR] = !!GLXEW_EXT_swap_control_tear;
#else
#error "Swap control tear checking not implemented on current platform"
#endif
    FeatureSupport[FEATURE_GPU_MEMORY_INFO] = FindExtension( "GL_NVX_gpu_memory_info" );

    FeatureSupport[FEATURE_SPIR_V] = FindExtension( "GL_ARB_gl_spirv" );

    if ( !FindExtension( "GL_EXT_texture_compression_s3tc" ) ) {
        GLogger.Printf( "Warning: required extension GL_EXT_texture_compression_s3tc isn't supported\n" );
    }

    if ( !FindExtension( "GL_ARB_texture_compression_rgtc" ) && !FindExtension( "GL_EXT_texture_compression_rgtc" ) ) {
        GLogger.Printf( "Warning: required extension GL_ARB_texture_compression_rgtc/GL_EXT_texture_compression_rgtc isn't supported\n" );
    }

    if ( !FindExtension( "GL_EXT_texture_compression_rgtc" ) ) {
        GLogger.Printf( "Warning: required extension GL_EXT_texture_compression_rgtc isn't supported\n" );
    }

#if 0
    unsigned int MaxCombinedTextureImageUnits = GL_GetInteger( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS );
    //MaxVertexTextureImageUnits = GL_GetInteger( GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS );
    unsigned int MaxImageUnits = GL_GetInteger( GL_MAX_IMAGE_UNITS );
#endif

    DeviceCaps[DEVICE_CAPS_MAX_VERTEX_BUFFER_SLOTS] = GL_GetInteger( GL_MAX_VERTEX_ATTRIB_BINDINGS ); // TODO: check if 0 ???
    DeviceCaps[DEVICE_CAPS_MAX_VERTEX_ATTRIB_STRIDE] = GL_GetInteger( GL_MAX_VERTEX_ATTRIB_STRIDE ); // GL_MAX_VERTEX_ATTRIB_STRIDE sinse GL v4.4
    if ( DeviceCaps[DEVICE_CAPS_MAX_VERTEX_ATTRIB_STRIDE] == 0 ) {
        DeviceCaps[DEVICE_CAPS_MAX_VERTEX_ATTRIB_STRIDE] = std::numeric_limits< unsigned int >::max(); // Set some undefined value
    }
    DeviceCaps[DEVICE_CAPS_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET] = GL_GetInteger( GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET );

    DeviceCaps[DEVICE_CAPS_BUFFER_VIEW_MAX_SIZE] = GL_GetInteger( GL_MAX_TEXTURE_BUFFER_SIZE );

    DeviceCaps[DEVICE_CAPS_BUFFER_VIEW_OFFSET_ALIGNMENT] = GL_GetInteger( GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT );
    if ( !DeviceCaps[DEVICE_CAPS_BUFFER_VIEW_OFFSET_ALIGNMENT] ) {
        GLogger.Printf( "Warning: TextureBufferOffsetAlignment == 0, using default alignment (256)\n" );
        DeviceCaps[DEVICE_CAPS_BUFFER_VIEW_OFFSET_ALIGNMENT] = 256;
    }

    DeviceCaps[DEVICE_CAPS_CONSTANT_BUFFER_OFFSET_ALIGNMENT] = GL_GetInteger( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT );
    if ( !DeviceCaps[DEVICE_CAPS_CONSTANT_BUFFER_OFFSET_ALIGNMENT] ) {
        GLogger.Printf( "Warning: ConstantBufferOffsetAlignment == 0, using default alignment (256)\n" );
        DeviceCaps[DEVICE_CAPS_CONSTANT_BUFFER_OFFSET_ALIGNMENT] = 256;
    }

    DeviceCaps[DEVICE_CAPS_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT] = GL_GetInteger( GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT );
    if ( !DeviceCaps[DEVICE_CAPS_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT] ) {
        GLogger.Printf( "Warning: ShaderStorageBufferOffsetAlignment == 0, using default alignment (256)\n" );
        DeviceCaps[DEVICE_CAPS_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT] = 256;
    }

    DeviceCaps[DEVICE_CAPS_MAX_CONSTANT_BUFFER_BINDINGS] = GL_GetInteger( GL_MAX_UNIFORM_BUFFER_BINDINGS );
    DeviceCaps[DEVICE_CAPS_MAX_SHADER_STORAGE_BUFFER_BINDINGS] = GL_GetInteger( GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS );
    DeviceCaps[DEVICE_CAPS_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS] = GL_GetInteger( GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS );
    DeviceCaps[DEVICE_CAPS_MAX_TRANSFORM_FEEDBACK_BUFFERS] = GL_GetInteger( GL_MAX_TRANSFORM_FEEDBACK_BUFFERS );

    DeviceCaps[DEVICE_CAPS_CONSTANT_BUFFER_MAX_BLOCK_SIZE] = GL_GetInteger( GL_MAX_UNIFORM_BLOCK_SIZE );

    if ( FeatureSupport[FEATURE_TEXTURE_ANISOTROPY] ) {
        DeviceCaps[DEVICE_CAPS_MAX_TEXTURE_ANISOTROPY] = (unsigned int)GL_GetFloat( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT );
    } else {
        DeviceCaps[DEVICE_CAPS_MAX_TEXTURE_ANISOTROPY] = 0;
    }

    DeviceCaps[DEVICE_CAPS_MAX_TEXTURE_SIZE] = GL_GetInteger( GL_MAX_TEXTURE_SIZE );    
    DeviceCaps[DEVICE_CAPS_MAX_TEXTURE_LAYERS] = GL_GetInteger( GL_MAX_ARRAY_TEXTURE_LAYERS );
    DeviceCaps[DEVICE_CAPS_MAX_SPARSE_TEXTURE_LAYERS] = GL_GetInteger( GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS );
    DeviceCaps[DEVICE_CAPS_MAX_PATCH_VERTICES] = GL_GetInteger( GL_MAX_PATCH_VERTICES );

#if 0
    GLogger.Printf( "GL_MAX_TESS_CONTROL_INPUT_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_INPUT_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS %d\n", GL_GetInteger( GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS %d\n", GL_GetInteger( GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_GEN_LEVEL %d\n", GL_GetInteger( GL_MAX_TESS_GEN_LEVEL ) );
    GLogger.Printf( "GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS ) );
    GLogger.Printf( "GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS %d\n", GL_GetInteger( GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS ) );
    GLogger.Printf( "GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_PATCH_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_PATCH_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS %d\n", GL_GetInteger( GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS ) );
    GLogger.Printf( "GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS %d\n", GL_GetInteger( GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS ) );
    GLogger.Printf( "GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS %d\n", GL_GetInteger( GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS ) );
#endif
    GLogger.Printf( "Features:\n" );
    for ( int i = 0 ; i < FEATURE_MAX ; i++ ) {
        GLogger.Printf( "\t%s: %s\n", FeatureName[i], FeatureSupport[i] ? "Yes" : "No" );
    }

    GLogger.Printf( "Device caps:\n" );
    for ( int i = 0 ; i < DEVICE_CAPS_MAX ; i++ ) {
        GLogger.Printf( "\t%s: %u\n", DeviceCapName[i], DeviceCaps[i] );
    }

    if ( FeatureSupport[FEATURE_GPU_MEMORY_INFO] ) {
        int32_t dedicated = GL_GetInteger( GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX );
        int32_t totalAvail = GL_GetInteger( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX );
        int32_t currentAvail = GL_GetInteger( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX );
        int32_t evictionCount = GL_GetInteger( GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX );
        int32_t evictedMemory = GL_GetInteger( GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX );

        GLogger.Printf( "Video memory info:\n" );
        GLogger.Printf( "\tDedicated: %d Megs\n", dedicated >> 10 );
        GLogger.Printf( "\tTotal available: %d Megs\n", totalAvail >> 10 );
        GLogger.Printf( "\tCurrent available: %d Megs\n", currentAvail >> 10 );
        GLogger.Printf( "\tEviction count: %d\n", evictionCount );
        GLogger.Printf( "\tEvicted memory: %d Megs\n", evictedMemory >> 10 );
    }

    Allocator = _Allocator ? *_Allocator : DefaultAllocator;
    HashCB = _Hash ? _Hash : SDBM_Hash;

    TotalContexts++;
    pMainContext = new AImmediateContextGLImpl( this, _CreateInfo, windowCtx );

    // Mark swap interval modified to set initial swap interval.
    SwapInterval = -9999;

    // Clear garbage on screen
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT );

    // Set initial swap interval and swap the buffers
    SwapBuffers( _CreateInfo.Window, _CreateInfo.SwapInterval );
}

ADeviceGLImpl::~ADeviceGLImpl()
{
    for ( SamplerInfo * sampler : SamplerCache ) {
        glDeleteSamplers( 1, &sampler->Id );

        Allocator.Deallocate( sampler );
    }

    for ( SBlendingStateInfo * state : BlendingStateCache ) {
        Allocator.Deallocate( state );
    }

    for ( SRasterizerStateInfo * state : RasterizerStateCache ) {
        Allocator.Deallocate( state );
    }

    for ( SDepthStencilStateInfo * state : DepthStencilStateCache ) {
        Allocator.Deallocate( state );
    }

    SamplerCache.Free();
    BlendingStateCache.Free();
    RasterizerStateCache.Free();
    DepthStencilStateCache.Free();

    SamplerHash.Free();
    BlendingHash.Free();
    RasterizerHash.Free();
    DepthStencilHash.Free();

    ReleaseImmediateContext( pMainContext );
    pMainContext = nullptr;

#ifdef AN_DEBUG
    //SDL_SetRelativeMouseMode( SDL_FALSE );
    IDeviceObject * deviceObjects = GetDeviceObjects_DEBUG();
    for ( IDeviceObject * object = deviceObjects ; object ; object = object->GetNext_DEBUG() ) {
        GLogger.Printf( "Not removed device object: \"%s\"\n", object->GetDebugName() );
    }
#endif

    AN_ASSERT( TotalContexts == 0 );
    AN_ASSERT( TotalBuffers == 0 );
    AN_ASSERT( TotalTextures == 0 );
    AN_ASSERT( TotalShaderModules == 0 );
}

void ADeviceGLImpl::SwapBuffers( SDL_Window * WindowHandle, int InSwapInterval )
{
    InSwapInterval = Math::Clamp( InSwapInterval, -1, 1 );
    if ( InSwapInterval == -1 && !FeatureSupport[FEATURE_SWAP_CONTROL_TEAR] ) {
        // Tearing not supported
        InSwapInterval = 0;
    }

    if ( SwapInterval != InSwapInterval ) {
        GLogger.Printf( "Changing swap interval to %d\n", InSwapInterval );

        SDL_GL_SetSwapInterval( InSwapInterval );

        SwapInterval = InSwapInterval;
    }

    SDL_GL_SwapWindow( WindowHandle );
}

void ADeviceGLImpl::GetImmediateContext( IImmediateContext ** ppImmediateContext )
{
    *ppImmediateContext = pMainContext;
}

void ADeviceGLImpl::CreateImmediateContext( SImmediateContextCreateInfo const & _CreateInfo, IImmediateContext ** ppImmediateContext )
{
    TotalContexts++;
    *ppImmediateContext = new AImmediateContextGLImpl( this, _CreateInfo, nullptr );
}

void ADeviceGLImpl::ReleaseImmediateContext( IImmediateContext * pImmediateContext )
{
    if ( pImmediateContext ) {
        AN_ASSERT( pImmediateContext->GetRefCount() == 1 );
        pImmediateContext->RemoveRef();
        TotalContexts--;
    }
}

void ADeviceGLImpl::CreateFramebuffer( SFramebufferCreateInfo const & _CreateInfo, TRef< IFramebuffer > * ppFramebuffer )
{
    *ppFramebuffer = MakeRef< AFramebufferGLImpl >( this, _CreateInfo, false );
}

void ADeviceGLImpl::CreateRenderPass( SRenderPassCreateInfo const & _CreateInfo, TRef< IRenderPass > * ppRenderPass )
{
    *ppRenderPass = MakeRef< ARenderPassGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreatePipeline( SPipelineCreateInfo const & _CreateInfo, TRef< IPipeline > * ppPipeline )
{
    *ppPipeline = MakeRef< APipelineGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreateShaderFromBinary( SShaderBinaryData const * _BinaryData, TRef< IShaderModule > * ppShaderModule )
{
    *ppShaderModule = MakeRef< AShaderModuleGLImpl >( this, _BinaryData );
}

void ADeviceGLImpl::CreateShaderFromCode( SHADER_TYPE _ShaderType, unsigned int _NumSources, const char * const * _Sources, TRef< IShaderModule > * ppShaderModule )
{
    *ppShaderModule = MakeRef< AShaderModuleGLImpl >( this, _ShaderType, _NumSources, _Sources );
}

void ADeviceGLImpl::CreateBuffer( SBufferCreateInfo const & _CreateInfo, const void * _SysMem, TRef< IBuffer > * ppBuffer )
{
    *ppBuffer = MakeRef< ABufferGLImpl >( this, _CreateInfo, _SysMem );
}

void ADeviceGLImpl::CreateBufferView( SBufferViewCreateInfo const & CreateInfo, TRef< IBuffer > pBuffer, TRef< IBufferView > * ppBufferView )
{
    *ppBufferView = MakeRef< ABufferViewGLImpl >( this, CreateInfo, static_cast< ABufferGLImpl * >( pBuffer.GetObject() ) );
}

void ADeviceGLImpl::CreateTexture( STextureCreateInfo const & _CreateInfo, TRef< ITexture > * ppTexture )
{
    *ppTexture = MakeRef< ATextureGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreateTextureView( STextureViewCreateInfo const & _CreateInfo, TRef< ITexture > * ppTexture )
{
    *ppTexture = MakeRef< ATextureGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreateSparseTexture( SSparseTextureCreateInfo const & _CreateInfo, TRef< ISparseTexture > * ppTexture )
{
    *ppTexture = MakeRef< ASparseTextureGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreateTransformFeedback( STransformFeedbackCreateInfo const & _CreateInfo, TRef< ITransformFeedback > * ppTransformFeedback )
{
    *ppTransformFeedback = MakeRef< ATransformFeedbackGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::CreateQueryPool( SQueryPoolCreateInfo const & _CreateInfo, TRef< IQueryPool > * ppQueryPool )
{
    *ppQueryPool = MakeRef< AQueryPoolGLImpl >( this, _CreateInfo );
}

void ADeviceGLImpl::GetBindlessSampler( ITexture * pTexture, SSamplerInfo const & _CreateInfo, TRef< IBindlessSampler > * ppBindlessSampler )
{
    *ppBindlessSampler = MakeRef< ABindlessSamplerGLImpl >( this, pTexture, _CreateInfo );
}

void ADeviceGLImpl::CreateResourceTable( TRef< IResourceTable > * ppResourceTable )
{
    *ppResourceTable = MakeRef< AResourceTableGLImpl >( this );
}


bool ADeviceGLImpl::CreateShaderBinaryData( SHADER_TYPE _ShaderType,
                                            unsigned int _NumSources,
                                            const char * const * _Sources,
                                            SShaderBinaryData * _BinaryData )
{
    return AShaderModuleGLImpl::CreateShaderBinaryData( this, _ShaderType, _NumSources, _Sources, _BinaryData );
}

void ADeviceGLImpl::DestroyShaderBinaryData( SShaderBinaryData * _BinaryData )
{
    AShaderModuleGLImpl::DestroyShaderBinaryData( this, _BinaryData );
}

SAllocatorCallback const & ADeviceGLImpl::GetAllocator() const
{
    return Allocator;
}

bool ADeviceGLImpl::IsFeatureSupported( FEATURE_TYPE InFeatureType )
{
    return FeatureSupport[InFeatureType];
}

unsigned int ADeviceGLImpl::GetDeviceCaps( DEVICE_CAPS InDeviceCaps )
{
    return DeviceCaps[InDeviceCaps];
}

int32_t ADeviceGLImpl::GetGPUMemoryTotalAvailable()
{
    if ( FeatureSupport[FEATURE_GPU_MEMORY_INFO] ) {
        return GL_GetInteger( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX );
    }
    else {
        GLogger.Printf( "ADeviceGLImpl::GetGPUMemoryTotalAvailable: FEATURE_GPU_MEMORY_INFO is not supported by video driver\n" );
    }
    return 0;
}

int32_t ADeviceGLImpl::GetGPUMemoryCurrentAvailable()
{
    if ( FeatureSupport[FEATURE_GPU_MEMORY_INFO] ) {
        return GL_GetInteger( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX );
    }
    else {
        GLogger.Printf( "ADeviceGLImpl::GetGPUMemoryTotalAvailable: FEATURE_GPU_MEMORY_INFO is not supported by video driver\n" );
    }
    return 0;
}

SBlendingStateInfo const * ADeviceGLImpl::CachedBlendingState( SBlendingStateInfo const & _BlendingState )
{
    int hash = Hash( ( unsigned char * )&_BlendingState, sizeof( _BlendingState ) );

    int i = BlendingHash.First( hash );
    for ( ; i != -1 ; i = BlendingHash.Next( i ) ) {

        SBlendingStateInfo const * state = BlendingStateCache[ i ];

        if ( !memcmp( state, &_BlendingState, sizeof( *state ) ) ) {
            //GLogger.Printf( "Caching blending state\n" );
            return state;
        }
    }

    SBlendingStateInfo * state = static_cast< SBlendingStateInfo * >( Allocator.Allocate( sizeof( SBlendingStateInfo ) ) );
    memcpy( state, &_BlendingState, sizeof( *state ) );

    i = BlendingStateCache.Size();

    BlendingHash.Insert( hash, i );
    BlendingStateCache.Append( state );

    //GLogger.Printf( "Total blending states %d\n", i+1 );

    return state;
}

SRasterizerStateInfo const * ADeviceGLImpl::CachedRasterizerState( SRasterizerStateInfo const & _RasterizerState )
{
    int hash = Hash( ( unsigned char * )&_RasterizerState, sizeof( _RasterizerState ) );

    int i = RasterizerHash.First( hash );
    for ( ; i != -1 ; i = RasterizerHash.Next( i ) ) {

        SRasterizerStateInfo const * state = RasterizerStateCache[ i ];

        if ( !memcmp( state, &_RasterizerState, sizeof( *state ) ) ) {
            //GLogger.Printf( "Caching rasterizer state\n" );
            return state;
        }
    }

    SRasterizerStateInfo * state = static_cast< SRasterizerStateInfo * >( Allocator.Allocate( sizeof( SRasterizerStateInfo ) ) );
    memcpy( state, &_RasterizerState, sizeof( *state ) );

    i = RasterizerStateCache.Size();

    RasterizerHash.Insert( hash, i );
    RasterizerStateCache.Append( state );

    //GLogger.Printf( "Total rasterizer states %d\n", i+1 );

    return state;
}

SDepthStencilStateInfo const * ADeviceGLImpl::CachedDepthStencilState( SDepthStencilStateInfo const & _DepthStencilState )
{
    int hash = Hash( ( unsigned char * )&_DepthStencilState, sizeof( _DepthStencilState ) );

    int i = DepthStencilHash.First( hash );
    for ( ; i != -1 ; i = DepthStencilHash.Next( i ) ) {

        SDepthStencilStateInfo const * state = DepthStencilStateCache[ i ];

        if ( !memcmp( state, &_DepthStencilState, sizeof( *state ) ) ) {
            //GLogger.Printf( "Caching depth stencil state\n" );
            return state;
        }
    }

    SDepthStencilStateInfo * state = static_cast< SDepthStencilStateInfo * >( Allocator.Allocate( sizeof( SDepthStencilStateInfo ) ) );
    memcpy( state, &_DepthStencilState, sizeof( *state ) );

    i = DepthStencilStateCache.Size();

    DepthStencilHash.Insert( hash, i );
    DepthStencilStateCache.Append( state );

    //GLogger.Printf( "Total depth stencil states %d\n", i+1 );

    return state;
}

unsigned int ADeviceGLImpl::CachedSampler( SSamplerInfo const & _CreateInfo )
{
    int hash = Hash( (unsigned char *)&_CreateInfo, sizeof( _CreateInfo ) );

    int i = SamplerHash.First( hash );
    for ( ; i != -1 ; i = SamplerHash.Next( i ) ) {

        SamplerInfo const * sampler = SamplerCache[i];

        if ( !memcmp( &sampler->CreateInfo, &_CreateInfo, sizeof( sampler->CreateInfo ) ) ) {
            //GLogger.Printf( "Caching sampler\n" );
            return sampler->Id;
        }
    }

    SamplerInfo * sampler = static_cast< SamplerInfo * >( Allocator.Allocate( sizeof( SamplerInfo ) ) );
    memcpy( &sampler->CreateInfo, &_CreateInfo, sizeof( sampler->CreateInfo ) );

    i = SamplerCache.Size();

    SamplerHash.Insert( hash, i );
    SamplerCache.Append( sampler );

    //GLogger.Printf( "Total samplers %d\n", i+1 );

    // 3.3 or GL_ARB_sampler_objects

    GLuint id;

    glCreateSamplers( 1, &id ); // 4.5

    glSamplerParameteri( id, GL_TEXTURE_MIN_FILTER, SamplerFilterModeLUT[_CreateInfo.Filter].Min );
    glSamplerParameteri( id, GL_TEXTURE_MAG_FILTER, SamplerFilterModeLUT[_CreateInfo.Filter].Mag );
    glSamplerParameteri( id, GL_TEXTURE_WRAP_S, SamplerAddressModeLUT[_CreateInfo.AddressU] );
    glSamplerParameteri( id, GL_TEXTURE_WRAP_T, SamplerAddressModeLUT[_CreateInfo.AddressV] );
    glSamplerParameteri( id, GL_TEXTURE_WRAP_R, SamplerAddressModeLUT[_CreateInfo.AddressW] );
    glSamplerParameterf( id, GL_TEXTURE_LOD_BIAS, _CreateInfo.MipLODBias );
    if ( FeatureSupport[FEATURE_TEXTURE_ANISOTROPY] && _CreateInfo.MaxAnisotropy > 0 ) {
        glSamplerParameteri( id, GL_TEXTURE_MAX_ANISOTROPY_EXT, Math::Clamp< unsigned int >( _CreateInfo.MaxAnisotropy, 1u, DeviceCaps[DEVICE_CAPS_MAX_TEXTURE_ANISOTROPY] ) );
    }
    if ( _CreateInfo.bCompareRefToTexture ) {
        glSamplerParameteri( id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
    }
    glSamplerParameteri( id, GL_TEXTURE_COMPARE_FUNC, ComparisonFuncLUT[_CreateInfo.ComparisonFunc] );
    glSamplerParameterfv( id, GL_TEXTURE_BORDER_COLOR, _CreateInfo.BorderColor );
    glSamplerParameterf( id, GL_TEXTURE_MIN_LOD, _CreateInfo.MinLOD );
    glSamplerParameterf( id, GL_TEXTURE_MAX_LOD, _CreateInfo.MaxLOD );
    glSamplerParameteri( id, GL_TEXTURE_CUBE_MAP_SEAMLESS, _CreateInfo.bCubemapSeamless );

    // We can use also these functions to retrieve sampler parameters:
    //glGetSamplerParameterfv
    //glGetSamplerParameterIiv
    //glGetSamplerParameterIuiv
    //glGetSamplerParameteriv

    sampler->Id = id;

    return id;
}

bool ADeviceGLImpl::EnumerateSparseTexturePageSize( SPARSE_TEXTURE_TYPE Type, TEXTURE_FORMAT Format, int * NumPageSizes, int * PageSizesX, int * PageSizesY, int * PageSizesZ )
{
    GLenum target = SparseTextureTargetLUT[Type].Target;
    GLenum internalFormat = InternalFormatLUT[Format].InternalFormat;

    AN_ASSERT( NumPageSizes != nullptr );

    *NumPageSizes = 0;

    if ( !FeatureSupport[ FEATURE_SPARSE_TEXTURES ] ) {
        GLogger.Printf( "ADeviceGLImpl::EnumerateSparseTexturePageSize: sparse textures are not supported by video driver\n" );
        return false;
    }

    glGetInternalformativ( target, internalFormat, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, NumPageSizes );

    if ( PageSizesX ) {
        glGetInternalformativ( target, internalFormat, GL_VIRTUAL_PAGE_SIZE_X_ARB, *NumPageSizes, PageSizesX );
    }

    if ( PageSizesY ) {
        glGetInternalformativ( target, internalFormat, GL_VIRTUAL_PAGE_SIZE_Y_ARB, *NumPageSizes, PageSizesY );
    }

    if ( PageSizesZ ) {
        glGetInternalformativ( target, internalFormat, GL_VIRTUAL_PAGE_SIZE_Z_ARB, *NumPageSizes, PageSizesZ );
    }

    return *NumPageSizes > 0;
}

bool ADeviceGLImpl::ChooseAppropriateSparseTexturePageSize( SPARSE_TEXTURE_TYPE Type, TEXTURE_FORMAT Format, int Width, int Height, int Depth, int * PageSizeIndex, int * PageSizeX, int * PageSizeY, int * PageSizeZ )
{
    AN_ASSERT( PageSizeIndex != nullptr );

    *PageSizeIndex = -1;

    if ( PageSizeX ) {
        *PageSizeX = 0;
    }

    if ( PageSizeY ) {
        *PageSizeY = 0;
    }

    if ( PageSizeZ ) {
        *PageSizeZ = 0;
    }

    int pageSizes;
    EnumerateSparseTexturePageSize( Type, Format, &pageSizes, nullptr, nullptr, nullptr );

    if ( pageSizes <= 0 ) {
        return false;
    }

    switch ( Type ) {
    case SPARSE_TEXTURE_2D:
    case SPARSE_TEXTURE_2D_ARRAY:
    case SPARSE_TEXTURE_CUBE_MAP:
    case SPARSE_TEXTURE_CUBE_MAP_ARRAY:
    case SPARSE_TEXTURE_RECT_GL:
    {
        int * pageSizeX = (int *)StackAlloc( pageSizes * sizeof( int ) );
        int * pageSizeY = (int *)StackAlloc( pageSizes * sizeof( int ) );
        EnumerateSparseTexturePageSize( Type, Format, &pageSizes, pageSizeX, pageSizeY, nullptr );

        for ( int i = 0 ; i < pageSizes ; i++ ) {
            if ( ( Width % pageSizeX[i] ) == 0
                 && ( Height % pageSizeY[i] ) == 0 ) {
                *PageSizeIndex = i;
                if ( PageSizeX ) {
                    *PageSizeX = pageSizeX[i];
                }
                if ( PageSizeY ) {
                    *PageSizeY = pageSizeY[i];
                }
                if ( PageSizeZ ) {
                    *PageSizeZ = 1;
                }
                break;
            }
        }
        break;
    }
    case SPARSE_TEXTURE_3D:
    {
        int * pageSizeX = (int *)StackAlloc( pageSizes * sizeof( int ) );
        int * pageSizeY = (int *)StackAlloc( pageSizes * sizeof( int ) );
        int * pageSizeZ = (int *)StackAlloc( pageSizes * sizeof( int ) );
        EnumerateSparseTexturePageSize( Type, Format, &pageSizes, pageSizeX, pageSizeY, pageSizeZ );

        for ( int i = 0 ; i < pageSizes ; i++ ) {
            if ( ( Width % pageSizeX[i] ) == 0
                 && ( Height % pageSizeY[i] ) == 0
                 && ( Depth % pageSizeZ[i] ) == 0 ) {
                *PageSizeIndex = i;
                if ( PageSizeX ) {
                    *PageSizeX = pageSizeX[i];
                }
                if ( PageSizeY ) {
                    *PageSizeY = pageSizeY[i];
                }
                if ( PageSizeZ ) {
                    *PageSizeZ = pageSizeZ[i];
                }
                break;
            }
        }
        break;
    }
    default:
        AN_ASSERT( 0 );
    }

    return *PageSizeIndex != -1;
}

bool ADeviceGLImpl::LookupImageFormat( const char * _FormatQualifier, TEXTURE_FORMAT * _Format )
{
    int numFormats = sizeof( InternalFormatLUT ) / sizeof( InternalFormatLUT[0] );
    for ( int i = 0 ; i < numFormats ; i++ ) {
        if ( !strcmp( InternalFormatLUT[ i ].ShaderImageFormatQualifier, _FormatQualifier ) ) {
            *_Format = (TEXTURE_FORMAT)i;
            return true;
        }
    }
    return false;
}

const char * ADeviceGLImpl::LookupImageFormatQualifier( TEXTURE_FORMAT _Format )
{
    return InternalFormatLUT[ _Format ].ShaderImageFormatQualifier;
}

}
