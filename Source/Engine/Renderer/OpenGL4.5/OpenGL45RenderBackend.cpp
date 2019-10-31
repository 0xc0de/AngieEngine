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

#include "OpenGL45RenderBackend.h"
#include "OpenGL45GPUSync.h"
#include "OpenGL45FrameResources.h"
#include "OpenGL45RenderTarget.h"
#include "OpenGL45ShadowMapRT.h"
#include "OpenGL45Material.h"
#include "OpenGL45CanvasPassRenderer.h"
#include "OpenGL45ShadowMapPassRenderer.h"
#include "OpenGL45DepthPassRenderer.h"
#include "OpenGL45ColorPassRenderer.h"
#include "OpenGL45WireframePassRenderer.h"
#include "OpenGL45DebugDrawPassRenderer.h"
#include "OpenGL45JointsAllocator.h"

#include <Engine/Runtime/Public/RuntimeVariable.h>
#include <Engine/Core/Public/WindowsDefs.h>

#include <GL/glew.h>

#ifdef AN_OS_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GL/wglew.h>
#endif

#ifdef AN_OS_LINUX
#include <GL/glxew.h>
#define GLFW_EXPOSE_NATIVE_GLX
#endif

#include <GLFW/glfw3.h>

FRuntimeVariable RVRenderView( _CTS("RenderView"), _CTS("1"), VAR_CHEAT );
FRuntimeVariable RVSwapInterval( _CTS("SwapInterval"), _CTS("0"), 0, _CTS("1 - enable vsync, 0 - disable vsync, -1 - tearing") );

extern thread_local char LogBuffer[16384]; // Use existing log buffer

namespace GHI {

static State * CurrentState;
void SetCurrentState( State * _State ) {
    CurrentState = _State;
}

State * GetCurrentState() {
    return CurrentState;
}

void LogPrintf( const char * _Format, ... ) {
    va_list VaList;
    va_start( VaList, _Format );
    FString::vsnprintf( LogBuffer, sizeof( LogBuffer ), _Format, VaList );
    va_end( VaList );
    GLogger.Print( LogBuffer );
}

}

namespace OpenGL45 {

FRenderBackend GOpenGL45RenderBackend;

static GHI::TEXTURE_PIXEL_FORMAT PixelFormatTable[256];
static GHI::INTERNAL_PIXEL_FORMAT InternalPixelFormatTable[256];

static GLFWwindow * WindowHandle;
static bool bSwapControl = false;
static bool bSwapControlTear = false;

//
// GHI import
//

static int GHIImport_Hash( const unsigned char * _Data, int _Size ) {
    return FCore::Hash( ( const char * )_Data, _Size );
}

static void * GHIImport_Allocate( size_t _BytesCount ) {
    return GZoneMemory.Alloc( _BytesCount, 1 );
}

static void GHIImport_Deallocate( void * _Bytes ) {
    GZoneMemory.Dealloc( _Bytes );
}

void FRenderBackend::PreInit() {
    glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
    // Possible APIs: GLFW_OPENGL_ES_API

    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    // Possible profiles: GLFW_OPENGL_ANY_PROFILE, GLFW_OPENGL_COMPAT_PROFILE

    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
    glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE );

    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 5 );

    //GLFW_CONTEXT_RELEASE_BEHAVIOR = GLFW_ANY_RELEASE_BEHAVIOR/GLFW_RELEASE_BEHAVIOR_FLUSH/GLFW_RELEASE_BEHAVIOR_NONE
    //GLFW_CONTEXT_NO_ERROR         = false/true
    //GLFW_CONTEXT_CREATION_API     = GLFW_NATIVE_CONTEXT_API/GLFW_EGL_CONTEXT_API
    //GLFW_CONTEXT_REVISION           (read only, glfwGetWindowAttrib)
    //GLFW_CONTEXT_ROBUSTNESS       = GLFW_NO_ROBUSTNESS/GLFW_NO_RESET_NOTIFICATION/GLFW_LOSE_CONTEXT_ON_RESET

    glfwWindowHint( GLFW_RED_BITS, 8 );
    glfwWindowHint( GLFW_GREEN_BITS, 8 );
    glfwWindowHint( GLFW_BLUE_BITS, 8 );
    glfwWindowHint( GLFW_ALPHA_BITS, 8 );
    glfwWindowHint( GLFW_DEPTH_BITS, 0 );   // 24
    glfwWindowHint( GLFW_STENCIL_BITS, 0 ); // 8
    glfwWindowHint( GLFW_ACCUM_RED_BITS, 0 );
    glfwWindowHint( GLFW_ACCUM_GREEN_BITS, 0 );
    glfwWindowHint( GLFW_ACCUM_BLUE_BITS, 0 );
    glfwWindowHint( GLFW_ACCUM_ALPHA_BITS, 0 );
    glfwWindowHint( GLFW_AUX_BUFFERS, 0 );
    glfwWindowHint( GLFW_DOUBLEBUFFER, 1 );

    glfwWindowHint( GLFW_SRGB_CAPABLE, 1 );
    glfwWindowHint( GLFW_SAMPLES, 0 );
    glfwWindowHint( GLFW_STEREO, 0 );
}

void FRenderBackend::Initialize( void * _NativeWindowHandle ) {
    using namespace GHI;

    GLogger.Printf( "Initializing OpenGL backend...\n" );

    WindowHandle = (GLFWwindow *)_NativeWindowHandle;

    // Set context for primary display
    glfwMakeContextCurrent( WindowHandle );

    glfwSwapInterval(0);

    glewExperimental = true;
    GLenum result = glewInit();
    if ( result != GLEW_OK ) {
        GLogger.Printf( "Failed to load OpenGL functions\n" );
        // ...
    }

    const char * vendorString = (const char *)glGetString( GL_VENDOR );
    vendorString = vendorString ? vendorString : FString::NullCString();

    const char * adapterString = (const char *)glGetString( GL_RENDERER );
    adapterString = adapterString ? adapterString : "Unknown";
#if 0
    if ( strstr( vendorString, "NVIDIA" ) != NULL ) {
        GPUVendor = GPU_VENDOR_NVIDIA;
    } else if ( strstr( vendorString, "ATI" ) != NULL
             || strstr( vendorString, "AMD" ) != NULL ) {
        GPUVendor = GPU_VENDOR_ATI;
    } else if ( strstr( vendorString, "INTEL" ) != NULL ) {
        GPUVendor = GPU_VENDOR_INTEL;
    } else {
        GPUVendor = GPU_VENDOR_OTHER;
    }
#endif
    GLogger.Printf( "Graphics vendor: %s\n", vendorString );
    GLogger.Printf( "Graphics adapter: %s\n", adapterString );
#if 0
    FMemoryInfo gpuMemoryInfo = GetGPUMemoryInfo();

    if ( gpuMemoryInfo.TotalAvailableMegabytes > 0 && gpuMemoryInfo.CurrentAvailableMegabytes > 0 ) {
        GLogger.Printf( "Total available GPU memory: %d Megs\n", gpuMemoryInfo.TotalAvailableMegabytes );
        GLogger.Printf( "Current available GPU memory: %d Megs\n", gpuMemoryInfo.CurrentAvailableMegabytes );
    }
#endif

#if defined( AN_OS_WIN32 )
    bSwapControl = !!WGLEW_EXT_swap_control;
    bSwapControlTear = !!WGLEW_EXT_swap_control_tear;
#elif defined( AN_OS_LINUX )
    bSwapControl = !!GLXEW_EXT_swap_control || !!GLXEW_MESA_swap_control || !!GLXEW_SGI_swap_control;
    bSwapControlTear = !!GLXEW_EXT_swap_control_tear;
#else
#error "Swap control tear checking not implemented on current platform"
#endif

    AllocatorCallback allocator;

    allocator.Allocate = GHIImport_Allocate;
    allocator.Deallocate = GHIImport_Deallocate;

    GDevice.Initialize( &allocator, GHIImport_Hash );

    StateCreateInfo stateCreateInfo = {};
    stateCreateInfo.ClipControl = CLIP_CONTROL_DIRECTX;
    stateCreateInfo.ViewportOrigin = VIEWPORT_ORIGIN_TOP_LEFT;

    GState.Initialize( &GDevice, stateCreateInfo );

    SetCurrentState( &GState );

    ZeroMem( PixelFormatTable, sizeof( PixelFormatTable ) );
    ZeroMem( InternalPixelFormatTable, sizeof( InternalPixelFormatTable ) );

    PixelFormatTable[TEXTURE_PF_R8_SIGNED] = PIXEL_FORMAT_BYTE_R;
    PixelFormatTable[TEXTURE_PF_RG8_SIGNED] = PIXEL_FORMAT_BYTE_RG;
    PixelFormatTable[TEXTURE_PF_BGR8_SIGNED] = PIXEL_FORMAT_BYTE_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA8_SIGNED] = PIXEL_FORMAT_BYTE_BGRA;

    PixelFormatTable[TEXTURE_PF_R8] = PIXEL_FORMAT_UBYTE_R;
    PixelFormatTable[TEXTURE_PF_RG8] = PIXEL_FORMAT_UBYTE_RG;
    PixelFormatTable[TEXTURE_PF_BGR8] = PIXEL_FORMAT_UBYTE_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA8] = PIXEL_FORMAT_UBYTE_BGRA;

    PixelFormatTable[TEXTURE_PF_BGR8_SRGB] = PIXEL_FORMAT_UBYTE_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA8_SRGB] = PIXEL_FORMAT_UBYTE_BGRA;

    PixelFormatTable[TEXTURE_PF_R16_SIGNED] = PIXEL_FORMAT_SHORT_R;
    PixelFormatTable[TEXTURE_PF_RG16_SIGNED] = PIXEL_FORMAT_SHORT_RG;
    PixelFormatTable[TEXTURE_PF_BGR16_SIGNED] = PIXEL_FORMAT_SHORT_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA16_SIGNED] = PIXEL_FORMAT_SHORT_BGRA;

    PixelFormatTable[TEXTURE_PF_R16] = PIXEL_FORMAT_USHORT_R;
    PixelFormatTable[TEXTURE_PF_RG16] = PIXEL_FORMAT_USHORT_RG;
    PixelFormatTable[TEXTURE_PF_BGR16] = PIXEL_FORMAT_USHORT_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA16] = PIXEL_FORMAT_USHORT_BGRA;

    PixelFormatTable[TEXTURE_PF_R32_SIGNED] = PIXEL_FORMAT_INT_R;
    PixelFormatTable[TEXTURE_PF_RG32_SIGNED] = PIXEL_FORMAT_INT_RG;
    PixelFormatTable[TEXTURE_PF_BGR32_SIGNED] = PIXEL_FORMAT_INT_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA32_SIGNED] = PIXEL_FORMAT_INT_BGRA;

    PixelFormatTable[TEXTURE_PF_R32] = PIXEL_FORMAT_UINT_R;
    PixelFormatTable[TEXTURE_PF_RG32] = PIXEL_FORMAT_UINT_RG;
    PixelFormatTable[TEXTURE_PF_BGR32] = PIXEL_FORMAT_UINT_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA32] = PIXEL_FORMAT_UINT_BGRA;

    PixelFormatTable[TEXTURE_PF_R16F] = PIXEL_FORMAT_HALF_R;
    PixelFormatTable[TEXTURE_PF_RG16F] = PIXEL_FORMAT_HALF_RG;
    PixelFormatTable[TEXTURE_PF_BGR16F] = PIXEL_FORMAT_HALF_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA16F] = PIXEL_FORMAT_HALF_BGRA;

    PixelFormatTable[TEXTURE_PF_R32F] = PIXEL_FORMAT_FLOAT_R;
    PixelFormatTable[TEXTURE_PF_RG32F] = PIXEL_FORMAT_FLOAT_RG;
    PixelFormatTable[TEXTURE_PF_BGR32F] = PIXEL_FORMAT_FLOAT_BGR;
    PixelFormatTable[TEXTURE_PF_BGRA32F] = PIXEL_FORMAT_FLOAT_BGRA;

    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_DXT1] = PIXEL_FORMAT_COMPRESSED_RGB_DXT1;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT1] = PIXEL_FORMAT_COMPRESSED_RGBA_DXT1;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT3] = PIXEL_FORMAT_COMPRESSED_RGBA_DXT3;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT5] = PIXEL_FORMAT_COMPRESSED_RGBA_DXT5;

    PixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_DXT1] = PIXEL_FORMAT_COMPRESSED_SRGB_DXT1;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT1] = PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_DXT1;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT3] = PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_DXT3;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT5] = PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_DXT5;

    PixelFormatTable[TEXTURE_PF_COMPRESSED_RED_RGTC1] = PIXEL_FORMAT_COMPRESSED_RED_RGTC1;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RG_RGTC2] = PIXEL_FORMAT_COMPRESSED_RG_RGTC2;

    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_BPTC_UNORM] = PIXEL_FORMAT_COMPRESSED_RGBA_BPTC_UNORM;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_BPTC_UNORM] = PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_BPTC_SIGNED_FLOAT] = PIXEL_FORMAT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
    PixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT] = PIXEL_FORMAT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;




    // FIXME: ?
    //    INTERNAL_PIXEL_FORMAT_R8_SNORM,       // RED   s8
    //    INTERNAL_PIXEL_FORMAT_RG8_SNORM,      // RG    s8  s8
    //    INTERNAL_PIXEL_FORMAT_RGB8_SNORM,     // RGB   s8  s8  s8
    //    INTERNAL_PIXEL_FORMAT_RGBA8_SNORM,    // RGBA  s8  s8  s8  s8
    InternalPixelFormatTable[TEXTURE_PF_R8_SIGNED] = INTERNAL_PIXEL_FORMAT_R8I;
    InternalPixelFormatTable[TEXTURE_PF_RG8_SIGNED] = INTERNAL_PIXEL_FORMAT_RG8I;
    InternalPixelFormatTable[TEXTURE_PF_BGR8_SIGNED] = INTERNAL_PIXEL_FORMAT_RGB8I;
    InternalPixelFormatTable[TEXTURE_PF_BGRA8_SIGNED] = INTERNAL_PIXEL_FORMAT_RGBA8I;

    //    INTERNAL_PIXEL_FORMAT_R8UI,           // RED   ui8
    //    INTERNAL_PIXEL_FORMAT_RG8UI,          // RG    ui8   ui8
    //    INTERNAL_PIXEL_FORMAT_RGB8UI,         // RGB   ui8  ui8  ui8
    //    INTERNAL_PIXEL_FORMAT_RGBA8UI,        // RGBA  ui8  ui8  ui8  ui8
    InternalPixelFormatTable[TEXTURE_PF_R8] = INTERNAL_PIXEL_FORMAT_R8;
    InternalPixelFormatTable[TEXTURE_PF_RG8] = INTERNAL_PIXEL_FORMAT_RG8;
    InternalPixelFormatTable[TEXTURE_PF_BGR8] = INTERNAL_PIXEL_FORMAT_RGB8;
    InternalPixelFormatTable[TEXTURE_PF_BGRA8] = INTERNAL_PIXEL_FORMAT_RGBA8;

    InternalPixelFormatTable[TEXTURE_PF_BGR8_SRGB] = INTERNAL_PIXEL_FORMAT_SRGB8;
    InternalPixelFormatTable[TEXTURE_PF_BGRA8_SRGB] = INTERNAL_PIXEL_FORMAT_SRGB8_ALPHA8;

    // FIXME: ?
    //    INTERNAL_PIXEL_FORMAT_R16_SNORM,      // RED   s16
    //    INTERNAL_PIXEL_FORMAT_RG16_SNORM,     // RG    s16 s16
    //    INTERNAL_PIXEL_FORMAT_RGB16_SNORM,    // RGB   s16  s16  s16
    //    INTERNAL_PIXEL_FORMAT_RGBA16_SNORM,   // RGBA  s16  s16  s16  s16
    InternalPixelFormatTable[TEXTURE_PF_R16_SIGNED] = INTERNAL_PIXEL_FORMAT_R16I;
    InternalPixelFormatTable[TEXTURE_PF_RG16_SIGNED] = INTERNAL_PIXEL_FORMAT_RG16I;
    InternalPixelFormatTable[TEXTURE_PF_BGR16_SIGNED] = INTERNAL_PIXEL_FORMAT_RGB16I;
    InternalPixelFormatTable[TEXTURE_PF_BGRA16_SIGNED] = INTERNAL_PIXEL_FORMAT_RGBA16I;

    // FIXME: ?
    //    INTERNAL_PIXEL_FORMAT_R16,            // RED   16
    //    INTERNAL_PIXEL_FORMAT_RG16,           // RG    16  16
    //    INTERNAL_PIXEL_FORMAT_RGB16,          // RGB   16  16  16
    //    INTERNAL_PIXEL_FORMAT_RGBA16,         // RGBA  16  16  16  16
    InternalPixelFormatTable[TEXTURE_PF_R16] = INTERNAL_PIXEL_FORMAT_R16UI;
    InternalPixelFormatTable[TEXTURE_PF_RG16] = INTERNAL_PIXEL_FORMAT_RG16UI;
    InternalPixelFormatTable[TEXTURE_PF_BGR16] = INTERNAL_PIXEL_FORMAT_RGB16UI;
    InternalPixelFormatTable[TEXTURE_PF_BGRA16] = INTERNAL_PIXEL_FORMAT_RGBA16UI;

    InternalPixelFormatTable[TEXTURE_PF_R32_SIGNED] = INTERNAL_PIXEL_FORMAT_R32I;
    InternalPixelFormatTable[TEXTURE_PF_RG32_SIGNED] = INTERNAL_PIXEL_FORMAT_RG32I;
    InternalPixelFormatTable[TEXTURE_PF_BGR32_SIGNED] = INTERNAL_PIXEL_FORMAT_RGB32I;
    InternalPixelFormatTable[TEXTURE_PF_BGRA32_SIGNED] = INTERNAL_PIXEL_FORMAT_RGBA32I;

    InternalPixelFormatTable[TEXTURE_PF_R32] = INTERNAL_PIXEL_FORMAT_R32UI;
    InternalPixelFormatTable[TEXTURE_PF_RG32] = INTERNAL_PIXEL_FORMAT_RG32UI;
    InternalPixelFormatTable[TEXTURE_PF_BGR32] = INTERNAL_PIXEL_FORMAT_RGB32UI;
    InternalPixelFormatTable[TEXTURE_PF_BGRA32] = INTERNAL_PIXEL_FORMAT_RGBA32UI;

    InternalPixelFormatTable[TEXTURE_PF_R16F] = INTERNAL_PIXEL_FORMAT_R16F;
    InternalPixelFormatTable[TEXTURE_PF_RG16F] = INTERNAL_PIXEL_FORMAT_RG16F;
    InternalPixelFormatTable[TEXTURE_PF_BGR16F] = INTERNAL_PIXEL_FORMAT_RGB16F;
    InternalPixelFormatTable[TEXTURE_PF_BGRA16F] = INTERNAL_PIXEL_FORMAT_RGBA16F;

    InternalPixelFormatTable[TEXTURE_PF_R32F] = INTERNAL_PIXEL_FORMAT_R32F;
    InternalPixelFormatTable[TEXTURE_PF_RG32F] = INTERNAL_PIXEL_FORMAT_RG32F;
    InternalPixelFormatTable[TEXTURE_PF_BGR32F] = INTERNAL_PIXEL_FORMAT_RGB32F;
    InternalPixelFormatTable[TEXTURE_PF_BGRA32F] = INTERNAL_PIXEL_FORMAT_RGBA32F;

    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_DXT1] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGB_S3TC_DXT1;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT1] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGBA_S3TC_DXT1;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT3] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGBA_S3TC_DXT3;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_DXT5] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGBA_S3TC_DXT5;

    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_DXT1] = INTERNAL_PIXEL_FORMAT_COMPRESSED_SRGB_S3TC_DXT1;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT1] = INTERNAL_PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT3] = INTERNAL_PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_DXT5] = INTERNAL_PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5;

    // FIXME: ?
    // INTERNAL_PIXEL_FORMAT_COMPRESSED_SIGNED_RED_RGTC1
    // INTERNAL_PIXEL_FORMAT_COMPRESSED_SIGNED_RG_RGTC2
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RED_RGTC1] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RED_RGTC1;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RG_RGTC2] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RG_RGTC2;

    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGBA_BPTC_UNORM] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGBA_BPTC_UNORM;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_SRGB_ALPHA_BPTC_UNORM] = INTERNAL_PIXEL_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_BPTC_SIGNED_FLOAT] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
    InternalPixelFormatTable[TEXTURE_PF_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT] = INTERNAL_PIXEL_FORMAT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;

    // Unused formats:
    //    INTERNAL_PIXEL_FORMAT_R3_G3_B2,       // RGB   3  3  2
    //    INTERNAL_PIXEL_FORMAT_RGB4,           // RGB   4  4  4
    //    INTERNAL_PIXEL_FORMAT_RGB5,           // RGB   5  5  5
    //    INTERNAL_PIXEL_FORMAT_RGB10,          // RGB   10  10  10
    //    INTERNAL_PIXEL_FORMAT_RGB12,          // RGB   12  12  12
    //    INTERNAL_PIXEL_FORMAT_RGBA2,          // RGB   2  2  2  2
    //    INTERNAL_PIXEL_FORMAT_RGBA4,          // RGB   4  4  4  4
    //    INTERNAL_PIXEL_FORMAT_RGB5_A1,        // RGBA  5  5  5  1
    //    INTERNAL_PIXEL_FORMAT_RGB10_A2,       // RGBA  10  10  10  2
    //    INTERNAL_PIXEL_FORMAT_RGB10_A2UI,     // RGBA  ui10  ui10  ui10  ui2
    //    INTERNAL_PIXEL_FORMAT_RGBA12,         // RGBA  12  12  12  12
    //    INTERNAL_PIXEL_FORMAT_R11F_G11F_B10F, // RGB   f11  f11  f10
    //    INTERNAL_PIXEL_FORMAT_RGB9_E5,        // RGB   9  9  9     5


    GJointsAllocator.Initialize();
    GRenderTarget.Initialize();
    GShadowMapRT.Initialize();
    GShadowMapPassRenderer.Initialize();
    GDepthPassRenderer.Initialize();
    GColorPassRenderer.Initialize();
    GWireframePassRenderer.Initialize();
    GDebugDrawPassRenderer.Initialize();
    GCanvasPassRenderer.Initialize();
    GFrameResources.Initialize();
}

void FRenderBackend::Deinitialize() {
    GLogger.Printf( "Deinitializing OpenGL backend...\n" );

    GJointsAllocator.Deinitialize();
    GRenderTarget.Deinitialize();
    GShadowMapRT.Deinitialize();
    GShadowMapPassRenderer.Deinitialize();
    GDepthPassRenderer.Deinitialize();
    GColorPassRenderer.Deinitialize();
    GWireframePassRenderer.Deinitialize();
    GDebugDrawPassRenderer.Deinitialize();
    GCanvasPassRenderer.Deinitialize();
    GFrameResources.Deinitialize();
    GOpenGL45GPUSync.Release();

    GState.Deinitialize();
    GDevice.Deinitialize();
}

void FRenderBackend::WaitGPU() {
    GOpenGL45GPUSync.Wait();
}

void FRenderBackend::SetGPUEvent() {
    GOpenGL45GPUSync.SetEvent();
}

FTextureGPU * FRenderBackend::CreateTexture( IGPUResourceOwner * _Owner ) {
    void * pData = GZoneMemory.AllocCleared( sizeof( FTextureGPU ), 1 );
    FTextureGPU * texture = new (pData) FTextureGPU;
    texture->pOwner = _Owner;
    texture->pHandleGPU = GZoneMemory.AllocCleared( sizeof( GHI::Texture ), 1 );
    new (texture->pHandleGPU) GHI::Texture;
    RegisterGPUResource( texture );
    return texture;
}

void FRenderBackend::DestroyTexture( FTextureGPU * _Texture ) {
    using namespace GHI;
    UnregisterGPUResource( _Texture );
    GHI::Texture * texture = GPUTextureHandle( _Texture );
    texture->~Texture();
    GZoneMemory.Dealloc( _Texture->pHandleGPU );
    _Texture->~FTextureGPU();
    GZoneMemory.Dealloc( _Texture );
}

void FRenderBackend::InitializeTexture1D( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_1D;
    textureCI.Resolution.Tex1D.Width = _Width;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTexture1DArray( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_1D_ARRAY;
    textureCI.Resolution.Tex1DArray.Width = _Width;
    textureCI.Resolution.Tex1DArray.NumLayers = _ArraySize;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTexture2D( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_2D;
    textureCI.Resolution.Tex2D.Width = _Width;
    textureCI.Resolution.Tex2D.Height = _Height;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTexture2DArray( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _ArraySize ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_2D_ARRAY;
    textureCI.Resolution.Tex2DArray.Width = _Width;
    textureCI.Resolution.Tex2DArray.Height = _Height;
    textureCI.Resolution.Tex2DArray.NumLayers = _ArraySize;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTexture3D( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height, int _Depth ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_3D;
    textureCI.Resolution.Tex3D.Width = _Width;
    textureCI.Resolution.Tex3D.Height = _Height;
    textureCI.Resolution.Tex3D.Depth = _Depth;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTextureCubemap( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_CUBE_MAP;
    textureCI.Resolution.TexCubemap.Width = _Width;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTextureCubemapArray( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _ArraySize ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_CUBE_MAP_ARRAY;
    textureCI.Resolution.TexCubemapArray.Width = _Width;
    textureCI.Resolution.TexCubemapArray.NumLayers = _ArraySize;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::InitializeTexture2DNPOT( FTextureGPU * _Texture, ETexturePixelFormat _PixelFormat, int _NumLods, int _Width, int _Height ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureStorageCreateInfo textureCI = {};
    textureCI.Type = GHI::TEXTURE_RECT;
    textureCI.Resolution.TexRect.Width = _Width;
    textureCI.Resolution.TexRect.Height = _Height;
    textureCI.InternalFormat = InternalPixelFormatTable[_PixelFormat];
    textureCI.NumLods = _NumLods;

    texture->InitializeStorage( textureCI );
}

void FRenderBackend::WriteTexture( FTextureGPU * _Texture, FTextureRect const & _Rectangle, ETexturePixelFormat _PixelFormat, size_t _SizeInBytes, unsigned int _Alignment, const void * _SysMem ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureRect rect;
    rect.Offset.X = _Rectangle.Offset.X;
    rect.Offset.Y = _Rectangle.Offset.Y;
    rect.Offset.Z = _Rectangle.Offset.Z;
    rect.Offset.Lod = _Rectangle.Offset.Lod;
    rect.Dimension.X = _Rectangle.Dimension.X;
    rect.Dimension.Y = _Rectangle.Dimension.Y;
    rect.Dimension.Z = _Rectangle.Dimension.Z;

    texture->WriteRect( rect, PixelFormatTable[_PixelFormat], _SizeInBytes, _Alignment, _SysMem );
}

void FRenderBackend::ReadTexture( FTextureGPU * _Texture, FTextureRect const & _Rectangle, ETexturePixelFormat _PixelFormat, size_t _SizeInBytes, unsigned int _Alignment, void * _SysMem ) {
    GHI::Texture * texture = GPUTextureHandle( _Texture );

    GHI::TextureRect rect;
    rect.Offset.X = _Rectangle.Offset.X;
    rect.Offset.Y = _Rectangle.Offset.Y;
    rect.Offset.Z = _Rectangle.Offset.Z;
    rect.Offset.Lod = _Rectangle.Offset.Lod;
    rect.Dimension.X = _Rectangle.Dimension.X;
    rect.Dimension.Y = _Rectangle.Dimension.Y;
    rect.Dimension.Z = _Rectangle.Dimension.Z;

    texture->ReadRect( rect, PixelFormatTable[_PixelFormat], _SizeInBytes, _Alignment, _SysMem );
}

FBufferGPU * FRenderBackend::CreateBuffer( IGPUResourceOwner * _Owner ) {
    void * pData = GZoneMemory.AllocCleared( sizeof( FBufferGPU ), 1 );
    FBufferGPU * buffer = new (pData) FBufferGPU;
    buffer->pOwner = _Owner;
    buffer->pHandleGPU = GZoneMemory.AllocCleared( sizeof( GHI::Buffer ), 1 );
    new (buffer->pHandleGPU) GHI::Buffer;
    RegisterGPUResource( buffer );
    return buffer;
}

void FRenderBackend::DestroyBuffer( FBufferGPU * _Buffer ) {
    using namespace GHI;
    UnregisterGPUResource( _Buffer );
    GHI::Buffer * texture = GPUBufferHandle( _Buffer );
    texture->~Buffer();
    GZoneMemory.Dealloc( _Buffer->pHandleGPU );
    _Buffer->~FBufferGPU();
    GZoneMemory.Dealloc( _Buffer );
}

void FRenderBackend::InitializeBuffer( FBufferGPU * _Buffer, size_t _SizeInBytes, bool _DynamicStorage ) {
    GHI::Buffer * buffer = GPUBufferHandle( _Buffer );

    GHI::BufferCreateInfo bufferCI = {};
    if ( _DynamicStorage ) {
        bufferCI.ImmutableStorageFlags = GHI::IMMUTABLE_DYNAMIC_STORAGE;
        bufferCI.bImmutableStorage = true;

        //bufferCI.bImmutableStorage = false;
        //bufferCI.MutableClientAccess = GHI::MUTABLE_STORAGE_CLIENT_WRITE_ONLY;
        //bufferCI.MutableUsage = GHI::MUTABLE_STORAGE_STREAM;
        //bufferCI.ImmutableStorageFlags = (GHI::IMMUTABLE_STORAGE_FLAGS)0;
    } else {
        bufferCI.MutableClientAccess = GHI::MUTABLE_STORAGE_CLIENT_WRITE_ONLY;
        bufferCI.MutableUsage = GHI::MUTABLE_STORAGE_STATIC;
#if 1
        // Mutable storage with flag MUTABLE_STORAGE_STATIC is much faster during rendering (tested on NVidia GeForce GTX 770)
        bufferCI.ImmutableStorageFlags = (GHI::IMMUTABLE_STORAGE_FLAGS)0;
        bufferCI.bImmutableStorage = false;
#else
        bufferCI.ImmutableStorageFlags = GHI_ImmutableMapWrite | GHI_ImmutableMapPersistent | GHI_ImmutableMapCoherent;
        bufferCI.bImmutableStorage = true;
#endif
    }

    bufferCI.SizeInBytes = _SizeInBytes;

    buffer->Initialize( bufferCI );
}

void FRenderBackend::WriteBuffer( FBufferGPU * _Buffer, size_t _ByteOffset, size_t _SizeInBytes, const void * _SysMem ) {
    GHI::Buffer * buffer = GPUBufferHandle( _Buffer );

    buffer->WriteRange( _ByteOffset, _SizeInBytes, _SysMem );
}

void FRenderBackend::ReadBuffer( FBufferGPU * _Buffer, size_t _ByteOffset, size_t _SizeInBytes, void * _SysMem ) {
    GHI::Buffer * buffer = GPUBufferHandle( _Buffer );

    buffer->ReadRange( _ByteOffset, _SizeInBytes, _SysMem );
}

FMaterialGPU * FRenderBackend::CreateMaterial( IGPUResourceOwner * _Owner ) {
    void * pData = GZoneMemory.AllocCleared( sizeof( FMaterialGPU ), 1 );
    FMaterialGPU * material = new (pData) FMaterialGPU;
    material->pOwner = _Owner;
    RegisterGPUResource( material );
    return material;
}

void FRenderBackend::DestroyMaterial( FMaterialGPU * _Material ) {
    using namespace GHI;

    UnregisterGPUResource( _Material );

    FShadeModelLit * Lit = (FShadeModelLit *)_Material->ShadeModel.Lit;
    FShadeModelUnlit* Unlit = (FShadeModelUnlit *)_Material->ShadeModel.Unlit;
    FShadeModelHUD * HUD = (FShadeModelHUD *)_Material->ShadeModel.HUD;

    if ( Lit ) {
        Lit->~FShadeModelLit();
        GZoneMemory.Dealloc( Lit );
    }

    if ( Unlit ) {
        Unlit->~FShadeModelUnlit();
        GZoneMemory.Dealloc( Unlit );
    }

    if ( HUD ) {
        HUD->~FShadeModelHUD();
        GZoneMemory.Dealloc( HUD );
    }

    _Material->~FMaterialGPU();
    GZoneMemory.Dealloc( _Material );
}

void FRenderBackend::InitializeMaterial( FMaterialGPU * _Material, FMaterialBuildData const * _BuildData ) {
    using namespace GHI;

    _Material->MaterialType = _BuildData->Type;
    _Material->LightmapSlot = _BuildData->LightmapSlot;
    _Material->bDepthPassTextureFetch     = _BuildData->bDepthPassTextureFetch;
    _Material->bColorPassTextureFetch     = _BuildData->bColorPassTextureFetch;
    _Material->bWireframePassTextureFetch = _BuildData->bWireframePassTextureFetch;
    _Material->bShadowMapPassTextureFetch = _BuildData->bShadowMapPassTextureFetch;
    _Material->bHasVertexDeform = _BuildData->bHasVertexDeform;
    _Material->bNoCastShadow    = _BuildData->bNoCastShadow;
    _Material->bShadowMapMasking= _BuildData->bShadowMapMasking;
    _Material->NumSamplers      = _BuildData->NumSamplers;

    static constexpr POLYGON_CULL PolygonCullLUT[] = {
        POLYGON_CULL_FRONT,
        POLYGON_CULL_BACK,
        POLYGON_CULL_DISABLED
    };

    POLYGON_CULL cullMode = PolygonCullLUT[_BuildData->Facing];

    FShadeModelLit   * Lit   = (FShadeModelLit *)_Material->ShadeModel.Lit;
    FShadeModelUnlit * Unlit = (FShadeModelUnlit *)_Material->ShadeModel.Unlit;
    FShadeModelHUD   * HUD   = (FShadeModelHUD *)_Material->ShadeModel.HUD;

    if ( Lit ) {
        Lit->~FShadeModelLit();
        GZoneMemory.Dealloc( Lit );
        _Material->ShadeModel.Lit = nullptr;
    }

    if ( Unlit ) {
        Unlit->~FShadeModelUnlit();
        GZoneMemory.Dealloc( Unlit );
        _Material->ShadeModel.Unlit = nullptr;
    }

    if ( HUD ) {
        HUD->~FShadeModelHUD();
        GZoneMemory.Dealloc( HUD );
        _Material->ShadeModel.HUD = nullptr;
    }

    switch ( _Material->MaterialType ) {
    case MATERIAL_TYPE_PBR:
    case MATERIAL_TYPE_BASELIGHT: {
        void * pMem = GZoneMemory.Alloc( sizeof( FShadeModelLit ), 1 );
        Lit = new (pMem) FShadeModelLit();
        _Material->ShadeModel.Lit = Lit;

        Lit->ColorPassSimple.Create( _BuildData->ShaderData, cullMode, false, _BuildData->bDepthTest_EXPEREMENTAL );
        Lit->ColorPassSkinned.Create( _BuildData->ShaderData, cullMode, true, _BuildData->bDepthTest_EXPEREMENTAL );

        Lit->ColorPassLightmap.Create( _BuildData->ShaderData, cullMode, _BuildData->bDepthTest_EXPEREMENTAL );
        Lit->ColorPassVertexLight.Create( _BuildData->ShaderData, cullMode, _BuildData->bDepthTest_EXPEREMENTAL );

        Lit->DepthPass.Create( _BuildData->ShaderData, cullMode, false );
        Lit->DepthPassSkinned.Create( _BuildData->ShaderData, cullMode, true );

        Lit->WireframePass.Create( _BuildData->ShaderData, cullMode, false );
        Lit->WireframePassSkinned.Create( _BuildData->ShaderData, cullMode, true );

        Lit->ShadowPass.Create( _BuildData->ShaderData, _BuildData->bShadowMapMasking, false );
        Lit->ShadowPassSkinned.Create( _BuildData->ShaderData, _BuildData->bShadowMapMasking, true );
        break;
    }

    case MATERIAL_TYPE_UNLIT: {
        void * pMem = GZoneMemory.Alloc( sizeof( FShadeModelUnlit ), 1 );
        Unlit = new (pMem) FShadeModelUnlit();
        _Material->ShadeModel.Unlit = Unlit;

        Unlit->ColorPassSimple.Create( _BuildData->ShaderData, cullMode, false, _BuildData->bDepthTest_EXPEREMENTAL );
        Unlit->ColorPassSkinned.Create( _BuildData->ShaderData, cullMode, true, _BuildData->bDepthTest_EXPEREMENTAL );

        Unlit->DepthPass.Create( _BuildData->ShaderData, cullMode, false );
        Unlit->DepthPassSkinned.Create( _BuildData->ShaderData, cullMode, true );

        Unlit->WireframePass.Create( _BuildData->ShaderData, cullMode, false );
        Unlit->WireframePassSkinned.Create( _BuildData->ShaderData, cullMode, true );

        Unlit->ShadowPass.Create( _BuildData->ShaderData, _BuildData->bShadowMapMasking, false );
        Unlit->ShadowPassSkinned.Create( _BuildData->ShaderData, _BuildData->bShadowMapMasking, true );
        break;
    }

    case MATERIAL_TYPE_HUD:
    case MATERIAL_TYPE_POSTPROCESS: {
        void * pMem = GZoneMemory.Alloc( sizeof( FShadeModelHUD ), 1 );
        HUD = new (pMem) FShadeModelHUD();
        _Material->ShadeModel.HUD = HUD;
        HUD->ColorPassHUD.Create( _BuildData->ShaderData );
        break;
    }

    default:
        AN_Assert( 0 );
    }

    SamplerCreateInfo samplerCI;

    samplerCI.Filter = FILTER_MIN_NEAREST_MIPMAP_LINEAR_MAG_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressV = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressW = SAMPLER_ADDRESS_WRAP;
    samplerCI.MipLODBias = 0;
    samplerCI.MaxAnisotropy = 0;
    samplerCI.ComparisonFunc = CMPFUNC_LEQUAL;
    samplerCI.bCompareRefToTexture = false;
    samplerCI.BorderColor[0] = 0;
    samplerCI.BorderColor[1] = 0;
    samplerCI.BorderColor[2] = 0;
    samplerCI.BorderColor[3] = 0;
    samplerCI.MinLOD = -1000;
    samplerCI.MaxLOD = 1000;

    memset( _Material->pSampler, 0, sizeof( _Material->pSampler ) );

    static constexpr SAMPLER_FILTER SamplerFilterLUT[] = {
        FILTER_LINEAR,
        FILTER_NEAREST,
        FILTER_MIPMAP_NEAREST,
        FILTER_MIPMAP_BILINEAR,
        FILTER_MIPMAP_NLINEAR,
        FILTER_MIPMAP_TRILINEAR
    };

    static constexpr SAMPLER_ADDRESS_MODE SamplerAddressLUT[] = {
        SAMPLER_ADDRESS_WRAP,
        SAMPLER_ADDRESS_MIRROR,
        SAMPLER_ADDRESS_CLAMP,
        SAMPLER_ADDRESS_BORDER,
        SAMPLER_ADDRESS_MIRROR_ONCE
    };

    for ( int i = 0 ; i < _BuildData->NumSamplers ; i++ ) {
        FTextureSampler const * desc = &_BuildData->Samplers[i];

        samplerCI.Filter = SamplerFilterLUT[ desc->Filter ];
        samplerCI.AddressU = SamplerAddressLUT[ desc->AddressU ];
        samplerCI.AddressV = SamplerAddressLUT[ desc->AddressV ];
        samplerCI.AddressW = SamplerAddressLUT[ desc->AddressW ];
        samplerCI.MipLODBias = desc->MipLODBias;
        samplerCI.MaxAnisotropy = desc->Anisotropy;
        samplerCI.MinLOD = desc->MinLod;
        samplerCI.MaxLOD = desc->MaxLod;
        samplerCI.bCubemapSeamless = true; // FIXME: use desc->bCubemapSeamless ?

        _Material->pSampler[i] = GDevice.GetOrCreateSampler( samplerCI );
    }
}

size_t FRenderBackend::AllocateJoints( size_t _JointsCount ) {
    return GJointsAllocator.AllocJoints( _JointsCount );
}

void FRenderBackend::WriteJoints( size_t _Offset, size_t _JointsCount, Float3x4 const * _Matrices ) {
    GJointsAllocator.Buffer.WriteRange( _Offset, _JointsCount * sizeof( Float3x4 ), _Matrices );
}

void FRenderBackend::RenderFrame( FRenderFrame * _FrameData ) {
    GFrameData = _FrameData;

    GJointsAllocator.Reset();

    GState.SetSwapChainResolution( GFrameData->CanvasWidth, GFrameData->CanvasHeight );

    glEnable( GL_FRAMEBUFFER_SRGB );

    GDebugDrawPassRenderer.UploadBuffers();

    GRenderTarget.ReallocSurface( GFrameData->AllocSurfaceWidth, GFrameData->AllocSurfaceHeight );

    // Calc canvas projection matrix
    const Float2 orthoMins( 0.0f, (float)GFrameData->CanvasHeight );
    const Float2 orthoMaxs( (float)GFrameData->CanvasWidth, 0.0f );
    GFrameResources.ViewUniformBufferUniformData.OrthoProjection = Float4x4::Ortho2DCC( orthoMins, orthoMaxs );
    GFrameResources.ViewUniformBuffer.WriteRange(
        GHI_STRUCT_OFS( FViewUniformBuffer, OrthoProjection ),
        sizeof( GFrameResources.ViewUniformBufferUniformData.OrthoProjection ),
        &GFrameResources.ViewUniformBufferUniformData.OrthoProjection );

    GCanvasPassRenderer.RenderInstances();

    SetGPUEvent();
    SwapBuffers();
}

void FRenderBackend::RenderView( FRenderView * _RenderView ) {
    GRenderView = _RenderView;

    if ( !RVRenderView ) {
        return;
    }

    GFrameResources.UploadUniforms();

    GShadowMapPassRenderer.RenderInstances();

#ifdef DEPTH_PREPASS
    GDepthPassRenderer.RenderInstances();
#endif
    GColorPassRenderer.RenderInstances();

    if ( GRenderView->bWireframe ) {
        GWireframePassRenderer.RenderInstances();
    }

    if ( GRenderView->DbgCmdCount > 0 ) {
        GDebugDrawPassRenderer.RenderInstances();
    }
}

void OpenGL45RenderView( FRenderView * _RenderView ) {
    GOpenGL45RenderBackend.RenderView( _RenderView );
}

void FRenderBackend::SwapBuffers() {
    if ( bSwapControl ) {
        int i = FMath::Clamp( RVSwapInterval.GetInteger(), -1, 1 );
        if ( i == -1 && !bSwapControlTear ) {
            // Tearing not supported
            i = 0;
        }
        glfwSwapInterval( i );
    }

    glfwSwapBuffers( WindowHandle );
}

}
