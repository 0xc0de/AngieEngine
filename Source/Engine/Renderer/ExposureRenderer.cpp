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

#include "ExposureRenderer.h"
#include "RenderCommon.h"

#include <Runtime/Public/RuntimeVariable.h>

ARuntimeVariable RVShowDefaultExposure( _CTS( "ShowDefaultExposure" ), _CTS( "0" ) );

using namespace RenderCore;

AExposureRenderer::AExposureRenderer()
{
    // Create textures
    STextureCreateInfo texCI = {};
    texCI.Type = RenderCore::TEXTURE_2D;
    texCI.NumLods = 1;
    texCI.Format = RenderCore::TEXTURE_FORMAT_RG16F;

    texCI.Resolution.Tex2D.Width = 64;
    texCI.Resolution.Tex2D.Height = 64;
    GDevice->CreateTexture( texCI, &Luminance64 );

    texCI.Resolution.Tex2D.Width = 32;
    texCI.Resolution.Tex2D.Height = 32;
    GDevice->CreateTexture( texCI, &Luminance32 );

    texCI.Resolution.Tex2D.Width = 16;
    texCI.Resolution.Tex2D.Height = 16;
    GDevice->CreateTexture( texCI, &Luminance16 );

    texCI.Resolution.Tex2D.Width = 8;
    texCI.Resolution.Tex2D.Height = 8;
    GDevice->CreateTexture( texCI, &Luminance8 );

    texCI.Resolution.Tex2D.Width = 4;
    texCI.Resolution.Tex2D.Height = 4;
    GDevice->CreateTexture( texCI, &Luminance4 );

    texCI.Resolution.Tex2D.Width = 2;
    texCI.Resolution.Tex2D.Height = 2;
    GDevice->CreateTexture( texCI, &Luminance2 );

    byte defaultLum[2] = { 30, 30 }; // TODO: choose appropriate value
    texCI.Resolution.Tex2D.Width = 1;
    texCI.Resolution.Tex2D.Height = 1;
    texCI.Format = RenderCore::TEXTURE_FORMAT_RG8;
    GDevice->CreateTexture( texCI, &DefaultLuminance );
    DefaultLuminance->Write( 0, RenderCore::FORMAT_UBYTE2, sizeof( defaultLum ), 1, defaultLum );

    CreateFullscreenQuadPipeline( &MakeLuminanceMapPipe, "postprocess/exposure/make_luminance.vert", "postprocess/exposure/make_luminance.frag" );
    CreateFullscreenQuadPipeline( &SumLuminanceMapPipe, "postprocess/exposure/sum_luminance.vert", "postprocess/exposure/sum_luminance.frag" );
    CreateFullscreenQuadPipeline( &DynamicExposurePipe, "postprocess/exposure/dynamic_exposure.vert", "postprocess/exposure/dynamic_exposure.frag", RenderCore::BLENDING_ALPHA );

    CreateSampler();
}

void AExposureRenderer::CreateSampler() {
    SSamplerCreateInfo samplerCI;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    samplerCI.Filter = FILTER_LINEAR;
    GDevice->GetOrCreateSampler( samplerCI, &LuminanceSampler );
}

AFrameGraphTexture * AExposureRenderer::AddPass( AFrameGraph & FrameGraph, AFrameGraphTexture * SourceTexture ) {
    ATextureGPU * exposureTexture = GRenderView->CurrentExposure;

    if ( !exposureTexture || RVShowDefaultExposure ) {
        return FrameGraph.AddExternalResource< RenderCore::STextureCreateInfo, RenderCore::ITexture >(
            "Fallback exposure texture",
            MakeTexture( RenderCore::TEXTURE_FORMAT_RG16F, RenderCore::STextureResolution2D( 1, 1 ) ),
            DefaultLuminance );
    }

    auto Exposure_R = FrameGraph.AddExternalResource(
        "Exposure texture",
        RenderCore::STextureCreateInfo(),
        GPUTextureHandle( exposureTexture )
    );
    auto Luminance64_R = FrameGraph.AddExternalResource(
        "Luminance64",
        RenderCore::STextureCreateInfo(),
        Luminance64
    );
    auto Luminance32_R = FrameGraph.AddExternalResource(
        "Luminance32",
        RenderCore::STextureCreateInfo(),
        Luminance32
    );
    auto Luminance16_R = FrameGraph.AddExternalResource(
        "Luminance16",
        RenderCore::STextureCreateInfo(),
        Luminance16
    );
    auto Luminance8_R = FrameGraph.AddExternalResource(
        "Luminance8",
        RenderCore::STextureCreateInfo(),
        Luminance8
    );
    auto Luminance4_R = FrameGraph.AddExternalResource(
        "Luminance4",
        RenderCore::STextureCreateInfo(),
        Luminance4
    );
    auto Luminance2_R = FrameGraph.AddExternalResource(
        "Luminance2",
        RenderCore::STextureCreateInfo(),
        Luminance2
    );

    // Make luminance map 64x64
    FrameGraph.AddTask< ARenderPass >( "Make luminance map 64x64" )
        .SetRenderArea( 64, 64 )
        .SetColorAttachments( { { Luminance64_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( SourceTexture, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = SourceTexture->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( MakeLuminanceMapPipe );
    });

    // Downscale luminance to 32x32
    FrameGraph.AddTask< ARenderPass >( "Downscale luminance to 32x32" )
        .SetRenderArea( 32, 32 )
        .SetColorAttachments( { { Luminance32_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance64_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance64_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( SumLuminanceMapPipe );
    } );

    // Downscale luminance to 16x16
    FrameGraph.AddTask< ARenderPass >( "Downscale luminance to 16x16" )
        .SetRenderArea( 16, 16 )
        .SetColorAttachments( { { Luminance16_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance32_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance32_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( SumLuminanceMapPipe );
    } );

    // Downscale luminance to 8x8
    FrameGraph.AddTask< ARenderPass >( "Downscale luminance to 8x8" )
        .SetRenderArea( 8, 8 )
        .SetColorAttachments( { { Luminance8_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance16_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance16_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( SumLuminanceMapPipe );
    } );

    // Downscale luminance to 4x4
    FrameGraph.AddTask< ARenderPass >( "Downscale luminance to 4x4" )
        .SetRenderArea( 4, 4 )
        .SetColorAttachments( { { Luminance4_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance8_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance8_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( SumLuminanceMapPipe );
    } );

    // Downscale luminance to 2x2
    FrameGraph.AddTask< ARenderPass >( "Downscale luminance to 2x2" )
        .SetRenderArea( 2, 2 )
        .SetColorAttachments( { { Luminance2_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance4_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance4_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( SumLuminanceMapPipe );
    } );

    // Render final exposure
    FrameGraph.AddTask< ARenderPass >( "Render final exposure" )
        .SetRenderArea( 1, 1 )
        .SetColorAttachments( { { Exposure_R, SAttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE ) } } )
        .AddResource( Luminance2_R, RESOURCE_ACCESS_READ )
        .AddSubpass( { 0 },
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        GFrameResources.TextureBindings[0].pTexture = Luminance2_R->Actual();
        GFrameResources.SamplerBindings[0].pSampler = LuminanceSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( DynamicExposurePipe );
    } );

    return Exposure_R;
}
