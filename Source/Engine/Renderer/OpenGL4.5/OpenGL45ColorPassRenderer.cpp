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

#include "OpenGL45ColorPassRenderer.h"
#include "OpenGL45ShadowMapPassRenderer.h"
#include "OpenGL45FrameResources.h"
#include "OpenGL45RenderTarget.h"
#include "OpenGL45ShadowMapRT.h"
#include "OpenGL45Material.h"

using namespace GHI;

namespace OpenGL45 {

AColorPassRenderer GColorPassRenderer;

void AColorPassRenderer::Initialize() {
    RenderPassCreateInfo renderPassCI = {};
    renderPassCI.NumColorAttachments = 1;

    AttachmentInfo colorAttachment = {};
    colorAttachment.LoadOp = ATTACHMENT_LOAD_OP_LOAD;
    //colorAttachment.LoadOp = ATTACHMENT_LOAD_OP_CLEAR;
    renderPassCI.pColorAttachments = &colorAttachment;

    AttachmentInfo depthAttachment = {};
#ifdef DEPTH_PREPASS
    depthAttachment.LoadOp = ATTACHMENT_LOAD_OP_LOAD;
#else
    depthAttachment.LoadOp = ATTACHMENT_LOAD_OP_CLEAR;
#endif
    renderPassCI.pDepthStencilAttachment = &depthAttachment;

    AttachmentRef colorAttachmentRef = {};
    colorAttachmentRef.Attachment = 0;

    SubpassInfo subpass = {};
    subpass.NumColorAttachments = 1;
    subpass.pColorAttachmentRefs = &colorAttachmentRef;

    renderPassCI.NumSubpasses = 1;
    renderPassCI.pSubpasses = &subpass;

    ColorPass.Initialize( renderPassCI );

    //
    // Create samplers
    //

    SamplerCreateInfo samplerCI;
    samplerCI.SetDefaults();
    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressV = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressW = SAMPLER_ADDRESS_WRAP;
    samplerCI.MaxAnisotropy = 16;
    LightmapSampler = GDevice.GetOrCreateSampler( samplerCI );
}

void AColorPassRenderer::Deinitialize() {
    ColorPass.Deinitialize();
}

bool AColorPassRenderer::BindMaterial( SRenderInstance const * instance ) {
    AMaterialGPU * pMaterial = instance->Material;
    Pipeline * pPipeline;
    Buffer * pSecondVertexBuffer = nullptr;

    AN_ASSERT( pMaterial );

    bool bSkinned = instance->SkeletonSize > 0;
    bool bLightmap = instance->LightmapUVChannel != nullptr && instance->Lightmap;
    bool bVertexLight = instance->VertexLightChannel != nullptr;

    switch ( pMaterial->MaterialType ) {
    case MATERIAL_TYPE_UNLIT:

        pPipeline = bSkinned ? &((AShadeModelUnlit*)pMaterial->ShadeModel.Unlit)->ColorPassSkinned
                             : &((AShadeModelUnlit*)pMaterial->ShadeModel.Unlit)->ColorPassSimple;

        pSecondVertexBuffer = bSkinned ? GPUBufferHandle( instance->WeightsBuffer ) : nullptr;
        break;

    case MATERIAL_TYPE_PBR:
    case MATERIAL_TYPE_BASELIGHT:

        if ( bSkinned ) {

            pPipeline = &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ColorPassSkinned;

            pSecondVertexBuffer = GPUBufferHandle( instance->WeightsBuffer );

        } else if ( bLightmap ) {

            pPipeline = &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ColorPassLightmap;

            pSecondVertexBuffer = GPUBufferHandle( instance->LightmapUVChannel );

            // lightmap is in last sample
            GFrameResources.TextureBindings[pMaterial->LightmapSlot].pTexture = GPUTextureHandle( instance->Lightmap );
            GFrameResources.SamplerBindings[pMaterial->LightmapSlot].pSampler = &LightmapSampler;

        } else if ( bVertexLight ) {

            pPipeline = &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ColorPassVertexLight;

            pSecondVertexBuffer = GPUBufferHandle( instance->VertexLightChannel );

        } else {

            pPipeline = &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ColorPassSimple;

            pSecondVertexBuffer = nullptr;
        }

        break;

    default:
        return false;
    }

    // Bind pipeline
    Cmd.BindPipeline( pPipeline );

    // Bind second vertex buffer
    Cmd.BindVertexBuffer( 1, pSecondVertexBuffer, 0 );

    // Set samplers
    if ( pMaterial->bColorPassTextureFetch ) {
        for ( int i = 0 ; i < pMaterial->NumSamplers ; i++ ) {
            GFrameResources.SamplerBindings[i].pSampler = pMaterial->pSampler[i];
        }
    }

    // Bind vertex and index buffers
    BindVertexAndIndexBuffers( instance );

    return true;
}

void AColorPassRenderer::BindTexturesColorPass( SMaterialFrameData * _Instance ) {
    if ( !_Instance->Material->bColorPassTextureFetch ) {
        return;
    }

    BindTextures( _Instance );
}

void AColorPassRenderer::RenderInstances() {
    ClearColorValue clearValue = {};

    clearValue.Float32[0] = GRenderView->BackgroundColor.X;
    clearValue.Float32[1] = GRenderView->BackgroundColor.Y;
    clearValue.Float32[2] = GRenderView->BackgroundColor.Z;
    clearValue.Float32[3] = 1;

    ClearDepthStencilValue depthStencilValue = {};

    depthStencilValue.Depth = 0;
    depthStencilValue.Stencil = 0;

    RenderPassBegin renderPassBegin = {};

    renderPassBegin.pRenderPass = &ColorPass;
    renderPassBegin.pFramebuffer = &GRenderTarget.GetFramebuffer();
    renderPassBegin.RenderArea.X = 0;
    renderPassBegin.RenderArea.Y = 0;
    renderPassBegin.RenderArea.Width = GRenderView->Width;
    renderPassBegin.RenderArea.Height = GRenderView->Height;
    renderPassBegin.pColorClearValues = &clearValue;
    renderPassBegin.pDepthStencilClearValue = &depthStencilValue;

    Cmd.BeginRenderPass( renderPassBegin );

    Viewport vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = GRenderView->Width;
    vp.Height = GRenderView->Height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    Cmd.SetViewport( vp );

//    bool bMakeSnapshot = RVRenderSnapshot;
//    if ( bMakeSnapshot ) {
//        RVRenderSnapshot = false;
//    }

    if ( GRenderView->bClearBackground || RVRenderSnapshot ) {
        unsigned int attachment = 0;
        Cmd.ClearFramebufferAttachments( GRenderTarget.GetFramebuffer(), &attachment, 1, &clearValue, nullptr, nullptr );

        if ( RVRenderSnapshot ) {
            SaveSnapshot(GRenderTarget.GetFramebufferTexture());
        }
    }

    DrawIndexedCmd drawCmd;
    drawCmd.InstanceCount = 1;
    drawCmd.StartInstanceLocation = 0;

    for ( int i = 0 ; i < GRenderView->InstanceCount ; i++ ) {
        SRenderInstance const * instance = GFrameData->Instances[ GRenderView->FirstInstance + i ];

        // Choose pipeline and second vertex buffer
        if ( !BindMaterial( instance ) ) {
            continue;
        }

        // Set material data (textures, uniforms)
        BindTexturesColorPass( instance->MaterialInstance );

        // Bind skeleton
        BindSkeleton( instance->SkeletonOffset, instance->SkeletonSize );

        // Bind shadow map
        GFrameResources.TextureBindings[15].pTexture = &GShadowMapRT.GetTexture();
        GFrameResources.SamplerBindings[15].pSampler = GShadowMapPassRenderer.GetShadowDepthSampler();

        // Set instance uniforms
        SetInstanceUniforms( i );

        Cmd.BindShaderResources( &GFrameResources.Resources );

        drawCmd.IndexCountPerInstance = instance->IndexCount;
        drawCmd.StartIndexLocation = instance->StartIndexLocation;
        drawCmd.BaseVertexLocation = instance->BaseVertexLocation;

        Cmd.Draw( &drawCmd );

        if ( RVRenderSnapshot ) {
            SaveSnapshot(GRenderTarget.GetFramebufferTexture());
        }
    }

    Cmd.EndRenderPass();
}

}
