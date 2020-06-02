#include "OpenGL45ShadowMapRenderer.h"
#include "OpenGL45Material.h"

using namespace GHI;

ARuntimeVariable RVShadowCascadeResolution( _CTS( "ShadowCascadeResolution" ), _CTS( "2048" ) );

extern ARuntimeVariable RVShadowCascadeBits;

namespace OpenGL45 {

static const float EVSM_positiveExponent = 40.0;
static const float EVSM_negativeExponent = 5.0;
static const Float2 EVSM_WarpDepth( std::exp( EVSM_positiveExponent ), -std::exp( -EVSM_negativeExponent ) );
const Float4 EVSM_ClearValue( EVSM_WarpDepth.X, EVSM_WarpDepth.Y, EVSM_WarpDepth.X*EVSM_WarpDepth.X, EVSM_WarpDepth.Y*EVSM_WarpDepth.Y );
const Float4 VSM_ClearValue( 1.0f );

AShadowMapRenderer::AShadowMapRenderer()
{
    CreatePipeline();
}

void AShadowMapRenderer::CreatePipeline() {
    AString codeVS = LoadShader( "instance_shadowmap_default.vert" );
    AString codeGS = LoadShader( "instance_shadowmap.geom" );
    AString codeFS = LoadShader( "instance_shadowmap_default.frag" );

    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.bScissorEnable = SCISSOR_TEST;
#if defined SHADOWMAP_VSM
    //Desc.CullMode = POLYGON_CULL_FRONT; // Less light bleeding
    Desc.CullMode = POLYGON_CULL_DISABLED;
#else
    //rsd.CullMode = POLYGON_CULL_BACK;
    //rsd.CullMode = POLYGON_CULL_DISABLED; // Less light bleeding
    rsd.CullMode = POLYGON_CULL_FRONT;
#endif
    //rsd.CullMode = POLYGON_CULL_DISABLED;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    //bsd.RenderTargetSlots[0].ColorWriteMask = COLOR_WRITE_DISABLED;  // FIXME: there is no fragment shader, so we realy need to disable color mask?
#if defined SHADOWMAP_VSM
    bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_NO_BLEND );
#endif

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();
    dssd.DepthFunc = CMPFUNC_LESS;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( Float3 );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = 1;
    pipelineCI.pVertexBindings = vertexBinding;

    constexpr VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            0
        }
    };

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo stages[3] = {};

    pipelineCI.NumStages = 0;

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule;
    ShaderModule geometryShaderModule;
    ShaderModule fragmentShaderModule;

    GShaderSources.Clear();
    //GShaderSources.Add( "#define SKINNED_MESH\n" );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( codeVS.CStr() );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    ShaderStageInfo & vs = stages[pipelineCI.NumStages++];
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    GShaderSources.Clear();
    //GShaderSources.Add( "#define SKINNED_MESH\n" );
    GShaderSources.Add( codeGS.CStr() );
    GShaderSources.Build( GEOMETRY_SHADER, &geometryShaderModule );

    ShaderStageInfo & gs = stages[pipelineCI.NumStages++];
    gs.Stage = SHADER_STAGE_GEOMETRY_BIT;
    gs.pModule = &geometryShaderModule;

    bool bVSM = false;

#if defined SHADOWMAP_VSM || defined SHADOWMAP_EVSM
    bVSM = true;
#endif

    if ( /*_ShadowMasking || */bVSM ) {
        GShaderSources.Clear();
        //GShaderSources.Add( "#define SHADOW_MASKING\n" );
        //GShaderSources.Add( "#define SKINNED_MESH\n" );
        GShaderSources.Add( codeFS.CStr() );
        GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

        ShaderStageInfo & fs = stages[pipelineCI.NumStages++];
        fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
        fs.pModule = &fragmentShaderModule;
    }

    pipelineCI.pStages = stages;
    //pipelineCI.pRenderPass = GetRenderPass();

    StaticShadowCasterPipeline.Initialize( pipelineCI );
}

bool AShadowMapRenderer::BindMaterialShadowMap( SShadowRenderInstance const * instance )
{
    AMaterialGPU * pMaterial = instance->Material;

    if ( pMaterial ) {
        bool bSkinned = instance->SkeletonSize > 0;
        Pipeline * pPipeline;

        // Choose pipeline
        switch ( pMaterial->MaterialType ) {
        case MATERIAL_TYPE_PBR:
        case MATERIAL_TYPE_BASELIGHT:
            pPipeline = bSkinned ? &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ShadowPassSkinned
                                 : &((AShadeModelLit*)pMaterial->ShadeModel.Lit)->ShadowPass;
            break;
        case MATERIAL_TYPE_UNLIT:
            pPipeline = bSkinned ? &((AShadeModelUnlit*)pMaterial->ShadeModel.Unlit)->ShadowPassSkinned
                                 : &((AShadeModelUnlit*)pMaterial->ShadeModel.Unlit)->ShadowPass;
            break;
        default:
            return false;
        }

        // Bind pipeline
        Cmd.BindPipeline( pPipeline );

        // Bind second vertex buffer
        if ( bSkinned ) {
            Buffer * pSecondVertexBuffer = GPUBufferHandle( instance->WeightsBuffer );
            Cmd.BindVertexBuffer( 1, pSecondVertexBuffer, instance->WeightsBufferOffset );
        } else {
            Cmd.BindVertexBuffer( 1, nullptr, 0 );
        }

        // Set samplers
        if ( pMaterial->bShadowMapPassTextureFetch ) {
            for ( int i = 0 ; i < pMaterial->NumSamplers ; i++ ) {
                GFrameResources.SamplerBindings[i].pSampler = pMaterial->pSampler[i];
            }
        }
    } else {
        Cmd.BindPipeline( &StaticShadowCasterPipeline );
        Cmd.BindVertexBuffer( 1, nullptr, 0 );
    }

    // Bind vertex and index buffers
    BindVertexAndIndexBuffers( instance );

    return true;
}

void BindTexturesShadowMap( SMaterialFrameData * _Instance )
{
    if ( !_Instance || !_Instance->Material->bShadowMapPassTextureFetch ) {
        return;
    }

    BindTextures( _Instance );
}

#if defined SHADOWMAP_VSM
static void BlurDepthMoments() {
    GHI_Framebuffer_t * FB = RenderFrame.Statement->GetFramebuffer< SHADOWMAP_FRAMEBUFFER >();

    GHI_SetDepthStencilTarget( RenderFrame.State, FB, NULL );
    GHI_SetDepthStencilState( RenderFrame.State, FDepthStencilState< false, GHI_DepthWrite_Disable >().HardwareState() );
    GHI_SetRasterizerState( RenderFrame.State, FRasterizerState< GHI_FillSolid, GHI_CullFront >().HardwareState() );
    GHI_SetSampler( RenderFrame.State, 0, ShadowDepthSampler1 );

    GHI_SetInputAssembler( RenderFrame.State, RenderFrame.SaqIA );
    GHI_SetPrimitiveTopology( RenderFrame.State, GHI_Prim_TriangleStrip );

    GHI_Viewport_t Viewport;
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.MinDepth = 0;
    Viewport.MaxDepth = 1;

    // Render horizontal blur

    GHI_Texture_t * DepthMomentsTextureTmp = ShadowPool_DepthMomentsVSMBlur();

    GHI_SetFramebufferSize( RenderFrame.State, FB, DepthMomentsTextureTmp->GetDesc().Width, DepthMomentsTextureTmp->GetDesc().Height );
    Viewport.Width = DepthMomentsTextureTmp->GetDesc().Width;
    Viewport.Height = DepthMomentsTextureTmp->GetDesc().Height;
    GHI_SetViewport( RenderFrame.State, &Viewport );

    RenderTargetVSM.Texture = DepthMomentsTextureTmp;
    GHI_SetRenderTargets( RenderFrame.State, FB, 1, &RenderTargetVSM );
    GHI_SetProgramPipeline( RenderFrame.State, RenderFrame.Statement->GetProgramAndAttach< FGaussianBlur9HProgram, FSaqVertex >() );
    GHI_AssignTextureUnit( RenderFrame.State, 0, ShadowPool_DepthMomentsVSM() );
    GHI_DrawInstanced( RenderFrame.State, 4, NumShadowMapCascades, 0 );

    // Render vertical blur

    GHI_SetFramebufferSize( RenderFrame.State, FB, r_ShadowCascadeResolution.GetInteger(), r_ShadowCascadeResolution.GetInteger() );
    Viewport.Width = r_ShadowCascadeResolution.GetInteger();
    Viewport.Height = r_ShadowCascadeResolution.GetInteger();
    GHI_SetViewport( RenderFrame.State, &Viewport );

    RenderTargetVSM.Texture = ShadowPool_DepthMomentsVSM();
    GHI_SetRenderTargets( RenderFrame.State, FB, 1, &RenderTargetVSM );
    GHI_SetProgramPipeline( RenderFrame.State, RenderFrame.Statement->GetProgramAndAttach< FGaussianBlur9VProgram, FSaqVertex >() );
    GHI_AssignTextureUnit( RenderFrame.State, 0, DepthMomentsTextureTmp );
    GHI_DrawInstanced( RenderFrame.State, 4, NumShadowMapCascades, 0 );
}
#endif

AFrameGraphTextureStorage * AShadowMapRenderer::AddPass( AFrameGraph & FrameGraph )
{
    int cascadeWidth = RVShadowCascadeResolution.GetInteger();
    int cascadeHeight = RVShadowCascadeResolution.GetInteger();
    int totalCascades = GFrameData->ShadowCascadePoolSize;

    GHI::INTERNAL_PIXEL_FORMAT depthFormat;
    if ( RVShadowCascadeBits.GetInteger() <= 16 ) {
        depthFormat = INTERNAL_PIXEL_FORMAT_DEPTH16;
    } else if ( RVShadowCascadeBits.GetInteger() <= 24 ) {
        depthFormat = INTERNAL_PIXEL_FORMAT_DEPTH24;
    } else {
        depthFormat = INTERNAL_PIXEL_FORMAT_DEPTH32;
    }

    ARenderPass & pass = FrameGraph.AddTask< ARenderPass >( "ShadowMap Pass" );

    pass.SetRenderArea( cascadeWidth, cascadeHeight );

    pass.SetDepthStencilAttachment(
    {
        "Shadow Cascade Depth texture",
        MakeTextureStorage( depthFormat, TextureResolution2DArray( cascadeWidth, cascadeHeight, totalCascades ) ),
        GHI::AttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_CLEAR )
    } );

    pass.SetDepthStencilClearValue( MakeClearDepthStencilValue( 1, 0 ) );

#if defined SHADOWMAP_EVSM || defined SHADOWMAP_VSM
#ifdef SHADOWMAP_EVSM
    InternalFormat = GHI_IPF_RGBA32F,
#else
    InternalFormat = GHI_IPF_RG32F,
#endif
    pass.SetColorAttachments(
    {
        {
            "Shadow Cascade Color texture",
            MakeTextureStorage( InternalFromat, TextureResolution2DArray( cascadeWidth, cascadeHeight, totalCascades ) ),
            GHI::AttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_CLEAR )
        },
        {
            "Shadow Cascade Color texture 2",
            MakeTextureStorage( InternalFromat, TextureResolution2DArray( cascadeWidth, cascadeHeight, totalCascades ) ),
            GHI::AttachmentInfo().SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE )
        },
    } );

    // TODO: очищать только нужные слои!
    pass.SetClearColors(
    {
#if defined SHADOWMAP_EVSM
        MakeClearColorValue( EVSM_ClearValue )
#elif defined SHADOWMAP_VSM
        MakeClearColorValue( VSM_ClearValue )
#endif
    } );
#endif

    pass.AddSubpass( {}, // no color attachments
                     [=]( ARenderPass const & RenderPass, int SubpassIndex )
    {
        if ( !GRenderView->NumShadowMapCascades ) {
            return;
        }

        if ( !GRenderView->ShadowInstanceCount ) {
            return;
        }

        DrawIndexedCmd drawCmd;
        drawCmd.StartInstanceLocation = 0;

        for ( int i = 0 ; i < GRenderView->ShadowInstanceCount ; i++ ) {
            SShadowRenderInstance const * instance = GFrameData->ShadowInstances[GRenderView->FirstShadowInstance + i];

            if ( !BindMaterialShadowMap( instance ) ) {
                continue;
            }

            // Set material data (textures, uniforms)
            BindTexturesShadowMap( instance->MaterialInstance );

            // Bind skeleton
            BindSkeleton( instance->SkeletonOffset, instance->SkeletonSize );

            // Set instance uniforms
            SetShadowInstanceUniforms( instance, i );

            Cmd.BindShaderResources( &GFrameResources.Resources );

            // TODO: должна быть предварительная проверка видимости (рассчитано закранее: instanceFirstCascade, instanceNumCascades)
            // instanceFirstCascade нужно записать в uniformBuffer, принадлежащий инстансу,
            // а drawCmd.InstanceCount = instanceNumCascades;
            // Далее в геометрическом шейдере gl_Layer = gl_InstanceId + instanceFirstCascade;
            // TODO: добавить к юниформам инстанса битовое поле uint16_t CascadeMask, для того чтобы в геометрическом шедере можно было сделать фильтр:
            // if ( CascadeMask & gl_LayerId ) {
            //      ... EmitPrimitive
            // } else {
            //      nothing
            // }
            drawCmd.InstanceCount = GRenderView->NumShadowMapCascades;
            drawCmd.IndexCountPerInstance = instance->IndexCount;
            drawCmd.StartIndexLocation = instance->StartIndexLocation;
            drawCmd.BaseVertexLocation = instance->BaseVertexLocation;

            Cmd.Draw( &drawCmd );
        }
    } );

    return pass.GetDepthStencilAttachment().Resource;
}

}
