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

#include "PipelineGLImpl.h"
#include "DeviceGLImpl.h"
#include "ImmediateContextGLImpl.h"
#include "ShaderModuleGLImpl.h"
#include "VertexArrayObjectGL.h"
#include "LUT.h"
#include "GL/glew.h"

#include <Core/Public/Logger.h>

namespace RenderCore {

APipelineGLImpl::APipelineGLImpl( ADeviceGLImpl * _Device, SPipelineCreateInfo const & _CreateInfo )
    : IPipeline( _Device ), pDevice( _Device )
{
    AImmediateContextGLImpl * ctx = AImmediateContextGLImpl::GetCurrent();

    GLuint pipelineId;

    if ( !pDevice->IsFeatureSupported( FEATURE_HALF_FLOAT_VERTEX ) ) {
        // Check half float vertex type

        for ( SVertexAttribInfo const * desc = _CreateInfo.pVertexAttribs ; desc < &_CreateInfo.pVertexAttribs[_CreateInfo.NumVertexAttribs] ; desc++ ) {
            if ( desc->TypeOfComponent() == COMPONENT_HALF ) {
                GLogger.Printf( "APipelineGLImpl::ctor: Half floats not supported by current hardware\n" );
            }
        }
    }

    // TODO: create program pipeline for each context
    glCreateProgramPipelines( 1, &pipelineId );

    pVS  = _CreateInfo.pVS;
    pTCS = _CreateInfo.pTCS;
    pTES = _CreateInfo.pTES;
    pGS  = _CreateInfo.pGS;
    pFS  = _CreateInfo.pFS;
    pCS  = _CreateInfo.pCS;

    if ( pVS ) {
        glUseProgramStages( pipelineId, GL_VERTEX_SHADER_BIT, pVS->GetHandleNativeGL() );
    }
    if ( pTCS ) {
        glUseProgramStages( pipelineId, GL_TESS_CONTROL_SHADER_BIT, pTCS->GetHandleNativeGL() );
    }
    if ( pTES ) {
        glUseProgramStages( pipelineId, GL_TESS_EVALUATION_SHADER_BIT, pTES->GetHandleNativeGL() );
    }
    if ( pGS ) {
        glUseProgramStages( pipelineId, GL_GEOMETRY_SHADER_BIT, pGS->GetHandleNativeGL() );
    }
    if ( pFS ) {
        glUseProgramStages( pipelineId, GL_FRAGMENT_SHADER_BIT, pFS->GetHandleNativeGL() );
    }
    if ( pCS ) {
        glUseProgramStages( pipelineId, GL_COMPUTE_SHADER_BIT, pCS->GetHandleNativeGL() );
    }

    glValidateProgramPipeline( pipelineId ); // 4.1

    SetHandleNativeGL( pipelineId );

    PrimitiveTopology = GL_TRIANGLES; // Use triangles by default

    if ( _CreateInfo.IA.Topology <= PRIMITIVE_TRIANGLE_STRIP_ADJ ) {
        PrimitiveTopology = PrimitiveTopologyLUT[ _CreateInfo.IA.Topology ];
        NumPatchVertices = 0;
    } else if (  _CreateInfo.IA.Topology >= PRIMITIVE_PATCHES_1 ) {
        PrimitiveTopology = GL_PATCHES;
        NumPatchVertices = _CreateInfo.IA.Topology - PRIMITIVE_PATCHES_1 + 1;  // Must be < GL_MAX_PATCH_VERTICES

        if ( NumPatchVertices > pDevice->GetDeviceCaps( DEVICE_CAPS_MAX_PATCH_VERTICES ) ) {
            GLogger.Printf( "APipelineGLImpl::ctor: num patch vertices > DEVICE_CAPS_MAX_PATCH_VERTICES\n" );
        }
    }

    bPrimitiveRestartEnabled = _CreateInfo.IA.bPrimitiveRestart;

    // Cache VAO for each context
    VAO = ctx->CachedVAO( _CreateInfo.pVertexBindings, _CreateInfo.NumVertexBindings,
                          _CreateInfo.pVertexAttribs, _CreateInfo.NumVertexAttribs );

    BlendingState = pDevice->CachedBlendingState( _CreateInfo.BS );
    RasterizerState = pDevice->CachedRasterizerState( _CreateInfo.RS );
    DepthStencilState = pDevice->CachedDepthStencilState( _CreateInfo.DSS );

    SAllocatorCallback const & allocator = pDevice->GetAllocator();

    NumSamplerObjects = _CreateInfo.ResourceLayout.NumSamplers;
    if ( NumSamplerObjects > 0 ) {
        SamplerObjects = (unsigned int *)allocator.Allocate( sizeof( SamplerObjects[0] ) * NumSamplerObjects );
        for ( int i = 0 ; i < NumSamplerObjects ; i++ ) {
            SamplerObjects[i] = pDevice->CachedSampler( _CreateInfo.ResourceLayout.Samplers[i] );
        }
    }
    else {
        SamplerObjects = nullptr;
    }

    NumImages = _CreateInfo.ResourceLayout.NumImages;
    if ( NumImages > 0 ) {
        Images = (SImageInfoGL *)allocator.Allocate( sizeof( SImageInfoGL ) * NumImages );
        for ( int i = 0 ; i < NumImages ; i++ ) {
            Images[i].AccessMode = ImageAccessModeLUT[ _CreateInfo.ResourceLayout.Images[i].AccessMode ];
            Images[i].InternalFormat = InternalFormatLUT[ _CreateInfo.ResourceLayout.Images[i].TextureFormat ].InternalFormat;
        }
    }
    else {
        Images = nullptr;
    }

    NumBuffers = _CreateInfo.ResourceLayout.NumBuffers;
    if ( NumBuffers > 0 ) {
        Buffers = (SBufferInfoGL *)allocator.Allocate( sizeof( SBufferInfoGL ) * NumBuffers );
        for ( int i = 0 ; i < NumBuffers ; i++ ) {
            Buffers[i].BufferType = BufferTargetLUT[ _CreateInfo.ResourceLayout.Buffers[i].BufferBinding ].Target;
        }
    }
    else {
        Buffers = nullptr;
    }

    pDevice->TotalPipelines++;
}

APipelineGLImpl::~APipelineGLImpl() {
    GLuint pipelineId = GetHandleNativeGL();
    if ( pipelineId ) {
        glDeleteProgramPipelines( 1, &pipelineId );
    }

    SAllocatorCallback const & allocator = pDevice->GetAllocator();

    if ( SamplerObjects ) {
        allocator.Deallocate( SamplerObjects );
    }

    if ( Images ) {
        allocator.Deallocate( Images );
    }

    if ( Buffers ) {
        allocator.Deallocate( Buffers );
    }

    pDevice->TotalPipelines--;
}

void SRenderTargetBlendingInfo::SetBlendingPreset( BLENDING_PRESET _Preset ) {
    switch ( _Preset ) {
    case BLENDING_ALPHA:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_ALPHA;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_INV_SRC_ALPHA;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_PREMULTIPLIED_ALPHA:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_INV_SRC_ALPHA;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_COLOR_ADD:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_MULTIPLY:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_DST_COLOR;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ZERO;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_SOURCE_TO_DEST:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_COLOR;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_ADD_MUL:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_INV_DST_COLOR;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_ADD_ALPHA:
        bBlendEnable = true;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_ALPHA;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    case BLENDING_NO_BLEND:
    default:
        bBlendEnable = false;
        ColorWriteMask = COLOR_WRITE_RGBA;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ZERO;
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        break;
    }
}

}
