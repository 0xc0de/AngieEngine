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

#include "SSAORenderer.h"

#include <Core/Public/Random.h>

ARuntimeVariable RVAODeinterleaved( _CTS( "AODeinterleaved" ), _CTS( "1" ) );
ARuntimeVariable RVAOBlur( _CTS( "AOBlur" ), _CTS( "1" ) );
ARuntimeVariable RVAORadius( _CTS( "AORadius" ), _CTS( "2" ) );
ARuntimeVariable RVAOBias( _CTS( "AOBias" ), _CTS( "0.1" ) );
ARuntimeVariable RVAOPowExponent( _CTS( "AOPowExponent" ), _CTS( "1.5" ) );

ARuntimeVariable RVCheckNearest( _CTS( "CheckNearest" ), _CTS( "1" ) );

using namespace RenderCore;

ASSAORenderer::ASSAORenderer()
{
    CreateFullscreenQuadPipeline( &Pipe, "postprocess/ssao/ssao.vert", "postprocess/ssao/simple.frag" );
    CreateFullscreenQuadPipeline( &Pipe_ORTHO, "postprocess/ssao/ssao.vert", "postprocess/ssao/simple_ortho.frag" );
    CreateFullscreenQuadPipelineGS( &CacheAwarePipe, "postprocess/ssao/ssao.vert", "postprocess/ssao/deinterleaved.frag", "postprocess/ssao/deinterleaved.geom" );
    CreateFullscreenQuadPipelineGS( &CacheAwarePipe_ORTHO, "postprocess/ssao/ssao.vert", "postprocess/ssao/deinterleaved_ortho.frag", "postprocess/ssao/deinterleaved.geom" );
    CreateFullscreenQuadPipeline( &BlurPipe, "postprocess/ssao/blur.vert", "postprocess/ssao/blur.frag" );
    CreateFullscreenQuadPipeline( &DeinterleavePipe, "postprocess/ssao/deinterleave.vert", "postprocess/ssao/deinterleave.frag" );
    CreateFullscreenQuadPipeline( &ReinterleavePipe, "postprocess/ssao/reinterleave.vert", "postprocess/ssao/reinterleave.frag" );
   
    CreateSamplers();

    AMersenneTwisterRand rng( 0u );

    const float NUM_DIRECTIONS = 8;

    Float3 hbaoRandom[HBAO_RANDOM_ELEMENTS];

    for ( int i = 0 ; i < HBAO_RANDOM_ELEMENTS ; i++ ) {
        const float r1 = rng.GetFloat();
        const float r2 = rng.GetFloat();

        // Random rotation angles in [0,2PI/NUM_DIRECTIONS)
        const float angle = Math::_2PI * r1 / NUM_DIRECTIONS;

        float s, c;

        Math::SinCos( angle, s, c );

        hbaoRandom[i].X = c;
        hbaoRandom[i].Y = s;
        hbaoRandom[i].Z = r2;

        StdSwap( hbaoRandom[i].X, hbaoRandom[i].Z ); // Swap to BGR
    }

    //for ( int i = 0; i < HBAO_RANDOM_ELEMENTS; i++ ) {
    //    GLogger.Printf( "vec2( %f, %f ),\n", float( i % 4 ) + 0.5f, float( i / 4 ) + 0.5f );
    //}

    //for ( int i = 0; i < HBAO_RANDOM_ELEMENTS; i++ ) {
    //    GLogger.Printf( "vec4( %f, %f, %f, 0.0 ),\n", hbaoRandom[i].X, hbaoRandom[i].Y, hbaoRandom[i].Z );
    //}

    GDevice->CreateTexture( RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_RGB16F, STextureResolution2D( HBAO_RANDOM_SIZE, HBAO_RANDOM_SIZE ) ), &RandomMap );
    RandomMap->Write( 0, RenderCore::FORMAT_FLOAT3, sizeof( hbaoRandom ), 1, hbaoRandom );
}

void ASSAORenderer::CreateSamplers()
{
    SSamplerCreateInfo samplerCI;

    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    GDevice->GetOrCreateSampler( samplerCI, &DepthSampler );

    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    GDevice->GetOrCreateSampler( samplerCI, &LinearDepthSampler );

    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    GDevice->GetOrCreateSampler( samplerCI, &NormalSampler );

    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    GDevice->GetOrCreateSampler( samplerCI, &BlurSampler );

    samplerCI.Filter = FILTER_NEAREST;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    GDevice->GetOrCreateSampler( samplerCI, &NearestSampler );

    samplerCI.Filter = FILTER_NEAREST;
    samplerCI.AddressU = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressV = SAMPLER_ADDRESS_WRAP;
    samplerCI.AddressW = SAMPLER_ADDRESS_WRAP;
    GDevice->GetOrCreateSampler( samplerCI, &RandomMapSampler );
}

void ASSAORenderer::ResizeAO( int Width, int Height ) {
    if ( AOWidth != Width
         || AOHeight != Height ) {

        AOWidth = Width;
        AOHeight = Height;

        AOQuarterWidth = ((AOWidth+3)/4);
        AOQuarterHeight = ((AOHeight+3)/4);

        GDevice->CreateTexture(
            RenderCore::MakeTexture( TEXTURE_FORMAT_R32F,
                                     RenderCore::STextureResolution2DArray( AOQuarterWidth, AOQuarterHeight, HBAO_RANDOM_ELEMENTS )
            ), &SSAODeinterleaveDepthArray
        );

        for ( int i = 0; i < HBAO_RANDOM_ELEMENTS; i++ ) {
            RenderCore::STextureViewCreateInfo textureCI = {};
            textureCI.Type = RenderCore::TEXTURE_2D;
            textureCI.Format = RenderCore::TEXTURE_FORMAT_R32F;
            textureCI.pOriginalTexture = SSAODeinterleaveDepthArray;
            textureCI.MinLod = 0;
            textureCI.NumLods = 1;
            textureCI.MinLayer = i;
            textureCI.NumLayers = 1;
            GDevice->CreateTextureView( textureCI, &SSAODeinterleaveDepthView[i] );
        }
    }
}

void ASSAORenderer::AddDeinterleaveDepthPass( AFrameGraph & FrameGraph, AFrameGraphTexture * LinearDepth, AFrameGraphTexture ** ppDeinterleaveDepthArray )
{
    AFrameGraphTexture * SSAODeinterleaveDepthView_R[16];
    for ( int i = 0 ; i < 16 ; i++ ) {
        SSAODeinterleaveDepthView_R[i] = FrameGraph.AddExternalResource< RenderCore::STextureCreateInfo, RenderCore::ITexture >(
            Core::Fmt( "Deinterleave Depth View %d", i ),
            RenderCore::STextureCreateInfo(),
            SSAODeinterleaveDepthView[i] );
    }

    ARenderPass & deinterleavePass = FrameGraph.AddTask< ARenderPass >( "Deinterleave Depth Pass" );
    deinterleavePass.SetRenderArea( AOQuarterWidth, AOQuarterHeight );
    deinterleavePass.AddResource( LinearDepth, RESOURCE_ACCESS_READ );
    deinterleavePass.SetColorAttachments(
    {
        { SSAODeinterleaveDepthView_R[0], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[1], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[2], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[3], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[4], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[5], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[6], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[7], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
    } );
    deinterleavePass.AddSubpass( { 0,1,2,3,4,5,6,7 }, // color attachment refs
                                 [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall {
            Float2 UVOffset;
            Float2 InvFullResolution;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();
        drawCall->UVOffset.X = 0.5f;
        drawCall->UVOffset.Y = 0.5f;
        drawCall->InvFullResolution.X = 1.0f / AOWidth;
        drawCall->InvFullResolution.Y = 1.0f / AOHeight;

        GFrameResources.TextureBindings[0].pTexture = LinearDepth->Actual();
        GFrameResources.SamplerBindings[0].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( DeinterleavePipe );
    } );

    ARenderPass & deinterleavePass2 = FrameGraph.AddTask< ARenderPass >( "Deinterleave Depth Pass 2" );
    deinterleavePass2.SetRenderArea( AOQuarterWidth, AOQuarterHeight );
    deinterleavePass2.AddResource( LinearDepth, RESOURCE_ACCESS_READ );
    deinterleavePass2.SetColorAttachments(
    {
        { SSAODeinterleaveDepthView_R[8], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[9], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[10], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[11], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[12], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[13], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[14], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
        { SSAODeinterleaveDepthView_R[15], RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE ) },
    } );
    deinterleavePass2.AddSubpass( { 0,1,2,3,4,5,6,7 }, // color attachment refs
                                  [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall {
            Float2 UVOffset;
            Float2 InvFullResolution;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();
        drawCall->UVOffset.X = float( 8 % 4 ) + 0.5f;
        drawCall->UVOffset.Y = float( 8 / 4 ) + 0.5f;
        drawCall->InvFullResolution.X = 1.0f / AOWidth;
        drawCall->InvFullResolution.Y = 1.0f / AOHeight;

        GFrameResources.TextureBindings[0].pTexture = LinearDepth->Actual();
        GFrameResources.SamplerBindings[0].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( DeinterleavePipe );
    } );

    AFrameGraphTexture * DeinterleaveDepthArray_R = FrameGraph.AddExternalResource< RenderCore::STextureCreateInfo, RenderCore::ITexture >(
        "Deinterleave Depth Array",
        RenderCore::STextureCreateInfo(),
        SSAODeinterleaveDepthArray );

    *ppDeinterleaveDepthArray = DeinterleaveDepthArray_R;
}

void ASSAORenderer::AddCacheAwareAOPass( AFrameGraph & FrameGraph, AFrameGraphTexture * DeinterleaveDepthArray, AFrameGraphTexture * NormalTexture, AFrameGraphTexture ** ppSSAOTextureArray )
{
    ARenderPass & cacheAwareAO = FrameGraph.AddTask< ARenderPass >( "Cache Aware AO Pass" );
    cacheAwareAO.SetRenderArea( AOQuarterWidth, AOQuarterHeight );
    cacheAwareAO.AddResource( DeinterleaveDepthArray, RESOURCE_ACCESS_READ );
    cacheAwareAO.AddResource( NormalTexture, RESOURCE_ACCESS_READ );
    cacheAwareAO.SetColorAttachments(
    {
        {
            "SSAO Texture Array",
            RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_R8, RenderCore::STextureResolution2DArray( AOQuarterWidth, AOQuarterHeight, HBAO_RANDOM_ELEMENTS ) ),
            RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE )
        }
    } );
    cacheAwareAO.AddSubpass( { 0 }, // color attachment refs
                             [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall
        {
            float Bias;
            float FallofFactor;
            float RadiusToScreen;
            float PowExponent;
            float Multiplier;
            float Pad;
            Float2 InvFullResolution;
            Float2 InvQuarterResolution;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();

        float projScale;

        if ( GRenderView->bPerspective ) {
            projScale = (float)AOHeight / std::tan( GRenderView->ViewFovY * 0.5f ) * 0.5f;
        } else {
            projScale = (float)AOHeight * GRenderView->ProjectionMatrix[1][1] * 0.5f;
        }

        drawCall->Bias = RVAOBias.GetFloat();
        drawCall->FallofFactor = -1.0f / (RVAORadius.GetFloat() * RVAORadius.GetFloat());
        drawCall->RadiusToScreen = RVAORadius.GetFloat() * 0.5f * projScale;
        drawCall->PowExponent = RVAOPowExponent.GetFloat();
        drawCall->Multiplier = 1.0f / (1.0f - RVAOBias.GetFloat());
        drawCall->InvFullResolution.X = 1.0f / AOWidth;
        drawCall->InvFullResolution.Y = 1.0f / AOHeight;
        drawCall->InvQuarterResolution.X = 1.0f / AOQuarterWidth;
        drawCall->InvQuarterResolution.Y = 1.0f / AOQuarterHeight;

        GFrameResources.TextureBindings[0].pTexture = DeinterleaveDepthArray->Actual();
        GFrameResources.SamplerBindings[0].pSampler = RVCheckNearest ? NearestSampler : LinearDepthSampler;

        GFrameResources.TextureBindings[1].pTexture = NormalTexture->Actual();
        GFrameResources.SamplerBindings[1].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        if ( GRenderView->bPerspective ) {
            DrawSAQ( CacheAwarePipe );
        } else {
            DrawSAQ( CacheAwarePipe_ORTHO );
        }
    } );

    *ppSSAOTextureArray = cacheAwareAO.GetColorAttachments()[0].Resource;
}

void ASSAORenderer::AddReinterleavePass( AFrameGraph & FrameGraph, AFrameGraphTexture * SSAOTextureArray, AFrameGraphTexture ** ppSSAOTexture )
{
    ARenderPass & reinterleavePass = FrameGraph.AddTask< ARenderPass >( "Reinterleave Pass" );
    reinterleavePass.SetRenderArea( AOWidth, AOHeight );
    reinterleavePass.AddResource( SSAOTextureArray, RESOURCE_ACCESS_READ );
    reinterleavePass.SetColorAttachments(
    {
        {
            "SSAO Texture",
            RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_R8, RenderCore::STextureResolution2D( AOWidth, AOHeight ) ),
            RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE )
        }
    } );
    reinterleavePass.AddSubpass( { 0 }, // color attachment refs
                                 [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        GFrameResources.TextureBindings[0].pTexture = SSAOTextureArray->Actual();
        GFrameResources.SamplerBindings[0].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( ReinterleavePipe );
    } );

    *ppSSAOTexture = reinterleavePass.GetColorAttachments()[0].Resource;
}

void ASSAORenderer::AddSimpleAOPass( AFrameGraph & FrameGraph, AFrameGraphTexture * LinearDepth, AFrameGraphTexture * NormalTexture, AFrameGraphTexture ** ppSSAOTexture )
{
    AFrameGraphTexture * RandomMapTexture_R = FrameGraph.AddExternalResource< RenderCore::STextureCreateInfo, RenderCore::ITexture >(
        "SSAO Random Map", RenderCore::STextureCreateInfo(), RandomMap );

    ARenderPass & pass = FrameGraph.AddTask< ARenderPass >( "Simple AO Pass" );
    pass.SetRenderArea( AOWidth, AOHeight );
    pass.AddResource( LinearDepth, RESOURCE_ACCESS_READ );
    pass.AddResource( NormalTexture, RESOURCE_ACCESS_READ );
    pass.AddResource( RandomMapTexture_R, RESOURCE_ACCESS_READ );
    pass.SetColorAttachments(
    {
        {
            "SSAO Texture (Interleaved)",
            RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_R8, RenderCore::STextureResolution2D( AOWidth, AOHeight ) ),
            RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE )
        }
    } );
    pass.AddSubpass( { 0 }, // color attachment refs
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall
        {
            float Bias;
            float FallofFactor;
            float RadiusToScreen;
            float PowExponent;
            float Multiplier;
            float Pad;
            Float2 InvFullResolution;
            Float2 InvQuarterResolution;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();

        float projScale;

        if ( GRenderView->bPerspective ) {
            projScale = (float)AOHeight / std::tan( GRenderView->ViewFovY * 0.5f ) * 0.5f;
        } else {
            projScale = (float)AOHeight * GRenderView->ProjectionMatrix[1][1] * 0.5f;
        }

        drawCall->Bias = RVAOBias.GetFloat();
        drawCall->FallofFactor = -1.0f / (RVAORadius.GetFloat() * RVAORadius.GetFloat());
        drawCall->RadiusToScreen = RVAORadius.GetFloat() * 0.5f * projScale;
        drawCall->PowExponent = RVAOPowExponent.GetFloat();
        drawCall->Multiplier = 1.0f / (1.0f - RVAOBias.GetFloat());
        drawCall->InvFullResolution.X = 1.0f / AOWidth;
        drawCall->InvFullResolution.Y = 1.0f / AOHeight;
        drawCall->InvQuarterResolution.X = 0; // don't care
        drawCall->InvQuarterResolution.Y = 0; // don't care

        GFrameResources.TextureBindings[0].pTexture = LinearDepth->Actual();
        GFrameResources.SamplerBindings[0].pSampler = RVCheckNearest ? NearestSampler : LinearDepthSampler;

        GFrameResources.TextureBindings[1].pTexture = NormalTexture->Actual();
        GFrameResources.SamplerBindings[1].pSampler = NearestSampler;

        GFrameResources.TextureBindings[2].pTexture = RandomMapTexture_R->Actual();
        GFrameResources.SamplerBindings[2].pSampler = RandomMapSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        if ( GRenderView->bPerspective ) {
            DrawSAQ( Pipe );
        } else {
            DrawSAQ( Pipe_ORTHO );
        }
    } );

    *ppSSAOTexture = pass.GetColorAttachments()[0].Resource;
}

void ASSAORenderer::AddAOBlurPass( AFrameGraph & FrameGraph, AFrameGraphTexture * SSAOTexture, AFrameGraphTexture * LinearDepth, AFrameGraphTexture ** ppBluredSSAO )
{
    ARenderPass & aoBlurXPass = FrameGraph.AddTask< ARenderPass >( "AO Blur X Pass" );
    aoBlurXPass.SetRenderArea( AOWidth, AOHeight );
    aoBlurXPass.SetColorAttachments(
    {
        {
            "Temp SSAO Texture (Blur X)",
            RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_R8, RenderCore::STextureResolution2D( AOWidth, AOHeight ) ),
            RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE )
        }
    } );
    aoBlurXPass.AddResource( SSAOTexture, RESOURCE_ACCESS_READ );
    aoBlurXPass.AddResource( LinearDepth, RESOURCE_ACCESS_READ );
    aoBlurXPass.AddSubpass( { 0 }, // color attachment refs
                            [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall {
            Float2 InvSize;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();
        drawCall->InvSize.X = 1.0f / RenderPass.GetRenderArea().Width;
        drawCall->InvSize.Y = 0;

        // SSAO blur X
        GFrameResources.TextureBindings[0].pTexture = SSAOTexture->Actual();
        GFrameResources.SamplerBindings[0].pSampler = BlurSampler;

        GFrameResources.TextureBindings[1].pTexture = LinearDepth->Actual();
        GFrameResources.SamplerBindings[1].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( BlurPipe );
    } );

    AFrameGraphTexture * TempSSAOTextureBlurX = aoBlurXPass.GetColorAttachments()[0].Resource;

    ARenderPass & aoBlurYPass = FrameGraph.AddTask< ARenderPass >( "AO Blur Y Pass" );
    aoBlurYPass.SetRenderArea( AOWidth, AOHeight );
    aoBlurYPass.SetColorAttachments(
    {
        {
            "Blured SSAO Texture",
            RenderCore::MakeTexture( RenderCore::TEXTURE_FORMAT_R8, RenderCore::STextureResolution2D( AOWidth, AOHeight ) ),
            RenderCore::SAttachmentInfo().SetLoadOp( RenderCore::ATTACHMENT_LOAD_OP_DONT_CARE )
        }
    } );
    aoBlurYPass.AddResource( TempSSAOTextureBlurX, RESOURCE_ACCESS_READ );
    aoBlurYPass.AddResource( LinearDepth, RESOURCE_ACCESS_READ );
    aoBlurYPass.AddSubpass( { 0 }, // color attachment refs
                            [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        using namespace RenderCore;

        struct SDrawCall {
            Float2 InvSize;
        };

        SDrawCall * drawCall = SetDrawCallUniforms< SDrawCall >();
        drawCall->InvSize.X = 0;
        drawCall->InvSize.Y = 1.0f / RenderPass.GetRenderArea().Height;

        // SSAO blur Y
        GFrameResources.TextureBindings[0].pTexture = TempSSAOTextureBlurX->Actual();
        GFrameResources.SamplerBindings[0].pSampler = BlurSampler;

        GFrameResources.TextureBindings[1].pTexture = LinearDepth->Actual();
        GFrameResources.SamplerBindings[1].pSampler = NearestSampler;

        rcmd->BindShaderResources( &GFrameResources.Resources );

        DrawSAQ( BlurPipe );
    } );

    *ppBluredSSAO = aoBlurYPass.GetColorAttachments()[0].Resource;
}

void ASSAORenderer::AddPasses( AFrameGraph & FrameGraph, AFrameGraphTexture * LinearDepth, AFrameGraphTexture * NormalTexture, AFrameGraphTexture ** ppSSAOTexture )
{
    ResizeAO( GFrameData->AllocSurfaceWidth, GFrameData->AllocSurfaceHeight );

    if ( RVAODeinterleaved ) {
        AFrameGraphTexture * DeinterleaveDepthArray, * SSAOTextureArray;
        
        AddDeinterleaveDepthPass( FrameGraph, LinearDepth, &DeinterleaveDepthArray );
        AddCacheAwareAOPass( FrameGraph, DeinterleaveDepthArray, NormalTexture, &SSAOTextureArray );
        AddReinterleavePass( FrameGraph, SSAOTextureArray, ppSSAOTexture );
    }
    else {
        AddSimpleAOPass( FrameGraph, LinearDepth, NormalTexture, ppSSAOTexture );
    }

    if ( RVAOBlur ) {
        AddAOBlurPass( FrameGraph, *ppSSAOTexture, LinearDepth, ppSSAOTexture );
    }
}
