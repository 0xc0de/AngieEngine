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

#include <RenderCore/Pipeline.h>

namespace RenderCore {

class ADeviceGLImpl;

class APipelineGLImpl final : public IPipeline
{
    friend class ADeviceGLImpl;
    friend class AImmediateContextGLImpl;

public:
    APipelineGLImpl( ADeviceGLImpl * _Device, SPipelineCreateInfo const & _CreateInfo );
    ~APipelineGLImpl();

private:
    ADeviceGLImpl * pDevice;
    struct SVertexArrayObject * VAO;
    SBlendingStateInfo const * BlendingState;
    SRasterizerStateInfo const * RasterizerState;
    SDepthStencilStateInfo const * DepthStencilState;
    unsigned int IndexBufferType;        // type of current binded index buffer (uin16 or uint32_t)
    size_t       IndexBufferTypeSizeOf;  // size of one index
    unsigned int IndexBufferOffset;      // offset in current binded index buffer
    unsigned int PrimitiveTopology;
    int          NumPatchVertices;
    bool         bPrimitiveRestartEnabled;
    TRef< IShaderModule > pVS;
    TRef< IShaderModule > pGS;
    TRef< IShaderModule > pTCS;
    TRef< IShaderModule > pTES;
    TRef< IShaderModule > pFS;
    TRef< IShaderModule > pCS;
};

}
