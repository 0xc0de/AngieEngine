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

#include <World/Public/Render/RenderFrontend.h>
#include <World/Public/World.h>
#include <World/Public/Components/CameraComponent.h>
#include <World/Public/Components/SkinnedComponent.h>
#include <World/Public/Components/DirectionalLightComponent.h>
#include <World/Public/Components/AnalyticLightComponent.h>
#include <World/Public/Components/IBLComponent.h>
#include <World/Public/Components/TerrainComponent.h>
#include <World/Public/Actors/PlayerController.h>
#include <World/Public/Widgets/WDesktop.h>
#include <World/Public/EngineInstance.h>
#include <Runtime/Public/Runtime.h>
#include <Runtime/Public/ScopedTimeCheck.h>
#include <Core/Public/IntrusiveLinkedListMacro.h>

ARuntimeVariable r_FixFrustumClusters( _CTS( "r_FixFrustumClusters" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable r_RenderView( _CTS( "r_RenderView" ), _CTS( "1" ), VAR_CHEAT );
ARuntimeVariable r_RenderSurfaces( _CTS( "r_RenderSurfaces" ), _CTS( "1" ), VAR_CHEAT );
ARuntimeVariable r_RenderMeshes( _CTS( "r_RenderMeshes" ), _CTS( "1" ), VAR_CHEAT );
ARuntimeVariable r_RenderTerrain( _CTS( "r_RenderTerrain" ), _CTS( "1" ), VAR_CHEAT );
ARuntimeVariable r_ResolutionScaleX( _CTS( "r_ResolutionScaleX" ), _CTS( "1" ) );
ARuntimeVariable r_ResolutionScaleY( _CTS( "r_ResolutionScaleY" ), _CTS( "1" ) );
ARuntimeVariable r_RenderLightPortals( _CTS( "r_RenderLightPortals" ), _CTS( "1" ) );

ARuntimeVariable com_DrawFrustumClusters( _CTS( "com_DrawFrustumClusters" ), _CTS( "0" ), VAR_CHEAT );

AN_CLASS_META( ARenderFrontend )

static constexpr int TerrainTileSize = 256;//32;//256;

ARenderFrontend::ARenderFrontend()
{
    TerrainMesh = MakeRef< ATerrainMesh >( TerrainTileSize );

    PhotometricProfiles = CreateInstanceOf< ATexture >();
    PhotometricProfiles->Initialize1DArray( TEXTURE_PF_R8_UNORM, 1, 256, 256 );
}

ARenderFrontend::~ARenderFrontend()
{
}

struct SInstanceSortFunction {
    bool operator() ( SRenderInstance const * _A, SRenderInstance * _B ) {
        return _A->SortKey < _B->SortKey;
    }
} InstanceSortFunction;

struct SShadowInstanceSortFunction {
    bool operator() ( SShadowRenderInstance const * _A, SShadowRenderInstance * _B ) {
        return _A->SortKey < _B->SortKey;
    }
} ShadowInstanceSortFunction;

void ARenderFrontend::Render( ACanvas * InCanvas ) {
    FrameData.FrameNumber = FrameNumber = GRuntime->SysFrameNumber();
    FrameData.DrawListHead = nullptr;
    FrameData.DrawListTail = nullptr;

    Stat.FrontendTime = Core::SysMilliseconds();
    Stat.PolyCount = 0;
    Stat.ShadowMapPolyCount = 0;

    MaxViewportWidth = 1;
    MaxViewportHeight = 1;
    Viewports.Clear();

    RenderCanvas( InCanvas );

    //RenderImgui();

    FrameData.RenderTargetMaxWidthP = FrameData.RenderTargetMaxWidth;
    FrameData.RenderTargetMaxHeightP = FrameData.RenderTargetMaxHeight;
    FrameData.RenderTargetMaxWidth = MaxViewportWidth;
    FrameData.RenderTargetMaxHeight = MaxViewportHeight;

    FrameData.CanvasWidth = InCanvas->GetWidth();
    FrameData.CanvasHeight = InCanvas->GetHeight();

    const Float2 orthoMins( 0.0f, (float)FrameData.CanvasHeight );
    const Float2 orthoMaxs( (float)FrameData.CanvasWidth, 0.0f );
    FrameData.CanvasOrthoProjection = Float4x4::Ortho2DCC( orthoMins, orthoMaxs );

    FrameData.Instances.Clear();
    FrameData.TranslucentInstances.Clear();
    FrameData.OutlineInstances.Clear();
    FrameData.ShadowInstances.Clear();
    FrameData.LightPortals.Clear();
    FrameData.DirectionalLights.Clear();
    FrameData.LightShadowmaps.Clear();
    FrameData.TerrainInstances.Clear();

    //FrameData.ShadowCascadePoolSize = 0;
    DebugDraw.Reset();

    // Allocate views
    FrameData.NumViews = Viewports.Size();
    FrameData.RenderViews = (SRenderView *)GRuntime->AllocFrameMem( sizeof( SRenderView ) * FrameData.NumViews );

    for ( int i = 0 ; i < FrameData.NumViews ; i++ ) {
        RenderView( i );
    }

    //int64_t t = GRuntime->SysMilliseconds();

    for ( SRenderView * view = FrameData.RenderViews ; view < &FrameData.RenderViews[FrameData.NumViews] ; view++ ) {
        StdSort( FrameData.Instances.Begin() + view->FirstInstance,
                 FrameData.Instances.Begin() + ( view->FirstInstance + view->InstanceCount ),
                 InstanceSortFunction );

        StdSort( FrameData.TranslucentInstances.Begin() + view->FirstTranslucentInstance,
                 FrameData.TranslucentInstances.Begin() + (view->FirstTranslucentInstance + view->TranslucentInstanceCount),
                 InstanceSortFunction );
    }
    //GLogger.Printf( "Sort instances time %d instances count %d\n", GRuntime->SysMilliseconds() - t, FrameData.Instances.Size() + FrameData.ShadowInstances.Size() );

    if ( DebugDraw.CommandsCount() > 0 ) {
        AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

        FrameData.DbgCmds = DebugDraw.GetCmds().ToPtr();
        FrameData.DbgVertexStreamOffset = streamedMemory->AllocateVertex( DebugDraw.GetVertices().Size() * sizeof( SDebugVertex ), DebugDraw.GetVertices().ToPtr() );
        FrameData.DbgIndexStreamOffset = streamedMemory->AllocateVertex( DebugDraw.GetIndices().Size() * sizeof( unsigned short ), DebugDraw.GetIndices().ToPtr() );
    }

    Stat.FrontendTime = Core::SysMilliseconds() - Stat.FrontendTime;
}

void ARenderFrontend::RenderView( int _Index ) {
    SViewport * viewport = const_cast< SViewport * >( Viewports[ _Index ] );
    ARenderingParameters * RP = viewport->RenderingParams;
    ACameraComponent * camera = viewport->Camera;
    AWorld * world = camera->GetWorld();
    SRenderView * view = &FrameData.RenderViews[_Index];
    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    AN_ASSERT( RP ); // TODO: Don't allow <null> rendering parameters

    view->GameRunningTimeSeconds = world->GetRunningTimeMicro() * 0.000001;
    view->GameplayTimeSeconds = world->GetGameplayTimeMicro() * 0.000001;
    view->GameplayTimeStep = world->IsPaused() ? 0.0f : Math::Max( GRuntime->SysFrameDuration() * 0.000001f, 0.0001f );
    view->ViewIndex = _Index;
    //view->Width = Align( (size_t)(viewport->Width * r_ResolutionScaleX.GetFloat()), 2 );
    //view->Height = Align( (size_t)(viewport->Height * r_ResolutionScaleY.GetFloat()), 2 );
    view->WidthP = viewport->ScaledWidth;
    view->HeightP = viewport->ScaledHeight;
    view->Width = viewport->ScaledWidth = viewport->Width * r_ResolutionScaleX.GetFloat();
    view->Height = viewport->ScaledHeight = viewport->Height * r_ResolutionScaleY.GetFloat();
    view->WidthR = viewport->Width;
    view->HeightR = viewport->Height;

    if ( camera )
    {
        view->ViewPosition = camera->GetWorldPosition();
        view->ViewRotation = camera->GetWorldRotation();
        view->ViewRightVec = camera->GetWorldRightVector();
        view->ViewUpVec = camera->GetWorldUpVector();
        view->ViewDir = camera->GetWorldForwardVector();
        view->ViewMatrix = camera->GetViewMatrix();
        view->ProjectionMatrix = camera->GetProjectionMatrix();

        view->ViewMatrixP = RP->ViewMatrix;
        view->ProjectionMatrixP = RP->ProjectionMatrix;

        RP->ViewMatrix = view->ViewMatrix;
        RP->ProjectionMatrix = view->ProjectionMatrix;

        view->ViewZNear = camera->GetZNear();
        view->ViewZFar = camera->GetZFar();
        view->ViewOrthoMins = camera->GetOrthoMins();
        view->ViewOrthoMaxs = camera->GetOrthoMaxs();
        camera->GetEffectiveFov( view->ViewFovX, view->ViewFovY );
        view->bPerspective = camera->IsPerspective();
        view->MaxVisibleDistance = camera->GetZFar(); // TODO: расчитать дальность до самой дальней точки на экране (по баундингам static&skinned mesh)
        view->NormalToViewMatrix = Float3x3( view->ViewMatrix );

        view->InverseProjectionMatrix = camera->IsPerspective() ?
                    view->ProjectionMatrix.PerspectiveProjectionInverseFast()
                  : view->ProjectionMatrix.OrthoProjectionInverseFast();
        camera->MakeClusterProjectionMatrix( view->ClusterProjectionMatrix );

        view->ClusterViewProjection = view->ClusterProjectionMatrix * view->ViewMatrix; // TODO: try to optimize with ViewMatrix.ViewInverseFast() * ProjectionMatrix.ProjectionInverseFast()
        view->ClusterViewProjectionInversed = view->ClusterViewProjection.Inversed();
    }

    view->ViewProjection = view->ProjectionMatrix * view->ViewMatrix;
    view->ViewProjectionP = view->ProjectionMatrixP * view->ViewMatrixP;
    view->ViewSpaceToWorldSpace = view->ViewMatrix.Inversed(); // TODO: Check with ViewInverseFast
    view->ClipSpaceToWorldSpace = view->ViewSpaceToWorldSpace * view->InverseProjectionMatrix;
    view->BackgroundColor = RP->BackgroundColor.GetRGB();
    view->bClearBackground = RP->bClearBackground;
    view->bWireframe = RP->bWireframe;
    if ( RP->bVignetteEnabled ) {
        view->VignetteColorIntensity = RP->VignetteColorIntensity;
        view->VignetteOuterRadiusSqr = RP->VignetteOuterRadiusSqr;
        view->VignetteInnerRadiusSqr = RP->VignetteInnerRadiusSqr;
    } else {
        view->VignetteColorIntensity.W = 0;
    }

    if ( RP->IsColorGradingEnabled() ) {
        view->ColorGradingLUT = RP->GetColorGradingLUT() ? RP->GetColorGradingLUT()->GetGPUResource() : NULL;
        view->CurrentColorGradingLUT = RP->GetCurrentColorGradingLUT()->GetGPUResource();
        view->ColorGradingAdaptationSpeed = RP->GetColorGradingAdaptationSpeed();

        // Procedural color grading
        view->ColorGradingGrain = RP->GetColorGradingGrain();
        view->ColorGradingGamma = RP->GetColorGradingGamma();
        view->ColorGradingLift = RP->GetColorGradingLift();
        view->ColorGradingPresaturation = RP->GetColorGradingPresaturation();
        view->ColorGradingTemperatureScale = RP->GetColorGradingTemperatureScale();
        view->ColorGradingTemperatureStrength = RP->GetColorGradingTemperatureStrength();
        view->ColorGradingBrightnessNormalization = RP->GetColorGradingBrightnessNormalization();

    } else {
        view->ColorGradingLUT = NULL;
        view->CurrentColorGradingLUT = NULL;
        view->ColorGradingAdaptationSpeed = 0;
    }

    view->CurrentExposure = RP->GetCurrentExposure()->GetGPUResource();

    // TODO: light and depth texture must have size of render view, not render target max w/h
    // FIXME: Do not initialize light&depth textures if screen space reflections disabled
    ATexture * lightTexture = RP->GetLightTexture();
    if ( lightTexture->GetDimensionX() != FrameData.RenderTargetMaxWidth
         || lightTexture->GetDimensionY() != FrameData.RenderTargetMaxHeight )
    {
        lightTexture->Initialize2D( TEXTURE_PF_R11F_G11F_B10F, 1, FrameData.RenderTargetMaxWidth, FrameData.RenderTargetMaxHeight );
    }

    ATexture * depthTexture = RP->GetDepthTexture();
    if ( depthTexture->GetDimensionX() != FrameData.RenderTargetMaxWidth
         || depthTexture->GetDimensionY() != FrameData.RenderTargetMaxHeight )
    {
        depthTexture->Initialize2D( TEXTURE_PF_R32F, 1, FrameData.RenderTargetMaxWidth, FrameData.RenderTargetMaxHeight );
    }

    view->LightTexture = lightTexture->GetGPUResource();
    view->DepthTexture = depthTexture->GetGPUResource();

    view->VTFeedback = &RP->VTFeedback;

    view->PhotometricProfiles = PhotometricProfiles->GetGPUResource();

    view->NumShadowMapCascades = 0;
    view->NumCascadedShadowMaps = 0;
    view->FirstInstance = FrameData.Instances.Size();
    view->InstanceCount = 0;
    view->FirstTranslucentInstance = FrameData.TranslucentInstances.Size();
    view->TranslucentInstanceCount = 0;
    view->FirstOutlineInstance = FrameData.OutlineInstances.Size();
    view->OutlineInstanceCount = 0;
    //view->FirstLightPortal = FrameData.LightPortals.Size();
    //view->LightPortalsCount = 0;
    //view->FirstShadowInstance = FrameData.ShadowInstances.Size();
    //view->ShadowInstanceCount = 0;
    view->FirstDirectionalLight = FrameData.DirectionalLights.Size();
    view->NumDirectionalLights = 0;
    view->FirstDebugDrawCommand = 0;
    view->DebugDrawCommandCount = 0;

    size_t size = MAX_TOTAL_SHADOW_CASCADES_PER_VIEW * sizeof( Float4x4 );

    view->ShadowMapMatricesStreamHandle = streamedMemory->AllocateConstant( size, nullptr );
    view->ShadowMapMatrices = (Float4x4 *)streamedMemory->Map( view->ShadowMapMatricesStreamHandle );

    size_t numFrustumClusters = MAX_FRUSTUM_CLUSTERS_X * MAX_FRUSTUM_CLUSTERS_Y * MAX_FRUSTUM_CLUSTERS_Z;

    view->ClusterLookupStreamHandle = streamedMemory->AllocateConstant( numFrustumClusters * sizeof( SClusterHeader ), nullptr );
    view->ClusterLookup = (SClusterHeader *)streamedMemory->Map( view->ClusterLookupStreamHandle );

    view->FirstTerrainInstance = FrameData.TerrainInstances.Size();
    view->TerrainInstanceCount = 0;

    if ( !r_RenderView || !camera ) {
        return;
    }

    world->E_OnPrepareRenderFrontend.Dispatch( camera, FrameNumber );

    RenderDef.FrameNumber = FrameNumber;
    RenderDef.View = view;
    RenderDef.Frustum = &camera->GetFrustum();
    RenderDef.VisibilityMask = RP ? RP->VisibilityMask : ~0;
    RenderDef.PolyCount = 0;
    RenderDef.ShadowMapPolyCount = 0;

    ViewRP = RP;

    QueryVisiblePrimitives( world );

    view->GlobalIrradianceMap = world->GetGlobalIrradianceMap();
    view->GlobalReflectionMap = world->GetGlobalReflectionMap();

    // Generate debug draw commands
    if ( RP && RP->bDrawDebug ) {
        DebugDraw.BeginRenderView( view, VisPass );
        world->DrawDebug( &DebugDraw );

        if ( com_DrawFrustumClusters ) {
            LightVoxelizer.DrawVoxels( &DebugDraw );
        }
    }

    AddRenderInstances( world );

    AddDirectionalShadowmapInstances( world );

    Stat.PolyCount += RenderDef.PolyCount;
    Stat.ShadowMapPolyCount += RenderDef.ShadowMapPolyCount;

    if ( RP && RP->bDrawDebug ) {

        for ( auto it : RP->TerrainViews ) {
            it.second->DrawDebug( &DebugDraw, TerrainMesh );
        }

        DebugDraw.EndRenderView();
    }
}

void ARenderFrontend::RenderCanvas( ACanvas * InCanvas ) {
    ImDrawList const * srcList = &InCanvas->GetDrawList();

    if ( srcList->VtxBuffer.empty() ) {
        return;
    }

    // Allocate draw list
    SHUDDrawList * drawList = ( SHUDDrawList * )GRuntime->AllocFrameMem( sizeof( SHUDDrawList ) );

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    // Copy vertex data
    drawList->VertexStreamOffset = streamedMemory->AllocateVertex( sizeof( SHUDDrawVert ) * srcList->VtxBuffer.Size, srcList->VtxBuffer.Data );
    drawList->IndexStreamOffset = streamedMemory->AllocateIndex( sizeof( unsigned short ) * srcList->IdxBuffer.Size, srcList->IdxBuffer.Data );

    // Allocate commands
    drawList->Commands = ( SHUDDrawCmd * )GRuntime->AllocFrameMem( sizeof( SHUDDrawCmd ) * srcList->CmdBuffer.Size );

    drawList->CommandsCount = 0;

    // Parse ImDrawCmd, create HUDDrawCmd-s
    SHUDDrawCmd * dstCmd = drawList->Commands;
    for ( ImDrawCmd const & cmd : srcList->CmdBuffer )
    {
        // TextureId can contain a viewport index, material instance or gpu texture
        if ( !cmd.TextureId )
        {
            GLogger.Printf( "ARenderFrontend::RenderCanvas: invalid command (TextureId==0)\n" );
            continue;
        }

        Core::Memcpy( &dstCmd->ClipMins, &cmd.ClipRect, sizeof( Float4 ) );
        dstCmd->IndexCount = cmd.ElemCount;
        dstCmd->StartIndexLocation = cmd.IdxOffset;
        dstCmd->BaseVertexLocation = cmd.VtxOffset;
        dstCmd->Type = (EHUDDrawCmd)( cmd.BlendingState & 0xff );
        dstCmd->Blending = (EColorBlending)( ( cmd.BlendingState >> 8 ) & 0xff );
        dstCmd->SamplerType = (EHUDSamplerType)( ( cmd.BlendingState >> 16 ) & 0xff );

        switch ( dstCmd->Type ) {
        case HUD_DRAW_CMD_VIEWPORT: {
            // Unpack viewport
            SViewport const * viewport = &InCanvas->GetViewports()[ (size_t)cmd.TextureId - 1 ];

            // Save pointer to viewport to array of viewports
            Viewports.Append( viewport );

            // Set viewport index in array of viewports
            dstCmd->ViewportIndex = Viewports.Size() - 1;

            // Calc max viewport size
            MaxViewportWidth = Math::Max( MaxViewportWidth, viewport->Width );
            MaxViewportHeight = Math::Max( MaxViewportHeight, viewport->Height );

            break;
        }

        case HUD_DRAW_CMD_MATERIAL: {
            // Unpack material instance
            AMaterialInstance * materialInstance = static_cast< AMaterialInstance * >( cmd.TextureId );

            // In normal case materialInstance never be null
            AN_ASSERT( materialInstance );

            // Get material
            AMaterial * material = materialInstance->GetMaterial();

            // GetMaterial never return null
            AN_ASSERT( material );

            // Check material type
            if ( material->GetType() != MATERIAL_TYPE_HUD )
            {
                GLogger.Printf( "ARenderFrontend::RenderCanvas: expected MATERIAL_TYPE_HUD\n" );
                continue;
            }

            // Update material frame data
            dstCmd->MaterialFrameData = materialInstance->PreRenderUpdate( FrameNumber );

            if ( !dstCmd->MaterialFrameData )
            {
                // Out of frame memory?
                continue;
            }

            break;
        }

        case HUD_DRAW_CMD_TEXTURE:
        case HUD_DRAW_CMD_ALPHA: {
            // Unpack texture
            dstCmd->Texture = (RenderCore::ITexture *)cmd.TextureId;
            break;
        }

        default:
            GLogger.Printf( "ARenderFrontend::RenderCanvas: unknown command type\n" );
            continue;
        }

        // Switch to next cmd
        dstCmd++;
        drawList->CommandsCount++;
    }

    // Add drawList
    SHUDDrawList * prev = FrameData.DrawListTail;
    drawList->pNext = nullptr;
    FrameData.DrawListTail = drawList;
    if ( prev ) {
        prev->pNext = drawList;
    } else {
        FrameData.DrawListHead = drawList;
    }
}

#if 0

void ARenderFrontend::RenderImgui() {
    ImDrawData * drawData = ImGui::GetDrawData();
    if ( drawData && drawData->CmdListsCount > 0 ) {
        // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
        int fb_width = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        int fb_height = drawData->DisplaySize.y * drawData->FramebufferScale.y;
        if ( fb_width == 0 || fb_height == 0 ) {
            return;
        }

        if ( drawData->FramebufferScale.x != 1.0f || drawData->FramebufferScale.y != 1.0f ) {
            drawData->ScaleClipRects( drawData->FramebufferScale );
        }

        bool bDrawMouseCursor = true;

        if ( bDrawMouseCursor ) {
            ImGuiMouseCursor cursor = ImGui::GetCurrentContext()->MouseCursor;

            ImDrawList * drawList = drawData->CmdLists[ drawData->CmdListsCount - 1 ];
            if ( cursor != ImGuiMouseCursor_None )
            {
                AN_ASSERT( cursor > ImGuiMouseCursor_None && cursor < ImGuiMouseCursor_COUNT );

                const ImU32 col_shadow = IM_COL32( 0, 0, 0, 48 );
                const ImU32 col_border = IM_COL32( 0, 0, 0, 255 );          // Black
                const ImU32 col_fill = IM_COL32( 255, 255, 255, 255 );    // White

                Float2 pos = AInputComponent::GetCursorPosition();
                float scale = 1.0f;

                ImFontAtlas* font_atlas = drawList->_Data->Font->ContainerAtlas;
                Float2 offset, size, uv[ 4 ];
                if ( font_atlas->GetMouseCursorTexData( cursor, (ImVec2*)&offset, (ImVec2*)&size, (ImVec2*)&uv[ 0 ], (ImVec2*)&uv[ 2 ] ) ) {
                    pos -= offset;
                    const ImTextureID tex_id = font_atlas->TexID;
                    drawList->PushClipRectFullScreen();
                    drawList->PushTextureID( tex_id );
                    drawList->AddImage( tex_id, pos + Float2( 1, 0 )*scale, pos + Float2( 1, 0 )*scale + size*scale, uv[ 2 ], uv[ 3 ], col_shadow );
                    drawList->AddImage( tex_id, pos + Float2( 2, 0 )*scale, pos + Float2( 2, 0 )*scale + size*scale, uv[ 2 ], uv[ 3 ], col_shadow );
                    drawList->AddImage( tex_id, pos, pos + size*scale, uv[ 2 ], uv[ 3 ], col_border );
                    drawList->AddImage( tex_id, pos, pos + size*scale, uv[ 0 ], uv[ 1 ], col_fill );
                    drawList->PopTextureID();
                    drawList->PopClipRect();
                }
            }
        }

        for ( int n = 0; n < drawData->CmdListsCount ; n++ ) {
            RenderImgui( drawData->CmdLists[ n ] );
        }
    }
}

void ARenderFrontend::RenderImgui( ImDrawList const * _DrawList ) {
    ImDrawList const * srcList = _DrawList;

    if ( srcList->VtxBuffer.empty() ) {
        return;
    }

    SHUDDrawList * drawList = ( SHUDDrawList * )GRuntime->AllocFrameMem( sizeof( SHUDDrawList ) );

    drawList->VerticesCount = srcList->VtxBuffer.size();
    drawList->IndicesCount = srcList->IdxBuffer.size();
    drawList->CommandsCount = srcList->CmdBuffer.size();

    int bytesCount = sizeof( SHUDDrawVert ) * drawList->VerticesCount;
    drawList->Vertices = ( SHUDDrawVert * )GRuntime->AllocFrameMem( bytesCount );

    Core::Memcpy( drawList->Vertices, srcList->VtxBuffer.Data, bytesCount );

    bytesCount = sizeof( unsigned short ) * drawList->IndicesCount;
    drawList->Indices = ( unsigned short * )GRuntime->AllocFrameMem( bytesCount );

    Core::Memcpy( drawList->Indices, srcList->IdxBuffer.Data, bytesCount );

    bytesCount = sizeof( SHUDDrawCmd ) * drawList->CommandsCount;
    drawList->Commands = ( SHUDDrawCmd * )GRuntime->AllocFrameMem( bytesCount );

    int startIndexLocation = 0;

    SHUDDrawCmd * dstCmd = drawList->Commands;
    for ( const ImDrawCmd * pCmd = srcList->CmdBuffer.begin() ; pCmd != srcList->CmdBuffer.end() ; pCmd++ ) {

        Core::Memcpy( &dstCmd->ClipMins, &pCmd->ClipRect, sizeof( Float4 ) );
        dstCmd->IndexCount = pCmd->ElemCount;
        dstCmd->StartIndexLocation = startIndexLocation;
        dstCmd->Type = (EHUDDrawCmd)( pCmd->BlendingState & 0xff );
        dstCmd->Blending = (EColorBlending)( ( pCmd->BlendingState >> 8 ) & 0xff );
        dstCmd->SamplerType = (EHUDSamplerType)( ( pCmd->BlendingState >> 16 ) & 0xff );

        startIndexLocation += pCmd->ElemCount;

        AN_ASSERT( pCmd->TextureId );

        switch ( dstCmd->Type ) {
        case HUD_DRAW_CMD_VIEWPORT:
        {
            drawList->CommandsCount--;
            continue;
        }

        case HUD_DRAW_CMD_MATERIAL:
        {
            AMaterialInstance * materialInstance = static_cast< AMaterialInstance * >( pCmd->TextureId );
            AN_ASSERT( materialInstance );

            AMaterial * material = materialInstance->GetMaterial();
            AN_ASSERT( material );

            if ( material->GetType() != MATERIAL_TYPE_HUD ) {
                drawList->CommandsCount--;
                continue;
            }

            dstCmd->MaterialFrameData = materialInstance->PreRenderUpdate( FrameNumber );
            AN_ASSERT( dstCmd->MaterialFrameData );

            dstCmd++;

            break;
        }
        case HUD_DRAW_CMD_TEXTURE:
        case HUD_DRAW_CMD_ALPHA:
        {
            dstCmd->Texture = (RenderCore::ITexture *)pCmd->TextureId;
            dstCmd++;
            break;
        }
        default:
            AN_ASSERT( 0 );
            break;
        }
    }

    SHUDDrawList * prev = FrameData.DrawListTail;
    drawList->pNext = nullptr;
    FrameData.DrawListTail = drawList;
    if ( prev ) {
        prev->pNext = drawList;
    } else {
        FrameData.DrawListHead = drawList;
    }
}

#endif

void ARenderFrontend::QueryVisiblePrimitives( AWorld * InWorld ) {
    SVisibilityQuery query;

    for ( int i = 0 ; i < 6 ; i++ ) {
        query.FrustumPlanes[i] = &(*RenderDef.Frustum)[i];
    }
    query.ViewPosition = RenderDef.View->ViewPosition;
    query.ViewRightVec = RenderDef.View->ViewRightVec;
    query.ViewUpVec = RenderDef.View->ViewUpVec;
    query.VisibilityMask = RenderDef.VisibilityMask;
    query.QueryMask = VSD_QUERY_MASK_VISIBLE | VSD_QUERY_MASK_VISIBLE_IN_LIGHT_PASS;// | VSD_QUERY_MASK_SHADOW_CAST;

    InWorld->QueryVisiblePrimitives( VisPrimitives, VisSurfaces, &VisPass, query );
}

void ARenderFrontend::QueryShadowCasters( AWorld * InWorld, Float4x4 const & LightViewProjection, Float3 const & LightPosition, Float3x3 const & LightBasis,
                                          TPodVector< SPrimitiveDef * > & Primitives, TPodVector< SSurfaceDef * > & Surfaces )
{
    SVisibilityQuery query;
    BvFrustum frustum;

    frustum.FromMatrix( LightViewProjection, true );

    for ( int i = 0 ; i < 6 ; i++ ) {
        query.FrustumPlanes[i] = &frustum[i];
    }
    query.ViewPosition = LightPosition;
    query.ViewRightVec = LightBasis[0];
    query.ViewUpVec = LightBasis[1];
    query.VisibilityMask = RenderDef.VisibilityMask;
    query.QueryMask = VSD_QUERY_MASK_VISIBLE | VSD_QUERY_MASK_SHADOW_CAST;
#if 0
#if 0
    Float3 clipBox[8] =
    {
        Float3(-1,-1, 0),
        Float3(-1,-1, 1),
        Float3(-1, 1, 0),
        Float3(-1, 1, 1),
        Float3( 1,-1, 0),
        Float3( 1,-1, 1),
        Float3( 1, 1, 0),
        Float3( 1, 1, 1)
    };

    Float4x4 inversed = LightViewProjection.Inversed();
    for ( int i = 0 ; i < 8 ; i++ ) {
        clipBox[i] = Float3( inversed * Float4( clipBox[i], 1.0f ) );
    }

    DebugDraw.SetDepthTest( false );
    DebugDraw.SetColor(AColor4(1,1,0,1));
    DebugDraw.DrawLine( clipBox, 8 );
#else
    Float3 vectorTR;
    Float3 vectorTL;
    Float3 vectorBR;
    Float3 vectorBL;
    Float3 origin = LightPosition;
    Float3 v[4];
    Float3 faces[4][3];
    float lightRadius = 4;
    float rayLength = lightRadius / Math::Cos( Math::_PI/4.0f );

    frustum.CornerVector_TR( vectorTR );
    frustum.CornerVector_TL( vectorTL );
    frustum.CornerVector_BR( vectorBR );
    frustum.CornerVector_BL( vectorBL );

    v[0] = origin + vectorTR * rayLength;
    v[1] = origin + vectorBR * rayLength;
    v[2] = origin + vectorBL * rayLength;
    v[3] = origin + vectorTL * rayLength;

    // top
    faces[0][0] = origin;
    faces[0][1] = v[0];
    faces[0][2] = v[3];

    // left
    faces[1][0] = origin;
    faces[1][1] = v[3];
    faces[1][2] = v[2];

    // bottom
    faces[2][0] = origin;
    faces[2][1] = v[2];
    faces[2][2] = v[1];

    // right
    faces[3][0] = origin;
    faces[3][1] = v[1];
    faces[3][2] = v[0];

    DebugDraw.SetDepthTest( true );

    DebugDraw.SetColor( AColor4( 0, 1, 1, 1 ) );
    DebugDraw.DrawLine( origin, v[0] );
    DebugDraw.DrawLine( origin, v[3] );
    DebugDraw.DrawLine( origin, v[1] );
    DebugDraw.DrawLine( origin, v[2] );
    DebugDraw.DrawLine( v, 4, true );

    DebugDraw.SetColor( AColor4( 1, 1, 1, 0.3f ) );
    DebugDraw.DrawTriangles( &faces[0][0], 4, sizeof( Float3 ), false );
    DebugDraw.DrawConvexPoly( v, 4, false );
#endif
#endif
    InWorld->QueryVisiblePrimitives( Primitives, Surfaces, nullptr, query );
}

void ARenderFrontend::AddRenderInstances( AWorld * InWorld )
{
    AScopedTimeCheck TimeCheck( "AddRenderInstances" );

    SRenderView * view = RenderDef.View;
    ADrawable * drawable;
    ATerrainComponent * terrain;
    AAnalyticLightComponent * light;
    AIBLComponent * ibl;
    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();
    ARenderWorld & renderWorld = InWorld->GetRender();

    VisLights.Clear();
    VisIBLs.Clear();

    for ( SPrimitiveDef * primitive : VisPrimitives ) {

        // TODO: Replace upcasting by something better (virtual function?)

        if ( nullptr != (drawable = Upcast< ADrawable >( primitive->Owner )) ) {
            AddDrawable( drawable );
            continue;
        }

        if ( nullptr != (terrain = Upcast< ATerrainComponent >( primitive->Owner )) ) {
            AddTerrain( terrain );
            continue;
        }

        if ( nullptr != (light = Upcast< AAnalyticLightComponent >( primitive->Owner )) ) {
            if ( !light->IsEnabled() ) {
                continue;
            }

            if ( VisLights.Size() < MAX_LIGHTS ) {
                VisLights.Append( light );
            } else {
                GLogger.Printf( "MAX_LIGHTS hit\n" );
            }
            continue;
        }

        if ( nullptr != (ibl = Upcast< AIBLComponent >( primitive->Owner )) ) {
            if ( !ibl->IsEnabled() ) {
                continue;
            }

            if ( VisIBLs.Size() < MAX_PROBES ) {
                VisIBLs.Append( ibl );
            } else {
                GLogger.Printf( "MAX_PROBES hit\n" );
            }
            continue;
        }

        GLogger.Printf( "Unhandled primitive\n" );
    }

    if ( r_RenderSurfaces && !VisSurfaces.IsEmpty() ) {
        struct SSortFunction {
            bool operator() ( SSurfaceDef const * _A, SSurfaceDef const * _B ) {
                return (_A->SortKey < _B->SortKey);
            }
        } SortFunction;

        StdSort( VisSurfaces.ToPtr(), VisSurfaces.ToPtr() + VisSurfaces.Size(), SortFunction );

        AddSurfaces( VisSurfaces.ToPtr(), VisSurfaces.Size() );
    }

    // Add directional lights
    view->NumShadowMapCascades = 0;
    view->NumCascadedShadowMaps = 0;
    for ( ADirectionalLightComponent * dirlight = renderWorld.GetDirectionalLights() ; dirlight ; dirlight = dirlight->GetNext() ) {

        if ( view->NumDirectionalLights >= MAX_DIRECTIONAL_LIGHTS ) {
            GLogger.Printf( "MAX_DIRECTIONAL_LIGHTS hit\n" );
            break;
        }

        if ( !dirlight->IsEnabled() ) {
            continue;
        }

        SDirectionalLightInstance * instance = (SDirectionalLightInstance *)GRuntime->AllocFrameMem( sizeof( SDirectionalLightInstance ) );

        FrameData.DirectionalLights.Append( instance );

        dirlight->AddShadowmapCascades( view, &instance->ViewProjStreamHandle, &instance->FirstCascade, &instance->NumCascades );

        view->NumCascadedShadowMaps += instance->NumCascades > 0 ? 1 : 0;  // Just statistics

        instance->ColorAndAmbientIntensity = dirlight->GetEffectiveColor();
        instance->Matrix = dirlight->GetWorldRotation().ToMatrix();
        instance->MaxShadowCascades = dirlight->GetMaxShadowCascades();
        instance->RenderMask = ~0;//dirlight->RenderingGroup;
        instance->ShadowmapIndex = -1;
        instance->ShadowCascadeResolution = dirlight->GetShadowCascadeResolution();

        view->NumDirectionalLights++;
    }

    LightVoxelizer.Reset();

    // Allocate lights
    view->NumPointLights = VisLights.Size();
    view->PointLightsStreamSize = sizeof( SLightParameters ) * view->NumPointLights;
    view->PointLightsStreamHandle = view->PointLightsStreamSize > 0
            ? streamedMemory->AllocateConstant( view->PointLightsStreamSize, nullptr ) : 0;
    view->PointLights = (SLightParameters *)streamedMemory->Map( view->PointLightsStreamHandle );

    for ( int i = 0 ; i < view->NumPointLights ; i++ ) {
        light = VisLights[i];

        light->PackLight( view->ViewMatrix, view->PointLights[i] );

        AddLightShadowmap( light, view->PointLights[i].Radius, &view->PointLights[i].ShadowmapIndex );

        APhotometricProfile * profile = light->GetPhotometricProfile();
        if ( profile ) {
            profile->WritePhotometricData( PhotometricProfiles, FrameNumber );
        }

        SItemInfo * info = LightVoxelizer.AllocItem();
        info->Type = ITEM_TYPE_LIGHT;
        info->ListIndex = i;

        BvAxisAlignedBox const & AABB = light->GetWorldBounds();
        info->Mins = AABB.Mins;
        info->Maxs = AABB.Maxs;

        if ( LightVoxelizer.IsSSE() ) {
            info->ClipToBoxMatSSE = light->GetOBBTransformInverse() * view->ClusterViewProjectionInversed;
        } else {
            info->ClipToBoxMat = light->GetOBBTransformInverse() * view->ClusterViewProjectionInversed;
        }
    }
    
    // Allocate probes
    view->NumProbes = VisIBLs.Size();
    view->ProbeStreamSize = sizeof( SProbeParameters ) * view->NumProbes;
    view->ProbeStreamHandle = view->ProbeStreamSize > 0 ?
                streamedMemory->AllocateConstant( view->ProbeStreamSize, nullptr ) : 0;
    view->Probes = (SProbeParameters *)streamedMemory->Map( view->ProbeStreamHandle );

    for ( int i = 0 ; i < view->NumProbes ; i++ ) {
        ibl = VisIBLs[i];

        ibl->PackProbe( view->ViewMatrix, view->Probes[i] );

        SItemInfo * info = LightVoxelizer.AllocItem();
        info->Type = ITEM_TYPE_PROBE;
        info->ListIndex = i;

        BvAxisAlignedBox const & AABB = ibl->GetWorldBounds();
        info->Mins = AABB.Mins;
        info->Maxs = AABB.Maxs;

        if ( LightVoxelizer.IsSSE() )  {
            info->ClipToBoxMatSSE = ibl->GetOBBTransformInverse() * view->ClusterViewProjectionInversed;
        } else  {
            info->ClipToBoxMat = ibl->GetOBBTransformInverse() * view->ClusterViewProjectionInversed;
        }
    }

    if ( !r_FixFrustumClusters ) {
        LightVoxelizer.Voxelize( view );
    }
}

void ARenderFrontend::AddDrawable( ADrawable * InComponent ) {
    switch ( InComponent->GetDrawableType() ) {
    case DRAWABLE_STATIC_MESH:
        AddStaticMesh( static_cast< AMeshComponent * >(InComponent) );
        break;
    case DRAWABLE_SKINNED_MESH:
        AddSkinnedMesh( static_cast< ASkinnedComponent * >(InComponent) );
        break;
    case DRAWABLE_PROCEDURAL_MESH:
        AddProceduralMesh( static_cast< AProceduralMeshComponent * >(InComponent) );
        break;
    default:
        break;
    }
}

void ARenderFrontend::AddTerrain( ATerrainComponent * InComponent ) {
    SRenderView * view = RenderDef.View;

    if ( !r_RenderTerrain )
    {
        return;
    }

    ATerrain * terrainResource = InComponent->GetTerrain();
    if ( !terrainResource )
    {
        return;
    }

    ATerrainView * terrainView;

    auto it = ViewRP->TerrainViews.find( terrainResource->Id );
    if ( it == ViewRP->TerrainViews.end() )
    {
        terrainView = new ATerrainView( TerrainTileSize );
        ViewRP->TerrainViews[terrainResource->Id] = terrainView;
    }
    else
    {
        terrainView = it->second;
    }

    // Terrain world rotation
    Float3x3 rotation;
    rotation = InComponent->GetWorldRotation().ToMatrix();

    // Terrain inversed transform
    Float3x4 const & terrainWorldTransformInv = InComponent->GetTerrainWorldTransformInversed();

    // Camera position in terrain space
    Float3 localViewPosition = terrainWorldTransformInv * view->ViewPosition;

    // Camera rotation in terrain space
    Float3x3 localRotation = rotation.Transposed() * view->ViewRotation.ToMatrix();

    Float3x3 basis = localRotation.Transposed();
    Float3 origin = basis * (-localViewPosition);

    Float4x4 localViewMatrix;
    localViewMatrix[0] = Float4( basis[0], 0.0f );
    localViewMatrix[1] = Float4( basis[1], 0.0f );
    localViewMatrix[2] = Float4( basis[2], 0.0f );
    localViewMatrix[3] = Float4( origin, 1.0f );

    Float4x4 localMVP = view->ProjectionMatrix * localViewMatrix;

    BvFrustum localFrustum;
    localFrustum.FromMatrix( localMVP, true );

    // Update resource
    terrainView->SetTerrain( terrainResource );
    // Update view
    terrainView->Update( TerrainMesh, localViewPosition, localFrustum );

    if ( terrainView->GetIndirectBufferDrawCount() == 0 )
    {
        // Everything was culled
        return;
    }

    STerrainRenderInstance * instance = (STerrainRenderInstance *)GRuntime->AllocFrameMem( sizeof( STerrainRenderInstance ) );

    FrameData.TerrainInstances.Append( instance );

    instance->VertexBuffer = TerrainMesh->GetVertexBufferGPU();
    instance->IndexBuffer = TerrainMesh->GetIndexBufferGPU();
    instance->InstanceBufferStreamHandle = terrainView->GetInstanceBufferStreamHandle();
    instance->IndirectBufferStreamHandle = terrainView->GetIndirectBufferStreamHandle();
    instance->IndirectBufferDrawCount = terrainView->GetIndirectBufferDrawCount();
    instance->Clipmaps = terrainView->GetClipmapArray();
    instance->Normals = terrainView->GetNormalMapArray();
    instance->ViewPositionAndHeight.X = localViewPosition.X;
    instance->ViewPositionAndHeight.Y = localViewPosition.Y;
    instance->ViewPositionAndHeight.Z = localViewPosition.Z;
    instance->ViewPositionAndHeight.W = terrainView->GetViewHeight();
    instance->LocalViewProjection = localMVP;
    instance->ModelNormalToViewSpace = view->NormalToViewMatrix * rotation;
    instance->ClipMin = terrainResource->GetClipMin();
    instance->ClipMax = terrainResource->GetClipMax();

    view->TerrainInstanceCount++;
}

void ARenderFrontend::AddStaticMesh( AMeshComponent * InComponent ) {
    AIndexedMesh * mesh = InComponent->GetMesh();

    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    Float3x4 const & componentWorldTransform = InComponent->GetWorldTransformMatrix();

    // TODO: optimize: parallel, sse, check if transformable
    Float4x4 instanceMatrix = RenderDef.View->ViewProjection * componentWorldTransform;
    Float4x4 instanceMatrixP = RenderDef.View->ViewProjectionP * InComponent->RenderTransformMatrix;

    Float3x3 worldRotation = InComponent->GetWorldRotation().ToMatrix();

    InComponent->RenderTransformMatrix = componentWorldTransform;

    ALevel * level = InComponent->GetLevel();

    AIndexedMeshSubpartArray const & subparts = mesh->GetSubparts();

    bool bHasLightmap = InComponent->LightmapUVChannel
            && InComponent->LightmapBlock >= 0
            && InComponent->LightmapBlock < level->Lightmaps.Size();

    for ( int subpartIndex = 0; subpartIndex < subparts.Size(); subpartIndex++ ) {

        AIndexedMeshSubpart * subpart = subparts[subpartIndex];

        AMaterialInstance * materialInstance = InComponent->GetMaterialInstance( subpartIndex );
        AN_ASSERT( materialInstance );

        AMaterial * material = materialInstance->GetMaterial();

        SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

        // Add render instance
        SRenderInstance * instance = (SRenderInstance *)GRuntime->AllocFrameMem( sizeof( SRenderInstance ) );

        if ( material->IsTranslucent() ) {
            FrameData.TranslucentInstances.Append( instance );
            RenderDef.View->TranslucentInstanceCount++;
        }
        else {
            FrameData.Instances.Append( instance );
            RenderDef.View->InstanceCount++;
        }

        if ( InComponent->bOutline ) {
            FrameData.OutlineInstances.Append( instance );
            RenderDef.View->OutlineInstanceCount++;
        }

        instance->Material = material->GetGPUResource();
        instance->MaterialInstance = materialInstanceFrameData;

        mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
        mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );
        mesh->GetWeightsBufferGPU( &instance->WeightsBuffer, &instance->WeightsBufferOffset );

        if ( bHasLightmap ) {
            InComponent->LightmapUVChannel->GetVertexBufferGPU( &instance->LightmapUVChannel, &instance->LightmapUVOffset );
            instance->LightmapOffset = InComponent->LightmapOffset;
            instance->Lightmap = level->Lightmaps[InComponent->LightmapBlock]->GetGPUResource();
        } else {
            instance->LightmapUVChannel = nullptr;
            instance->Lightmap = nullptr;
        }

        if ( InComponent->VertexLightChannel ) {
            InComponent->VertexLightChannel->GetVertexBufferGPU( &instance->VertexLightChannel, &instance->VertexLightOffset );
        } else {
            instance->VertexLightChannel = nullptr;
        }

        instance->IndexCount = subpart->GetIndexCount();
        instance->StartIndexLocation = subpart->GetFirstIndex();
        instance->BaseVertexLocation = subpart->GetBaseVertex() + InComponent->SubpartBaseVertexOffset;
        instance->SkeletonOffset = 0;
        instance->SkeletonOffsetMB = 0;
        instance->SkeletonSize = 0;
        instance->Matrix = instanceMatrix;
        instance->MatrixP = instanceMatrixP;
        instance->ModelNormalToViewSpace = RenderDef.View->NormalToViewMatrix * worldRotation;

        uint8_t priority = material->GetRenderingPriority();
        if ( InComponent->GetMotionBehavior() != MB_STATIC ) {
            priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;
        }

        instance->GenerateSortKey( priority, (uint64_t)mesh );

        RenderDef.PolyCount += instance->IndexCount / 3;
    }
}

void ARenderFrontend::AddSkinnedMesh( ASkinnedComponent * InComponent ) {
    AIndexedMesh * mesh = InComponent->GetMesh();

    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    size_t skeletonOffset = 0;
    size_t skeletonOffsetMB = 0;
    size_t skeletonSize = 0;

    InComponent->GetSkeletonHandle( skeletonOffset, skeletonOffsetMB, skeletonSize );

    Float3x4 const & componentWorldTransform = InComponent->GetWorldTransformMatrix();

    // TODO: optimize: parallel, sse, check if transformable
    Float4x4 instanceMatrix = RenderDef.View->ViewProjection * componentWorldTransform;
    Float4x4 instanceMatrixP = RenderDef.View->ViewProjectionP * InComponent->RenderTransformMatrix;

    Float3x3 worldRotation = InComponent->GetWorldRotation().ToMatrix();

    InComponent->RenderTransformMatrix = componentWorldTransform;

    AIndexedMeshSubpartArray const & subparts = mesh->GetSubparts();

    for ( int subpartIndex = 0; subpartIndex < subparts.Size(); subpartIndex++ ) {

        AIndexedMeshSubpart * subpart = subparts[subpartIndex];

        AMaterialInstance * materialInstance = InComponent->GetMaterialInstance( subpartIndex );
        AN_ASSERT( materialInstance );

        AMaterial * material = materialInstance->GetMaterial();

        SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

        // Add render instance
        SRenderInstance * instance = (SRenderInstance *)GRuntime->AllocFrameMem( sizeof( SRenderInstance ) );

        if ( material->IsTranslucent() ) {
            FrameData.TranslucentInstances.Append( instance );
            RenderDef.View->TranslucentInstanceCount++;
        } else {
            FrameData.Instances.Append( instance );
            RenderDef.View->InstanceCount++;
        }

        if ( InComponent->bOutline ) {
            FrameData.OutlineInstances.Append( instance );
            RenderDef.View->OutlineInstanceCount++;
        }

        instance->Material = material->GetGPUResource();
        instance->MaterialInstance = materialInstanceFrameData;
        //instance->VertexBuffer = mesh->GetVertexBufferGPU();
        //instance->IndexBuffer = mesh->GetIndexBufferGPU();
        //instance->WeightsBuffer = mesh->GetWeightsBufferGPU();

        mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
        mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );
        mesh->GetWeightsBufferGPU( &instance->WeightsBuffer, &instance->WeightsBufferOffset );

        instance->LightmapUVChannel = nullptr;
        instance->Lightmap = nullptr;
        instance->VertexLightChannel = nullptr;
        instance->IndexCount = subpart->GetIndexCount();
        instance->StartIndexLocation = subpart->GetFirstIndex();
        instance->BaseVertexLocation = subpart->GetBaseVertex();
        instance->SkeletonOffset = skeletonOffset;
        instance->SkeletonOffsetMB = skeletonOffsetMB;
        instance->SkeletonSize = skeletonSize;
        instance->Matrix = instanceMatrix;
        instance->MatrixP = instanceMatrixP;
        instance->ModelNormalToViewSpace = RenderDef.View->NormalToViewMatrix * worldRotation;

        uint8_t priority = material->GetRenderingPriority();

        // Skinned meshes are always dynamic
        priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;

        instance->GenerateSortKey( priority, (uint64_t)mesh );

        RenderDef.PolyCount += instance->IndexCount / 3;
    }
}

void ARenderFrontend::AddProceduralMesh( AProceduralMeshComponent * InComponent ) {
    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    AProceduralMesh * mesh = InComponent->GetMesh();
    if ( !mesh ) {
        return;
    }

    mesh->PreRenderUpdate( &RenderDef );

    if ( mesh->IndexCache.IsEmpty() ) {
        return;
    }

    Float3x4 const & componentWorldTransform = InComponent->GetWorldTransformMatrix();

    // TODO: optimize: parallel, sse, check if transformable
    Float4x4 instanceMatrix = RenderDef.View->ViewProjection * componentWorldTransform;
    Float4x4 instanceMatrixP = RenderDef.View->ViewProjectionP * InComponent->RenderTransformMatrix;

    InComponent->RenderTransformMatrix = componentWorldTransform;

    AMaterialInstance * materialInstance = InComponent->GetMaterialInstance();
    AN_ASSERT( materialInstance );

    AMaterial * material = materialInstance->GetMaterial();

    SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

    // Add render instance
    SRenderInstance * instance = (SRenderInstance *)GRuntime->AllocFrameMem( sizeof( SRenderInstance ) );

    if ( material->IsTranslucent() ) {
        FrameData.TranslucentInstances.Append( instance );
        RenderDef.View->TranslucentInstanceCount++;
    }
    else {
        FrameData.Instances.Append( instance );
        RenderDef.View->InstanceCount++;
    }

    if ( InComponent->bOutline ) {
        FrameData.OutlineInstances.Append( instance );
        RenderDef.View->OutlineInstanceCount++;
    }

    instance->Material = material->GetGPUResource();
    instance->MaterialInstance = materialInstanceFrameData;

    mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
    mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );

    instance->WeightsBuffer = nullptr;
    instance->WeightsBufferOffset = 0;
    instance->LightmapUVChannel = nullptr;
    instance->Lightmap = nullptr;
    instance->VertexLightChannel = nullptr;
    instance->IndexCount = mesh->IndexCache.Size();
    instance->StartIndexLocation = 0;
    instance->BaseVertexLocation = 0;
    instance->SkeletonOffset = 0;
    instance->SkeletonOffsetMB = 0;
    instance->SkeletonSize = 0;
    instance->Matrix = instanceMatrix;
    instance->MatrixP = instanceMatrixP;
    instance->ModelNormalToViewSpace = RenderDef.View->NormalToViewMatrix * InComponent->GetWorldRotation().ToMatrix();

    uint8_t priority = material->GetRenderingPriority();
    if ( InComponent->GetMotionBehavior() != MB_STATIC ) {
        priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;
    }

    instance->GenerateSortKey( priority, (uint64_t)mesh );

    RenderDef.PolyCount += instance->IndexCount / 3;
}

void ARenderFrontend::AddShadowmap_StaticMesh( SLightShadowmap * ShadowMap, AMeshComponent * InComponent ) {
    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    AIndexedMesh * mesh = InComponent->GetMesh();

    Float3x4 const & instanceMatrix = InComponent->GetWorldTransformMatrix();

    AIndexedMeshSubpartArray const & subparts = mesh->GetSubparts();

    for ( int subpartIndex = 0; subpartIndex < subparts.Size(); subpartIndex++ ) {

        // FIXME: check subpart bounding box here

        AIndexedMeshSubpart * subpart = subparts[subpartIndex];

        AMaterialInstance * materialInstance = InComponent->GetMaterialInstance( subpartIndex );
        AN_ASSERT( materialInstance );

        AMaterial * material = materialInstance->GetMaterial();

        // Prevent rendering of instances with disabled shadow casting
        if ( !material->CanCastShadow() ) {
            continue;
        }

        SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

        // Add render instance
        SShadowRenderInstance * instance = (SShadowRenderInstance *)GRuntime->AllocFrameMem( sizeof( SShadowRenderInstance ) );

        FrameData.ShadowInstances.Append( instance );

        instance->Material = material->GetGPUResource();
        instance->MaterialInstance = materialInstanceFrameData;

        mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
        mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );
        mesh->GetWeightsBufferGPU( &instance->WeightsBuffer, &instance->WeightsBufferOffset );

        instance->IndexCount = subpart->GetIndexCount();
        instance->StartIndexLocation = subpart->GetFirstIndex();
        instance->BaseVertexLocation = subpart->GetBaseVertex() + InComponent->SubpartBaseVertexOffset;
        instance->SkeletonOffset = 0;
        instance->SkeletonSize = 0;
        instance->WorldTransformMatrix = instanceMatrix;
        instance->CascadeMask = InComponent->CascadeMask;

        uint8_t priority = material->GetRenderingPriority();

        // Dynamic/Static geometry priority is doesn't matter for shadowmap pass
        //if ( InComponent->GetMotionBehavior() != MB_STATIC ) {
        //    priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;
        //}

        instance->GenerateSortKey( priority, (uint64_t)mesh );

        ShadowMap->ShadowInstanceCount++;

        RenderDef.ShadowMapPolyCount += instance->IndexCount / 3;
    }
}

void ARenderFrontend::AddShadowmap_SkinnedMesh( SLightShadowmap * ShadowMap, ASkinnedComponent * InComponent ) {
    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    AIndexedMesh * mesh = InComponent->GetMesh();

    size_t skeletonOffset = 0;
    size_t skeletonOffsetMB = 0;
    size_t skeletonSize = 0;

    InComponent->GetSkeletonHandle( skeletonOffset, skeletonOffsetMB, skeletonSize );

    Float3x4 const & instanceMatrix = InComponent->GetWorldTransformMatrix();

    AIndexedMeshSubpartArray const & subparts = mesh->GetSubparts();

    for ( int subpartIndex = 0; subpartIndex < subparts.Size(); subpartIndex++ ) {

        // FIXME: check subpart bounding box here

        AIndexedMeshSubpart * subpart = subparts[subpartIndex];

        AMaterialInstance * materialInstance = InComponent->GetMaterialInstance( subpartIndex );
        AN_ASSERT( materialInstance );

        AMaterial * material = materialInstance->GetMaterial();

        // Prevent rendering of instances with disabled shadow casting
        if ( !material->CanCastShadow() ) {
            continue;
        }

        SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

        // Add render instance
        SShadowRenderInstance * instance = (SShadowRenderInstance *)GRuntime->AllocFrameMem( sizeof( SShadowRenderInstance ) );

        FrameData.ShadowInstances.Append( instance );

        instance->Material = material->GetGPUResource();
        instance->MaterialInstance = materialInstanceFrameData;

        mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
        mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );
        mesh->GetWeightsBufferGPU( &instance->WeightsBuffer, &instance->WeightsBufferOffset );

        instance->IndexCount = subpart->GetIndexCount();
        instance->StartIndexLocation = subpart->GetFirstIndex();
        instance->BaseVertexLocation = subpart->GetBaseVertex();

        instance->SkeletonOffset = skeletonOffset;
        instance->SkeletonSize = skeletonSize;
        instance->WorldTransformMatrix = instanceMatrix;
        instance->CascadeMask = InComponent->CascadeMask;

        uint8_t priority = material->GetRenderingPriority();

        // Dynamic/Static geometry priority is doesn't matter for shadowmap pass
        //priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;

        instance->GenerateSortKey( priority, (uint64_t)mesh );

        ShadowMap->ShadowInstanceCount++;

        RenderDef.ShadowMapPolyCount += instance->IndexCount / 3;
    }
}

void ARenderFrontend::AddShadowmap_ProceduralMesh( SLightShadowmap * ShadowMap, AProceduralMeshComponent * InComponent ) {
    if ( !r_RenderMeshes ) {
        return;
    }

    InComponent->PreRenderUpdate( &RenderDef );

    AMaterialInstance * materialInstance = InComponent->GetMaterialInstance();
    AN_ASSERT( materialInstance );

    AMaterial * material = materialInstance->GetMaterial();

    // Prevent rendering of instances with disabled shadow casting
    if ( !material->CanCastShadow() ) {
        return;
    }

    AProceduralMesh * mesh = InComponent->GetMesh();
    if ( !mesh ) {
        return;
    }

    mesh->PreRenderUpdate( &RenderDef );

    if ( mesh->IndexCache.IsEmpty() ) {
        return;
    }

    SMaterialFrameData * materialInstanceFrameData = materialInstance->PreRenderUpdate( FrameNumber );

    // Add render instance
    SShadowRenderInstance * instance = (SShadowRenderInstance *)GRuntime->AllocFrameMem( sizeof( SShadowRenderInstance ) );

    FrameData.ShadowInstances.Append( instance );

    instance->Material = material->GetGPUResource();
    instance->MaterialInstance = materialInstanceFrameData;

    mesh->GetVertexBufferGPU( &instance->VertexBuffer, &instance->VertexBufferOffset );
    mesh->GetIndexBufferGPU( &instance->IndexBuffer, &instance->IndexBufferOffset );

    instance->WeightsBuffer = nullptr;
    instance->WeightsBufferOffset = 0;

    instance->IndexCount = mesh->IndexCache.Size();
    instance->StartIndexLocation = 0;
    instance->BaseVertexLocation = 0;
    instance->SkeletonOffset = 0;
    instance->SkeletonSize = 0;
    instance->WorldTransformMatrix = InComponent->GetWorldTransformMatrix();
    instance->CascadeMask = InComponent->CascadeMask;

    uint8_t priority = material->GetRenderingPriority();

    // Dynamic/Static geometry priority is doesn't matter for shadowmap pass
    //if ( InComponent->GetMotionBehavior() != MB_STATIC ) {
    //    priority |= RENDERING_GEOMETRY_PRIORITY_DYNAMIC;
    //}

    instance->GenerateSortKey( priority, (uint64_t)mesh );

    ShadowMap->ShadowInstanceCount++;

    RenderDef.ShadowMapPolyCount += instance->IndexCount / 3;
}

void ARenderFrontend::AddDirectionalShadowmapInstances( AWorld * InWorld ) {
    if ( !RenderDef.View->NumShadowMapCascades ) {
        return;
    }

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    // Create shadow instances

    ShadowCasters.Clear();
    ShadowBoxes.Clear();

    ARenderWorld & renderWorld = InWorld->GetRender();

    for ( ADrawable * component = renderWorld.GetShadowCasters() ; component ; component = component->GetNextShadowCaster() ) {
        if ( (component->GetVisibilityGroup() & RenderDef.VisibilityMask) == 0 ) {
            continue;
        }
        //component->CascadeMask = 0;

        ShadowCasters.Append( component );
        ShadowBoxes.Append( component->GetWorldBounds() );
    }

    ShadowBoxes.Resize( Align( ShadowBoxes.Size(), 4 ) );

    ShadowCasterCullResult.ResizeInvalidate( ShadowBoxes.Size() );

    BvFrustum frustum;

    for ( int lightIndex = 0 ; lightIndex < RenderDef.View->NumDirectionalLights ; lightIndex++ ) {
        int lightOffset = RenderDef.View->FirstDirectionalLight + lightIndex;

        SDirectionalLightInstance * lightDef = FrameData.DirectionalLights[ lightOffset ];

        if ( lightDef->NumCascades == 0 ) {
            continue;
        }

        lightDef->ShadowmapIndex = FrameData.LightShadowmaps.Size();

        SLightShadowmap * shadowMap = &FrameData.LightShadowmaps.Append();

        shadowMap->FirstShadowInstance = FrameData.ShadowInstances.Size();
        shadowMap->ShadowInstanceCount = 0;
        shadowMap->FirstLightPortal = FrameData.LightPortals.Size();
        shadowMap->LightPortalsCount = 0;

        Float4x4 * lightViewProjectionMatrices = (Float4x4 *)streamedMemory->Map( lightDef->ViewProjStreamHandle );

        // Perform culling for each cascade
        // TODO: Do it parallel (jobs)
        for ( int cascadeIndex = 0 ; cascadeIndex < lightDef->NumCascades ; cascadeIndex++ ) {

            frustum.FromMatrix( lightViewProjectionMatrices[cascadeIndex] );

            ShadowCasterCullResult.ZeroMem();

            frustum.CullBox_SSE( ShadowBoxes.ToPtr(), ShadowCasters.Size(), ShadowCasterCullResult.ToPtr() );
            //frustum.CullBox_Generic( ShadowBoxes.ToPtr(), ShadowCasters.Size(), ShadowCasterCullResult.ToPtr() );

            for ( int n = 0 ; n < ShadowCasters.Size() ; n++ ) {
                //GLogger.Printf( "Cull result %f\n", *(float*)&ShadowCasterCullResult[n] );
                //ShadowCasters[n]->CascadeMask |= (1-ShadowCasterCullResult[n]) << i;

                ShadowCasters[n]->CascadeMask |= (ShadowCasterCullResult[n]==0) << cascadeIndex;
            }
        }

        for ( int n = 0 ; n < ShadowCasters.Size() ; n++ ) {
            ADrawable * component = ShadowCasters[n];

            if ( component->CascadeMask == 0 ) {
                continue;
            }

            switch ( component->GetDrawableType() ) {
            case DRAWABLE_STATIC_MESH:
                AddShadowmap_StaticMesh( shadowMap, static_cast< AMeshComponent * >(component) );
                break;
            case DRAWABLE_SKINNED_MESH:
                AddShadowmap_SkinnedMesh( shadowMap, static_cast< ASkinnedComponent * >(component) );
                break;
            case DRAWABLE_PROCEDURAL_MESH:
                AddShadowmap_ProceduralMesh( shadowMap, static_cast< AProceduralMeshComponent * >(component) );
                break;
            default:
                break;
            }

            // Clear cascade mask for next light source
            component->CascadeMask = 0;
        }

        // Add static shadow casters
        for ( ALevel * level : InWorld->GetArrayOfLevels() ) {

            // TODO: Perform culling for each shadow cascade, set CascadeMask

            if ( level->ShadowCasterVerts.IsEmpty() ) {
                continue;
            }

            // Add render instance
            SShadowRenderInstance * instance = (SShadowRenderInstance *)GRuntime->AllocFrameMem( sizeof( SShadowRenderInstance ) );

            FrameData.ShadowInstances.Append( instance );

            instance->Material = nullptr;
            instance->MaterialInstance = nullptr;
            instance->VertexBuffer = level->GetShadowCasterVB();
            instance->VertexBufferOffset = 0;
            instance->IndexBuffer = level->GetShadowCasterIB();
            instance->IndexBufferOffset = 0;
            instance->WeightsBuffer = nullptr;
            instance->WeightsBufferOffset = 0;
            instance->IndexCount = level->ShadowCasterIndices.Size();
            instance->StartIndexLocation = 0;
            instance->BaseVertexLocation = 0;
            instance->SkeletonOffset = 0;
            instance->SkeletonSize = 0;
            instance->WorldTransformMatrix.SetIdentity();
            instance->CascadeMask = 0xffff; // TODO: Calculate!!!
            instance->SortKey = 0;

            shadowMap->ShadowInstanceCount++;

            RenderDef.ShadowMapPolyCount += instance->IndexCount / 3;
        }

        StdSort( FrameData.ShadowInstances.Begin() + shadowMap->FirstShadowInstance,
                 FrameData.ShadowInstances.Begin() + (shadowMap->FirstShadowInstance + shadowMap->ShadowInstanceCount),
                 ShadowInstanceSortFunction );

        if ( r_RenderLightPortals ) {
            // Add light portals
            for ( ALevel * level : InWorld->GetArrayOfLevels() ) {

                TPodVector< SLightPortalDef > const & lightPortals = level->GetLightPortals();

                if ( lightPortals.IsEmpty() ) {
                    continue;
                }

                for ( SLightPortalDef const & lightPortal : lightPortals ) {

                    // TODO: Perform culling for each light portal
                    // NOTE: We can precompute visible geometry for static light and meshes from every light portal

                    SLightPortalRenderInstance * instance = (SLightPortalRenderInstance *)GRuntime->AllocFrameMem( sizeof( SLightPortalRenderInstance ) );

                    FrameData.LightPortals.Append( instance );

                    instance->VertexBuffer = level->GetLightPortalsVB();
                    instance->VertexBufferOffset = 0;
                    instance->IndexBuffer = level->GetLightPortalsIB();
                    instance->IndexBufferOffset = 0;
                    instance->IndexCount = lightPortal.NumIndices;
                    instance->StartIndexLocation = lightPortal.FirstIndex;
                    instance->BaseVertexLocation = 0;

                    shadowMap->LightPortalsCount++;

                    //RenderDef.LightPortalPolyCount += instance->IndexCount / 3;
                }
            }
        }
    }    
}

AN_FORCEINLINE bool CanMergeSurfaces( SSurfaceDef const * InFirst, SSurfaceDef const * InSecond ) {
    return (    InFirst->Model->Id == InSecond->Model->Id
             && InFirst->LightmapBlock == InSecond->LightmapBlock
             && InFirst->MaterialIndex == InSecond->MaterialIndex
             /*&& InFirst->RenderingOrder == InSecond->RenderingOrder*/ );
}

AN_FORCEINLINE bool CanMergeSurfacesShadowmap( SSurfaceDef const * InFirst, SSurfaceDef const * InSecond ) {
    return (    InFirst->Model->Id == InSecond->Model->Id
             && InFirst->MaterialIndex == InSecond->MaterialIndex
             /*&& InFirst->RenderingOrder == InSecond->RenderingOrder*/ );
}

void ARenderFrontend::AddSurfaces( SSurfaceDef * const * Surfaces, int SurfaceCount ) {
    if ( !SurfaceCount ) {
        return;
    }

    int totalVerts = 0;
    int totalIndices = 0;
    for ( int i = 0 ; i < SurfaceCount ; i++ ) {
        SSurfaceDef const * surfDef = Surfaces[i];

        totalVerts += surfDef->NumVertices;
        totalIndices += surfDef->NumIndices;
    }

    if ( totalVerts == 0 || totalIndices < 3 ) {
        // Degenerate surfaces
        return;
    }

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    SurfaceStream.VertexAddr = streamedMemory->AllocateVertex( totalVerts * sizeof( SMeshVertex ), nullptr );
    SurfaceStream.VertexLightAddr = streamedMemory->AllocateVertex( totalVerts * sizeof( SMeshVertexLight ), nullptr );
    SurfaceStream.VertexUVAddr = streamedMemory->AllocateVertex( totalVerts * sizeof( SMeshVertexUV ), nullptr );
    SurfaceStream.IndexAddr = streamedMemory->AllocateIndex( totalIndices * sizeof( unsigned int ), nullptr );

    SMeshVertex * vertices = (SMeshVertex *)streamedMemory->Map( SurfaceStream.VertexAddr );
    SMeshVertexLight * vertexLight = (SMeshVertexLight *)streamedMemory->Map( SurfaceStream.VertexLightAddr );
    SMeshVertexUV * vertexUV = (SMeshVertexUV *)streamedMemory->Map( SurfaceStream.VertexUVAddr );
    unsigned int * indices = (unsigned int *)streamedMemory->Map( SurfaceStream.IndexAddr );

    int numVerts = 0;
    int numIndices = 0;
    int firstIndex = 0;

    SSurfaceDef const * merge = Surfaces[0];
    ABrushModel const * model = merge->Model;

    for ( int i = 0 ; i < SurfaceCount ; i++ ) {
        SSurfaceDef const * surfDef = Surfaces[i];

        if ( !CanMergeSurfaces( merge, surfDef ) ) {

            // Flush merged surfaces
            AddSurface( model->ParentLevel,
                        model->SurfaceMaterials[merge->MaterialIndex],
                        merge->LightmapBlock,
                        numIndices - firstIndex,
                        firstIndex/*,
                        merge->RenderingOrder*/ );

            merge = surfDef;
            model = merge->Model;
            firstIndex = numIndices;
        }

        SMeshVertex const * srcVerts = model->Vertices.ToPtr() + surfDef->FirstVertex;
        SMeshVertexUV const * srcLM = model->LightmapVerts.ToPtr() + surfDef->FirstVertex;
        SMeshVertexLight const * srcVL = model->VertexLight.ToPtr() + surfDef->FirstVertex;
        unsigned int const * srcIndices = model->Indices.ToPtr() + surfDef->FirstIndex;

        // NOTE: Here we can perform CPU transformation for surfaces (modify texCoord, color, or vertex position)

        AN_ASSERT( surfDef->FirstVertex + surfDef->NumVertices <= model->VertexLight.Size() );
        AN_ASSERT( surfDef->FirstIndex + surfDef->NumIndices <= model->Indices.Size() );

        Core::Memcpy( vertices    + numVerts, srcVerts, sizeof( SMeshVertex ) * surfDef->NumVertices );
        Core::Memcpy( vertexUV    + numVerts, srcLM   , sizeof( SMeshVertexUV ) * surfDef->NumVertices );
        Core::Memcpy( vertexLight + numVerts, srcVL   , sizeof( SMeshVertexLight ) * surfDef->NumVertices );

        for ( int ind = 0 ; ind < surfDef->NumIndices ; ind++ ) {
            *indices++ = numVerts + srcIndices[ind];
        }

        numVerts += surfDef->NumVertices;
        numIndices += surfDef->NumIndices;
    }

    // Flush merged surfaces
    AddSurface( model->ParentLevel,
                model->SurfaceMaterials[merge->MaterialIndex],
                merge->LightmapBlock,
                numIndices - firstIndex,
                firstIndex/*,
                merge->RenderingOrder*/ );

    AN_ASSERT( numVerts == totalVerts );
    AN_ASSERT( numIndices == totalIndices );
}

void ARenderFrontend::AddShadowmapSurfaces( SLightShadowmap * ShadowMap, SSurfaceDef * const * Surfaces, int SurfaceCount ) {
    if ( !SurfaceCount ) {
        return;
    }

    int totalVerts = 0;
    int totalIndices = 0;
    for ( int i = 0 ; i < SurfaceCount ; i++ ) {
        SSurfaceDef const * surfDef = Surfaces[i];

        if ( !surfDef->Model->SurfaceMaterials[surfDef->MaterialIndex]->GetMaterial()->CanCastShadow() ) {
            continue;
        }

        totalVerts += surfDef->NumVertices;
        totalIndices += surfDef->NumIndices;
    }

    if ( totalVerts == 0 || totalIndices < 3 ) {
        // Degenerate surfaces
        return;
    }

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    SurfaceStream.VertexAddr = streamedMemory->AllocateVertex( totalVerts * sizeof( SMeshVertex ), nullptr );
    SurfaceStream.IndexAddr = streamedMemory->AllocateIndex( totalIndices * sizeof( unsigned int ), nullptr );

    SMeshVertex * vertices = (SMeshVertex *)streamedMemory->Map( SurfaceStream.VertexAddr );
    unsigned int * indices = (unsigned int *)streamedMemory->Map( SurfaceStream.IndexAddr );

    int numVerts = 0;
    int numIndices = 0;
    int firstIndex = 0;

    SSurfaceDef const * merge = Surfaces[0];
    ABrushModel const * model = merge->Model;

    for ( int i = 0 ; i < SurfaceCount ; i++ ) {
        SSurfaceDef const * surfDef = Surfaces[i];

        if ( !surfDef->Model->SurfaceMaterials[surfDef->MaterialIndex]->GetMaterial()->CanCastShadow() ) {
            continue;
        }

        if ( !CanMergeSurfacesShadowmap( merge, surfDef ) ) {

            // Flush merged surfaces
            AddShadowmapSurface( ShadowMap,
                                 model->SurfaceMaterials[merge->MaterialIndex],
                                 numIndices - firstIndex,
                                 firstIndex/*,
                                 merge->RenderingOrder*/ );

            merge = surfDef;
            model = merge->Model;
            firstIndex = numIndices;
        }

        SMeshVertex const * srcVerts = model->Vertices.ToPtr() + surfDef->FirstVertex;
        unsigned int const * srcIndices = model->Indices.ToPtr() + surfDef->FirstIndex;

#if 0
        DebugDraw.SetDepthTest( false );
        DebugDraw.SetColor( AColor4( 1, 1, 0, 1 ) );
        DebugDraw.DrawTriangleSoupWireframe( &srcVerts->Position, sizeof( SMeshVertex ),
                                             srcIndices, surfDef->NumIndices );
        //DebugDraw.SetColor( AColor4( 0, 1, 0, 1 ) );
        //DebugDraw.DrawAABB( surfDef->Bounds );
#endif

        // NOTE: Here we can perform CPU transformation for surfaces (modify texCoord, color, or vertex position)

        AN_ASSERT( surfDef->FirstVertex + surfDef->NumVertices <= model->Vertices.Size() );
        AN_ASSERT( surfDef->FirstIndex + surfDef->NumIndices <= model->Indices.Size() );

        Core::Memcpy( vertices    + numVerts, srcVerts, sizeof( SMeshVertex ) * surfDef->NumVertices );

        for ( int ind = 0 ; ind < surfDef->NumIndices ; ind++ ) {
            *indices++ = numVerts + srcIndices[ind];
        }

        numVerts += surfDef->NumVertices;
        numIndices += surfDef->NumIndices;
    }

    // Flush merged surfaces
    AddShadowmapSurface( ShadowMap,
                         model->SurfaceMaterials[merge->MaterialIndex],
                         numIndices - firstIndex,
                         firstIndex/*,
                         merge->RenderingOrder*/ );

    AN_ASSERT( numVerts == totalVerts );
    AN_ASSERT( numIndices == totalIndices );
}

void ARenderFrontend::AddSurface( ALevel * Level, AMaterialInstance * MaterialInstance, int _LightmapBlock, int _NumIndices, int _FirstIndex/*, int _RenderingOrder*/ ) {
    AMaterial * material = MaterialInstance->GetMaterial();
    SMaterialFrameData * materialInstanceFrameData = MaterialInstance->PreRenderUpdate( FrameNumber );

    // Add render instance
    SRenderInstance * instance = ( SRenderInstance * )GRuntime->AllocFrameMem( sizeof( SRenderInstance ) );

    if ( material->IsTranslucent() ) {
        FrameData.TranslucentInstances.Append( instance );
        RenderDef.View->TranslucentInstanceCount++;
    } else {
        FrameData.Instances.Append( instance );
        RenderDef.View->InstanceCount++;
    }

    //if ( bOutline ) {
    //    FrameData.OutlineInstances.Append( instance );
    //    RenderDef.View->OutlineInstanceCount++;
    //}

    instance->Material = material->GetGPUResource();
    instance->MaterialInstance = materialInstanceFrameData;

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.VertexAddr, &instance->VertexBuffer, &instance->VertexBufferOffset );
    streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.IndexAddr, &instance->IndexBuffer, &instance->IndexBufferOffset );

    instance->WeightsBuffer = nullptr;

    instance->LightmapOffset.X = 0;
    instance->LightmapOffset.Y = 0;
    instance->LightmapOffset.Z = 1;
    instance->LightmapOffset.W = 1;
    if ( _LightmapBlock >= 0 && _LightmapBlock < Level->Lightmaps.Size() ) {
        instance->Lightmap = Level->Lightmaps[ _LightmapBlock ]->GetGPUResource();

        streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.VertexUVAddr, &instance->LightmapUVChannel, &instance->LightmapUVOffset );
    } else {
        instance->Lightmap = nullptr;
        instance->LightmapUVChannel = nullptr;
    }

    streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.VertexLightAddr, &instance->VertexLightChannel, &instance->VertexLightOffset );

    instance->IndexCount = _NumIndices;
    instance->StartIndexLocation = _FirstIndex;
    instance->BaseVertexLocation = 0;
    instance->SkeletonOffset = 0;
    instance->SkeletonOffsetMB = 0;
    instance->SkeletonSize = 0;
    instance->Matrix = RenderDef.View->ViewProjection;
    instance->MatrixP = RenderDef.View->ViewProjectionP;
    instance->ModelNormalToViewSpace = RenderDef.View->NormalToViewMatrix;

    uint8_t priority = material->GetRenderingPriority();

    instance->GenerateSortKey( priority, SurfaceStream.VertexAddr );

    RenderDef.PolyCount += instance->IndexCount / 3;
}

void ARenderFrontend::AddShadowmapSurface( SLightShadowmap * ShadowMap, AMaterialInstance * MaterialInstance, int _NumIndices, int _FirstIndex/*, int _RenderingOrder*/ ) {
    AMaterial * material = MaterialInstance->GetMaterial();
    SMaterialFrameData * materialInstanceFrameData = MaterialInstance->PreRenderUpdate( FrameNumber );

    // Add render instance
    SShadowRenderInstance * instance = ( SShadowRenderInstance * )GRuntime->AllocFrameMem( sizeof( SShadowRenderInstance ) );

    FrameData.ShadowInstances.Append( instance );

    instance->Material = material->GetGPUResource();
    instance->MaterialInstance = materialInstanceFrameData;

    AStreamedMemoryGPU * streamedMemory = GRuntime->GetStreamedMemoryGPU();

    streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.VertexAddr, &instance->VertexBuffer, &instance->VertexBufferOffset );
    streamedMemory->GetPhysicalBufferAndOffset( SurfaceStream.IndexAddr, &instance->IndexBuffer, &instance->IndexBufferOffset );

    instance->WeightsBuffer = nullptr;
    instance->WeightsBufferOffset = 0;
    instance->WorldTransformMatrix.SetIdentity();
    instance->IndexCount = _NumIndices;
    instance->StartIndexLocation = _FirstIndex;
    instance->BaseVertexLocation = 0;
    instance->SkeletonOffset = 0;
    instance->SkeletonSize = 0;
    instance->CascadeMask = 0xffff; // TODO?

    uint8_t priority = material->GetRenderingPriority();

    instance->GenerateSortKey( priority, SurfaceStream.VertexAddr );

    ShadowMap->ShadowInstanceCount++;

    RenderDef.ShadowMapPolyCount += instance->IndexCount / 3;
}

void ARenderFrontend::AddLightShadowmap( AAnalyticLightComponent * Light, float Radius, int * pShadowmapIndex )
{
    if ( !Light->IsCastShadow() ) {
        *pShadowmapIndex = -1;
        return;
    }

    AWorld * world = Light->GetWorld();

    Float4x4 const * cubeFaceMatrices = Float4x4::GetCubeFaceMatrices();
    Float4x4 projMat = Float4x4::PerspectiveRevCC( Math::_HALF_PI, Math::_HALF_PI, 0.1f, Radius );
    //Float4x4 projMat = Float4x4::PerspectiveCC( Math::_HALF_PI, Math::_HALF_PI, 0.1f, Radius );

    Float4x4 lightViewProjection;
    Float4x4 lightViewMatrix;

    Float3 lightPos = Light->GetWorldPosition();

    ADrawable * drawable;

    int shadowmapIndex = FrameData.LightShadowmaps.Size();

    *pShadowmapIndex = shadowmapIndex;

    int totalInstances = 0;
    int totalSurfaces = 0;

    for ( int faceIndex = 0 ; faceIndex < 6 ; faceIndex++ ) {
        lightViewMatrix = cubeFaceMatrices[faceIndex];
        lightViewMatrix[3] = Float4( Float3x3(lightViewMatrix) * -lightPos, 1.0f );

        lightViewProjection = projMat * lightViewMatrix;

        // TODO: VSD не учитывает FarPlane для кулинга - исправить это
        QueryShadowCasters( world, lightViewProjection, lightPos, Float3x3( cubeFaceMatrices[faceIndex] ), VisPrimitives, VisSurfaces );

        SLightShadowmap * shadowMap = &FrameData.LightShadowmaps.Append();

        shadowMap->FirstShadowInstance = FrameData.ShadowInstances.Size();
        shadowMap->ShadowInstanceCount = 0;
        shadowMap->FirstLightPortal = FrameData.LightPortals.Size();
        shadowMap->LightPortalsCount = 0;

        for ( SPrimitiveDef * primitive : VisPrimitives ) {

            // TODO: Replace upcasting by something better (virtual function?)

            if ( nullptr != (drawable = Upcast< ADrawable >( primitive->Owner )) ) {

                drawable->CascadeMask = 1 << faceIndex;

                switch ( drawable->GetDrawableType() ) {
                case DRAWABLE_STATIC_MESH:
                    AddShadowmap_StaticMesh( shadowMap, static_cast< AMeshComponent * >(drawable) );
                    break;
                case DRAWABLE_SKINNED_MESH:
                    AddShadowmap_SkinnedMesh( shadowMap, static_cast< ASkinnedComponent * >(drawable) );
                    break;
                case DRAWABLE_PROCEDURAL_MESH:
                    AddShadowmap_ProceduralMesh( shadowMap, static_cast< AProceduralMeshComponent * >(drawable) );
                    break;
                default:
                    break;
                }

#if 0
                DebugDraw.SetDepthTest( false );
                DebugDraw.SetColor( AColor4( 0, 1, 0, 1 ) );
                DebugDraw.DrawAABB( drawable->GetWorldBounds() );
#endif

                drawable->CascadeMask = 0;
            }
        }

        if ( r_RenderSurfaces && !VisSurfaces.IsEmpty() ) {
            struct SSortFunction {
                bool operator() ( SSurfaceDef const * _A, SSurfaceDef const * _B ) {
                    return (_A->SortKey < _B->SortKey);
                }
            } SortFunction;

            StdSort( VisSurfaces.ToPtr(), VisSurfaces.ToPtr() + VisSurfaces.Size(), SortFunction );

            AddShadowmapSurfaces( shadowMap, VisSurfaces.ToPtr(), VisSurfaces.Size() );

            totalSurfaces += VisSurfaces.Size();
        }

        StdSort( FrameData.ShadowInstances.Begin() + shadowMap->FirstShadowInstance,
                 FrameData.ShadowInstances.Begin() + (shadowMap->FirstShadowInstance + shadowMap->ShadowInstanceCount),
                 ShadowInstanceSortFunction );

        totalInstances += shadowMap->ShadowInstanceCount;
    }

    //GLogger.Printf( "Total Instances %d, surfaces %d\n", totalInstances, totalSurfaces );
}
