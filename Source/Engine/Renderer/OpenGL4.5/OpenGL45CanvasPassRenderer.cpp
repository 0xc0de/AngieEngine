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

#include "OpenGL45CanvasPassRenderer.h"
#include "OpenGL45ShaderSource.h"
#include "OpenGL45FrameResources.h"
#include "OpenGL45RenderTarget.h"
#include "OpenGL45Material.h"

using namespace GHI;

namespace OpenGL45 {

ACanvasPassRenderer GCanvasPassRenderer;

void OpenGL45RenderView( SRenderView * _RenderView );

void ACanvasPassRenderer::Initialize() {
    RenderPassCreateInfo renderPassCI = {};

    renderPassCI.NumColorAttachments = 1;

    AttachmentInfo colorAttachment = {};
    colorAttachment.LoadOp = ATTACHMENT_LOAD_OP_LOAD;
    renderPassCI.pColorAttachments = &colorAttachment;
    renderPassCI.pDepthStencilAttachment = NULL;

    AttachmentRef colorAttachmentRef = {};
    colorAttachmentRef.Attachment = 0;

    SubpassInfo subpass = {};
    subpass.NumColorAttachments = 1;
    subpass.pColorAttachmentRefs = &colorAttachmentRef;

    renderPassCI.NumSubpasses = 1;
    renderPassCI.pSubpasses = &subpass;

    CanvasPass.Initialize( renderPassCI );

    //
    // Create pipelines
    //

    CreatePresentViewPipeline();
    CreatePipelines();
    CreateAlphaPipelines();

    //
    // Create samplers
    //

    CreateSamplers();
}

void ACanvasPassRenderer::Deinitialize() {
    CanvasPass.Deinitialize();

    for ( int i = 0 ; i < COLOR_BLENDING_MAX ; i++ ) {
        PresentViewPipeline[i].Deinitialize();
        Pipelines[i].Deinitialize();
        AlphaPipeline[i].Deinitialize();
    }
}

void ACanvasPassRenderer::CreatePresentViewPipeline() {
    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_DISABLED;
    rsd.bScissorEnable = true;

    BlendingStateInfo bsd;
    bsd.SetDefaults();

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, TexCoord )
        },
        {
            "InColor",
            2,              // location
            0,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Color )
        }
    };


    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ) );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    const char * vertexSourceCode =
            "out gl_PerVertex\n"
            "{\n"
            "    vec4 gl_Position;\n"
            "};\n"
            "layout( location = 0 ) flat out vec2 VS_TexCoord;\n"
            "layout( location = 1 ) out vec4 VS_Color;\n"
            "void main() {\n"
            "  gl_Position = OrthoProjection * vec4( InPosition, 0.0, 1.0 );\n"
            "  VS_TexCoord = InTexCoord;\n"
            "  VS_Color = InColor;\n"
            "}\n";
    const char * fragmentSourceCode =
            "layout( origin_upper_left ) in vec4 gl_FragCoord;\n"
            "layout( binding = 0 ) uniform sampler2D tslot0;\n"
            "layout( location = 0 ) flat in vec2 VS_TexCoord;\n"
            "layout( location = 1 ) in vec4 VS_Color;\n"
            "layout( location = 0 ) out vec4 FS_FragColor;\n"
            "void main() {\n"
            "  ivec2 fragCoord = ivec2( gl_FragCoord.xy - VS_TexCoord );\n"
            "  fragCoord.y = textureSize( tslot0, 0 ).y - fragCoord.y - 1;\n"
            "  FS_FragColor = VS_Color * texelFetch( tslot0, fragCoord, 0 );\n"
            //"  FS_FragColor = VS_Color * texture( tslot0, VS_TexCoord );\n"
            //"  FS_FragColor = pow( FS_FragColor, vec4( vec3( 1.0/2.2 ), 1 ) );\n"
            "}\n";

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( vertexSourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( fragmentSourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[2] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SHUDDrawVert );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    pipelineCI.pRenderPass = &CanvasPass;
    pipelineCI.Subpass = 0;

    for ( int i = 0 ; i < COLOR_BLENDING_MAX ; i++ ) {
        if ( i == COLOR_BLENDING_DISABLED ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_NO_BLEND );
        } else if ( i == COLOR_BLENDING_ALPHA ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );
        } else {
            bsd.RenderTargetSlots[0].SetBlendingPreset( (BLENDING_PRESET)(BLENDING_NO_BLEND + i) );
        }

        pipelineCI.pBlending = &bsd;
        PresentViewPipeline[i].Initialize( pipelineCI );
    }
}

void ACanvasPassRenderer::CreatePipelines() {
    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_DISABLED;
    rsd.bScissorEnable = true;

    BlendingStateInfo bsd;
    bsd.SetDefaults();

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, TexCoord )
        },
        {
            "InColor",
            2,              // location
            0,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Color )
        }
    };


    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ) );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    const char * vertexSourceCode =
            "out gl_PerVertex\n"
            "{\n"
            "    vec4 gl_Position;\n"
            "};\n"
            "layout( location = 0 ) out vec2 VS_TexCoord;\n"
            "layout( location = 1 ) out vec4 VS_Color;\n"
            "void main() {\n"
            "  gl_Position = OrthoProjection * vec4( InPosition, 0.0, 1.0 );\n"
            "  VS_TexCoord = InTexCoord;\n"
            "  VS_Color = InColor;\n"
            "}\n";
    const char * fragmentSourceCode =
            "layout( binding = 0 ) uniform sampler2D tslot0;\n"
            "layout( location = 0 ) in vec2 VS_TexCoord;\n"
            "layout( location = 1 ) in vec4 VS_Color;\n"
            "layout( location = 0 ) out vec4 FS_FragColor;\n"
            "void main() {\n"
            "  FS_FragColor = VS_Color * texture( tslot0, VS_TexCoord );\n"
            //"  FS_FragColor = pow( FS_FragColor, vec4( vec3( 1.0/2.2 ), 1 ) );\n"
            "}\n";

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( vertexSourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( fragmentSourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[2] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SHUDDrawVert );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    pipelineCI.pRenderPass = &CanvasPass;
    pipelineCI.Subpass = 0;

    for ( int i = 0 ; i < COLOR_BLENDING_MAX ; i++ ) {
        if ( i == COLOR_BLENDING_DISABLED ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_NO_BLEND );
        } else if ( i == COLOR_BLENDING_ALPHA ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );
        } else {
            bsd.RenderTargetSlots[0].SetBlendingPreset( (BLENDING_PRESET)(BLENDING_NO_BLEND + i) );
        }

        pipelineCI.pBlending = &bsd;
        Pipelines[i].Initialize( pipelineCI );
    }
}

void ACanvasPassRenderer::CreateAlphaPipelines() {
    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_DISABLED;
    rsd.bScissorEnable = true;

    BlendingStateInfo bsd;
    bsd.SetDefaults();

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, TexCoord )
        },
        {
            "InColor",
            2,              // location
            0,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SHUDDrawVert, Color )
        }
    };


    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ) );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    const char * vertexSourceCode =
            "out gl_PerVertex\n"
            "{\n"
            "    vec4 gl_Position;\n"
            "};\n"
            "layout( location = 0 ) out vec2 VS_TexCoord;\n"
            "layout( location = 1 ) out vec4 VS_Color;\n"
            "void main() {\n"
            "  gl_Position = OrthoProjection * vec4( InPosition, 0.0, 1.0 );\n"
            "  VS_TexCoord = InTexCoord;\n"
            "  VS_Color = InColor;\n"
            "}\n";
    const char * fragmentSourceCode =
            "layout( binding = 0 ) uniform sampler2D tslot0;\n"
            "layout( location = 0 ) in vec2 VS_TexCoord;\n"
            "layout( location = 1 ) in vec4 VS_Color;\n"
            "layout( location = 0 ) out vec4 FS_FragColor;\n"
            "void main() {\n"
            "  FS_FragColor = VS_Color * vec4(texture( tslot0, VS_TexCoord ).r);\n"
            //"  FS_FragColor = pow( FS_FragColor, vec4( vec3( 1.0/2.2 ), 1 ) );\n"
            "}\n";

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( vertexSourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( UniformStr );
    GShaderSources.Add( fragmentSourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[2] = { vs, fs };

    pipelineCI.NumStages = 2;
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SHUDDrawVert );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    pipelineCI.pRenderPass = &CanvasPass;
    pipelineCI.Subpass = 0;

    for ( int i = 0 ; i < COLOR_BLENDING_MAX ; i++ ) {
        if ( i == COLOR_BLENDING_DISABLED ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_NO_BLEND );
        } else if ( i == COLOR_BLENDING_ALPHA ) {
            bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );
        } else {
            bsd.RenderTargetSlots[0].SetBlendingPreset( (BLENDING_PRESET)(BLENDING_NO_BLEND + i) );
        }

        pipelineCI.pBlending = &bsd;
        AlphaPipeline[i].Initialize( pipelineCI );
    }
}

void ACanvasPassRenderer::CreateSamplers() {
    SamplerCreateInfo samplerCI;
    samplerCI.SetDefaults();
    samplerCI.Filter = FILTER_NEAREST;
    samplerCI.AddressU = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressV = SAMPLER_ADDRESS_CLAMP;
    samplerCI.AddressW = SAMPLER_ADDRESS_CLAMP;
    PresentViewSampler = GDevice.GetOrCreateSampler( samplerCI );

    for ( int i = 0 ; i < HUD_SAMPLER_MAX ; i++ ) {
        samplerCI.Filter = ( i & 1 ) ? FILTER_NEAREST : FILTER_LINEAR;
        samplerCI.AddressU = samplerCI.AddressV = samplerCI.AddressW = (SAMPLER_ADDRESS_MODE)( i >> 1 );
        Samplers[i] = GDevice.GetOrCreateSampler( samplerCI );
    }
}

void ACanvasPassRenderer::BeginCanvasPass() {
    RenderPassBegin renderPassBegin = {};

    renderPassBegin.pRenderPass = &CanvasPass;
    renderPassBegin.pFramebuffer = GState.GetDefaultFramebuffer();
    renderPassBegin.RenderArea.X = 0;
    renderPassBegin.RenderArea.Y = 0;
    renderPassBegin.RenderArea.Width = GFrameData->CanvasWidth;
    renderPassBegin.RenderArea.Height = GFrameData->CanvasHeight;
    renderPassBegin.pColorClearValues = NULL;
    renderPassBegin.pDepthStencilClearValue = NULL;

    Cmd.BeginRenderPass( renderPassBegin );

    Viewport vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = GFrameData->CanvasWidth;
    vp.Height = GFrameData->CanvasHeight;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    Cmd.SetViewport( vp );
}

void ACanvasPassRenderer::RenderInstances() {
    if ( !GFrameData->DrawListHead ) {
        return;
    }

    Rect2D scissorRect;

    // Begin render pass
    BeginCanvasPass();

    DrawIndexedCmd drawCmd;
    drawCmd.InstanceCount = 1;
    drawCmd.StartInstanceLocation = 0;
    drawCmd.BaseVertexLocation = 0;

    Buffer * streamBuffer = GPUBufferHandle( GFrameData->StreamBuffer );

    for ( SHUDDrawList * drawList = GFrameData->DrawListHead ; drawList ; drawList = drawList->pNext ) {

        //Cmd.Barrier( VERTEX_ATTRIB_ARRAY_BARRIER_BIT | ELEMENT_ARRAY_BARRIER_BIT );

        // Process render commands
        for ( SHUDDrawCmd * cmd = drawList->Commands ; cmd < &drawList->Commands[drawList->CommandsCount] ; cmd++ ) {

            switch ( cmd->Type )
            {
                case HUD_DRAW_CMD_VIEWPORT:
                {
                    // End canvas pass
                    Cmd.EndRenderPass();

                    // Render scene to viewport
                    AN_ASSERT( cmd->ViewportIndex >= 0 && cmd->ViewportIndex < MAX_RENDER_VIEWS );

                    SRenderView * renderView = &GFrameData->RenderViews[cmd->ViewportIndex];

                    OpenGL45RenderView( renderView );

                    // Restore canvas pass
                    BeginCanvasPass();

                    // Draw just rendered scene on the canvas
                    Cmd.BindPipeline( &PresentViewPipeline[cmd->Blending] );
                    Cmd.BindVertexBuffer( 0, streamBuffer, drawList->VertexStreamOffset );
                    Cmd.BindIndexBuffer( streamBuffer, INDEX_TYPE_UINT16, drawList->IndexStreamOffset );

                    GFrameResources.TextureBindings[0].pTexture = &GRenderTarget.GetFramebufferTexture();
                    GFrameResources.SamplerBindings[0].pSampler = &PresentViewSampler;

                    scissorRect.X = cmd->ClipMins.X;
                    scissorRect.Y = cmd->ClipMins.Y;
                    scissorRect.Width = cmd->ClipMaxs.X - cmd->ClipMins.X;
                    scissorRect.Height = cmd->ClipMaxs.Y - cmd->ClipMins.Y;
                    Cmd.SetScissor( scissorRect );

                    Cmd.BindShaderResources( &GFrameResources.Resources );

                    drawCmd.IndexCountPerInstance = cmd->IndexCount;
                    drawCmd.StartIndexLocation = cmd->StartIndexLocation;
                    drawCmd.BaseVertexLocation = cmd->BaseVertexLocation;
                    Cmd.Draw( &drawCmd );
                    break;
                }

                case HUD_DRAW_CMD_MATERIAL:
                {
                    AN_ASSERT( cmd->MaterialFrameData );

                    AMaterialGPU * pMaterial = cmd->MaterialFrameData->Material;

                    AN_ASSERT( pMaterial->MaterialType == MATERIAL_TYPE_HUD );

                    Cmd.BindPipeline( &((AShadeModelHUD *)pMaterial->ShadeModel.HUD)->ColorPassHUD );
                    Cmd.BindVertexBuffer( 0, streamBuffer, drawList->VertexStreamOffset );
                    Cmd.BindIndexBuffer( streamBuffer, INDEX_TYPE_UINT16, drawList->IndexStreamOffset );

                    for ( int i = 0 ; i < pMaterial->NumSamplers ; i++ ) {
                        GFrameResources.SamplerBindings[i].pSampler = pMaterial->pSampler[i];
                    }

                    // FIXME: What about material uniforms?

                    BindTextures( cmd->MaterialFrameData );

                    scissorRect.X = cmd->ClipMins.X;
                    scissorRect.Y = cmd->ClipMins.Y;
                    scissorRect.Width = cmd->ClipMaxs.X - cmd->ClipMins.X;
                    scissorRect.Height = cmd->ClipMaxs.Y - cmd->ClipMins.Y;
                    Cmd.SetScissor( scissorRect );

                    Cmd.BindShaderResources( &GFrameResources.Resources );

                    drawCmd.IndexCountPerInstance = cmd->IndexCount;
                    drawCmd.StartIndexLocation = cmd->StartIndexLocation;
                    drawCmd.BaseVertexLocation = cmd->BaseVertexLocation;
                    Cmd.Draw( &drawCmd );
                    break;
                }

                default:
                {
                    Pipeline * pPipeline = ( cmd->Type == HUD_DRAW_CMD_ALPHA ) ? &AlphaPipeline[cmd->Blending] : &Pipelines[cmd->Blending];

                    Cmd.BindPipeline( pPipeline );
                    Cmd.BindVertexBuffer( 0, streamBuffer, drawList->VertexStreamOffset );
                    Cmd.BindIndexBuffer( streamBuffer, INDEX_TYPE_UINT16, drawList->IndexStreamOffset );

                    GFrameResources.TextureBindings[0].pTexture = GPUTextureHandle( cmd->Texture );
                    GFrameResources.SamplerBindings[0].pSampler = &Samplers[cmd->SamplerType];

                    scissorRect.X = cmd->ClipMins.X;
                    scissorRect.Y = cmd->ClipMins.Y;
                    scissorRect.Width = cmd->ClipMaxs.X - cmd->ClipMins.X;
                    scissorRect.Height = cmd->ClipMaxs.Y - cmd->ClipMins.Y;
                    Cmd.SetScissor( scissorRect );

                    Cmd.BindShaderResources( &GFrameResources.Resources );

                    drawCmd.IndexCountPerInstance = cmd->IndexCount;
                    drawCmd.StartIndexLocation = cmd->StartIndexLocation;
                    drawCmd.BaseVertexLocation = cmd->BaseVertexLocation;
                    Cmd.Draw( &drawCmd );
                    break;
                }
            }
        }
    }

    Cmd.EndRenderPass();
}

}
