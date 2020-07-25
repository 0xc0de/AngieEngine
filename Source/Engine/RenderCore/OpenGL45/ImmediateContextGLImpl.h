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

#pragma once

#include "DeviceGLImpl.h"
#include "FramebufferGLImpl.h"

namespace RenderCore {

class ABufferGLImpl;
class ATextureGLImpl;
class ARenderPassGLImpl;
class APipelineGLImpl;

class AImmediateContextGLImpl final : public IImmediateContext {

    friend class ADeviceGLImpl;
    friend class AFramebufferGLImpl;
    friend class APipelineGLImpl;
    friend class ARenderPassGLImpl;
    friend class ATextureGLImpl;

public:
    void MakeCurrent() override;

    static AImmediateContextGLImpl * GetCurrent() { return static_cast< AImmediateContextGLImpl * >( Current ); }

    void SetSwapChainResolution( int _Width, int _Height ) override;

    CLIP_CONTROL GetClipControl() const override { return ClipControl; }

    VIEWPORT_ORIGIN GetViewportOrigin() const override { return ViewportOrigin; }

    IFramebuffer * GetDefaultFramebuffer() override { return DefaultFramebuffer; }

    IDevice * GetDevice() override { return pDevice; }


    //
    // Pipeline
    //

    void BindPipeline( IPipeline * _Pipeline, int _Subpass = 0 ) override;

    //
    // Vertex & Index buffers
    //

    void BindVertexBuffer( unsigned int _InputSlot, /* optional */ IBuffer const * _VertexBuffer, unsigned int _Offset = 0 ) override;

    void BindVertexBuffers( unsigned int _StartSlot, unsigned int _NumBuffers, /* optional */ IBuffer * const * _VertexBuffers, uint32_t const * _Offsets = nullptr ) override;

    void BindIndexBuffer( IBuffer const * _IndexBuffer, INDEX_TYPE _Type, unsigned int _Offset = 0 ) override;

    //
    // Shader resources
    //

    void BindShaderResources( SShaderResources const * _Resources ) override;

    //
    // Viewport
    //

    void SetViewport( SViewport const & _Viewport ) override;

    void SetViewportArray( uint32_t _NumViewports, SViewport const * _Viewports ) override;

    void SetViewportArray( uint32_t _FirstIndex, uint32_t _NumViewports, SViewport const * _Viewports ) override;

    void SetViewportIndexed( uint32_t _Index, SViewport const & _Viewport ) override;

    //
    // Scissor
    //

    void SetScissor( /* optional */ SRect2D const & _Scissor ) override;

    void SetScissorArray( uint32_t _NumScissors, SRect2D const * _Scissors ) override;

    void SetScissorArray( uint32_t _FirstIndex, uint32_t _NumScissors, SRect2D const * _Scissors ) override;

    void SetScissorIndexed( uint32_t _Index, SRect2D const & _Scissor ) override;

    //
    // Render pass
    //

    void BeginRenderPass( SRenderPassBegin const & _RenderPassBegin ) override;

    void EndRenderPass() override;

    //
    // Transform feedback
    //

    void BindTransformFeedback( ITransformFeedback * _TransformFeedback ) override;

    void BeginTransformFeedback( PRIMITIVE_TOPOLOGY _OutputPrimitive ) override;

    void ResumeTransformFeedback() override;

    void PauseTransformFeedback() override;

    void EndTransformFeedback() override;

    //
    // Draw
    //

    void Draw( SDrawCmd const * _Cmd ) override;

    void Draw( SDrawIndexedCmd const * _Cmd ) override;

    void Draw( ITransformFeedback * _TransformFeedback, unsigned int _InstanceCount = 1, unsigned int _StreamIndex = 0 ) override;

    void DrawIndirect( SDrawIndirectCmd const * _Cmd ) override;

    void DrawIndirect( SDrawIndexedIndirectCmd const * _Cmd ) override;

    void DrawIndirect( IBuffer * _DrawIndirectBuffer, unsigned int _AlignedByteOffset, bool _Indexed ) override;

    void MultiDraw( unsigned int _DrawCount, const unsigned int * _VertexCount, const unsigned int * _StartVertexLocations ) override;

    void MultiDraw( unsigned int _DrawCount, const unsigned int * _IndexCount, const void * const * _IndexByteOffsets, const int * _BaseVertexLocations = nullptr ) override;

    void MultiDrawIndirect( unsigned int _DrawCount, SDrawIndirectCmd const * _Cmds, unsigned int _Stride ) override;

    void MultiDrawIndirect( unsigned int _DrawCount, SDrawIndexedIndirectCmd const * _Cmds, unsigned int _Stride ) override;

    //
    // Dispatch compute
    //

    void DispatchCompute( unsigned int _ThreadGroupCountX,
                          unsigned int _ThreadGroupCountY,
                          unsigned int _ThreadGroupCountZ ) override;

    void DispatchCompute( SDispatchIndirectCmd const * _Cmd ) override;

    void DispatchComputeIndirect( IBuffer * _DispatchIndirectBuffer, unsigned int _AlignedByteOffset ) override;

    //
    // Query
    //

    void BeginQuery( IQueryPool * _QueryPool, uint32_t _QueryID, uint32_t _StreamIndex = 0 ) override;

    void EndQuery( IQueryPool * _QueryPool, uint32_t _StreamIndex = 0 ) override;

    void CopyQueryPoolResultsAvailable( IQueryPool * _QueryPool,
                                        uint32_t _FirstQuery,
                                        uint32_t _QueryCount,
                                        IBuffer * _DstBuffer,
                                        size_t _DstOffst,
                                        size_t _DstStride,
                                        bool _QueryResult64Bit ) override;

    void CopyQueryPoolResults( IQueryPool * _QueryPool,
                               uint32_t _FirstQuery,
                               uint32_t _QueryCount,
                               IBuffer * _DstBuffer,
                               size_t _DstOffst,
                               size_t _DstStride,
                               QUERY_RESULT_FLAGS _Flags ) override;

    //
    // Conditional render
    //

    void BeginConditionalRender( IQueryPool * _QueryPool, uint32_t _QueryID, CONDITIONAL_RENDER_MODE _Mode ) override;

    void EndConditionalRender() override;

    //
    // Synchronization
    //

    SyncObject FenceSync() override;

    void RemoveSync( SyncObject _Sync ) override;

    CLIENT_WAIT_STATUS ClientWait( SyncObject _Sync, /* optional */ uint64_t _TimeOutNanoseconds = 0xFFFFFFFFFFFFFFFF ) override;

    void ServerWait( SyncObject _Sync ) override;

    bool IsSignaled( SyncObject _Sync ) override;

    void Flush() override;

    void Barrier( int _BarrierBits ) override;

    void BarrierByRegion( int _BarrierBits ) override;

    void TextureBarrier() override;

    //
    // DYNAMIC STATE
    //

    void DynamicState_BlendingColor( /* optional */ const float _ConstantColor[4] ) override;

    void DynamicState_SampleMask( /* optional */ const uint32_t _SampleMask[4] ) override;

    void DynamicState_StencilRef( uint32_t _StencilRef ) override;

    void SetLineWidth( float _Width ) override;

    //
    // Copy
    //

    void CopyBuffer( IBuffer * _SrcBuffer, IBuffer * _DstBuffer ) override;

    void CopyBufferRange( IBuffer * _SrcBuffer, IBuffer * _DstBuffer, uint32_t _NumRanges, SBufferCopy const * _Ranges ) override;

    bool CopyBufferToTexture( IBuffer const * _SrcBuffer,
                              ITexture * _DstTexture,
                              STextureRect const & _Rectangle,
                              DATA_FORMAT _Format,
                              size_t _CompressedDataByteLength,
                              size_t _SourceByteOffset,
                              unsigned int _Alignment
                              ) override;

    void CopyTextureToBuffer( ITexture const * _SrcTexture,
                              IBuffer * _DstBuffer,
                              STextureRect const & _Rectangle,
                              DATA_FORMAT _Format,
                              size_t _SizeInBytes,
                              size_t _DstByteOffset,
                              unsigned int _Alignment
                              ) override;

    void CopyTextureRect( ITexture const * _SrcTexture,
                          ITexture * _DstTexture,
                          uint32_t _NumCopies,
                          TextureCopy const * Copies ) override;

    bool CopyFramebufferToTexture( IFramebuffer const * _SrcFramebuffer,
                                   ITexture * _DstTexture,
                                   FRAMEBUFFER_ATTACHMENT _Attachment,
                                   STextureOffset const & _Offset,
                                   SRect2D const & _SrcRect,
                                   unsigned int _Alignment ) override;

    void CopyFramebufferToBuffer( IFramebuffer const * _SrcFramebuffer,
                                  IBuffer * _DstBuffer,
                                  FRAMEBUFFER_ATTACHMENT _Attachment,
                                  SRect2D const & _SrcRect,
                                  FRAMEBUFFER_CHANNEL _FramebufferChannel,
                                  FRAMEBUFFER_OUTPUT _FramebufferOutput,
                                  COLOR_CLAMP _ColorClamp,
                                  size_t _SizeInBytes,
                                  size_t _DstByteOffset,
                                  unsigned int _Alignment ) override;


    bool BlitFramebuffer( IFramebuffer const * _SrcFramebuffer,
                          FRAMEBUFFER_ATTACHMENT _SrcAttachment,
                          uint32_t _NumRectangles,
                          SBlitRectangle const * _Rectangles,
                          FRAMEBUFFER_MASK _Mask,
                          bool _LinearFilter ) override;

    //
    // Clear
    //

    void ClearBuffer( IBuffer * _Buffer, BUFFER_VIEW_PIXEL_FORMAT _InternalFormat, DATA_FORMAT _Format, const SClearValue * _ClearValue ) override;

    void ClearBufferRange( IBuffer * _Buffer, BUFFER_VIEW_PIXEL_FORMAT _InternalFormat, uint32_t _NumRanges, SBufferClear const * _Ranges, DATA_FORMAT _Format, const SClearValue * _ClearValue ) override;

    void ClearTexture( ITexture * _Texture, uint16_t _Lod, DATA_FORMAT _Format, const SClearValue * _ClearValue ) override;

    void ClearTextureRect( ITexture * _Texture,
                           uint32_t _NumRectangles,
                           STextureRect const * _Rectangles,
                           DATA_FORMAT _Format,
                           const SClearValue * _ClearValue ) override;

    void ClearFramebufferAttachments( IFramebuffer * _Framebuffer,
                                      /* optional */ unsigned int * _ColorAttachments,
                                      /* optional */ unsigned int _NumColorAttachments,
                                      /* optional */ SClearColorValue const * _ColorClearValues,
                                      /* optional */ SClearDepthStencilValue const * _DepthStencilClearValue,
                                      /* optional */ SRect2D const * _Rect ) override;

private:
    AImmediateContextGLImpl( ADeviceGLImpl * _Device, SImmediateContextCreateInfo const & _CreateInfo, void * _Context );
    ~AImmediateContextGLImpl();

    void PolygonOffsetClampSafe( float _Slope, int _Bias, float _Clamp );

    void PackAlignment( unsigned int _Alignment );

    void UnpackAlignment( unsigned int _Alignment );

    void ClampReadColor( COLOR_CLAMP _ColorClamp );

    struct SVertexArrayObject * CachedVAO( SVertexBindingInfo const * pVertexBindings,
                                           uint32_t NumVertexBindings,
                                           SVertexAttribInfo const * pVertexAttribs,
                                           uint32_t NumVertexAttribs );

    void BindRenderPassSubPass( ARenderPassGLImpl const * _RenderPass, int _Subpass );

    void BeginRenderPassDefaultFramebuffer( SRenderPassBegin const & _RenderPassBegin );

    bool CopyBufferToTexture1D( ABufferGLImpl const * _SrcBuffer,
                                ATextureGLImpl * _DstTexture,
                                uint16_t _Lod,
                                uint16_t _OffsetX,
                                uint16_t _DimensionX,
                                size_t _CompressedDataByteLength, // Only for compressed images
                                DATA_FORMAT _Format,
                                size_t _SourceByteOffset,
                                unsigned int _Alignment );

    bool CopyBufferToTexture2D( ABufferGLImpl const * _SrcBuffer,
                                ATextureGLImpl * _DstTexture,
                                uint16_t _Lod,
                                uint16_t _OffsetX,
                                uint16_t _OffsetY,
                                uint16_t _DimensionX,
                                uint16_t _DimensionY,
                                uint16_t _CubeFaceIndex, // only for TEXTURE_CUBE_MAP
                                uint16_t _NumCubeFaces, // only for TEXTURE_CUBE_MAP
                                size_t _CompressedDataByteLength, // Only for compressed images
                                DATA_FORMAT _Format,
                                size_t _SourceByteOffset,
                                unsigned int _Alignment );

    bool CopyBufferToTexture3D( ABufferGLImpl const * _SrcBuffer,
                                ATextureGLImpl * _DstTexture,
                                uint16_t _Lod,
                                uint16_t _OffsetX,
                                uint16_t _OffsetY,
                                uint16_t _OffsetZ,
                                uint16_t _DimensionX,
                                uint16_t _DimensionY,
                                uint16_t _DimensionZ,
                                size_t _CompressedDataByteLength, // Only for compressed images
                                DATA_FORMAT _Format,
                                size_t _SourceByteOffset,
                                unsigned int _Alignment );

    ADeviceGLImpl *           pDevice;
    struct SDL_Window *       pWindow;
    void *                    pContextGL;

    TRef< AFramebufferGLImpl >DefaultFramebuffer;

    CLIP_CONTROL              ClipControl;
    VIEWPORT_ORIGIN           ViewportOrigin;

    unsigned int *            TmpHandles;
    ptrdiff_t *               TmpPointers;
    ptrdiff_t *               TmpPointers2;

    unsigned int              BufferBindings[MAX_BUFFER_SLOTS];
    unsigned int              SampleBindings[MAX_SAMPLER_SLOTS];
    unsigned int              TextureBindings[MAX_SAMPLER_SLOTS];

    APipelineGLImpl *         CurrentPipeline;
    struct SVertexArrayObject*CurrentVAO;
    uint8_t                   NumPatchVertices;       // count of patch vertices to set by glPatchParameteri

    struct {
        unsigned int          PackAlignment;
        unsigned int          UnpackAlignment;
    } PixelStore;

    // current binding state
    struct {
        unsigned int          ReadFramebuffer;
        unsigned int          DrawFramebuffer;
        unsigned short        DrawFramebufferWidth;
        unsigned short        DrawFramebufferHeight;
        unsigned int          DrawInderectBuffer;
        unsigned int          DispatchIndirectBuffer;
        SBlendingStateInfo const *     BlendState;             // current blend state binding
        SRasterizerStateInfo const *   RasterizerState;        // current rasterizer state binding
        SDepthStencilStateInfo const * DepthStencilState;      // current depth-stencil state binding
    } Binding;

    unsigned int              BufferBinding[ 2 ];

    COLOR_CLAMP               ColorClamp;

    SBlendingStateInfo        BlendState;             // current blend state
    float                     BlendColor[4];
    unsigned int              SampleMask[4];
    bool                      bSampleMaskEnabled;
    bool                      bLogicOpEnabled;

    SRasterizerStateInfo      RasterizerState;       // current rasterizer state
    bool                      bPolygonOffsetEnabled;
    unsigned int              CullFace;

    SDepthStencilStateInfo    DepthStencilState;     // current depth-stencil state
    unsigned int              StencilRef;

    ARenderPassGLImpl const * CurrentRenderPass;
    int                       CurrentSubpass;
    SRect2D                   CurrentRenderPassRenderArea;

    float                     CurrentViewport[4];
    float                     CurrentDepthRange[2];

    SRect2D                   CurrentScissor;

    bool                      bPrimitiveRestartEnabled;

    int                       SwapChainWidth;
    int                       SwapChainHeight;

    THash<>                   VAOHash;
    TPodArray< struct SVertexArrayObject * > VAOCache;

    AImmediateContextGLImpl * Next;
    AImmediateContextGLImpl * Prev;
    static AImmediateContextGLImpl * StateHead;
    static AImmediateContextGLImpl * StateTail;
};

}
