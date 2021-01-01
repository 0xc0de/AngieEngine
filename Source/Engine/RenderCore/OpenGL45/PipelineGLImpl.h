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

#pragma once

#include <RenderCore/Pipeline.h>

namespace RenderCore {

class ADeviceGLImpl;

struct SImageInfoGL
{
    unsigned int AccessMode;
    unsigned int InternalFormat;
};

struct SBufferInfoGL
{
    unsigned int BufferType;
};

class APipelineGLImpl final : public IPipeline
{
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
    unsigned int * SamplerObjects;
    int          NumSamplerObjects;
    SImageInfoGL * Images;
    int          NumImages;
    SBufferInfoGL * Buffers;
    int          NumBuffers;
    unsigned int PrimitiveTopology;
    int          NumPatchVertices;
    bool         bPrimitiveRestartEnabled;
    TRef< IShaderModule > pVS;
    TRef< IShaderModule > pTCS;
    TRef< IShaderModule > pTES;
    TRef< IShaderModule > pGS;
    TRef< IShaderModule > pFS;
    TRef< IShaderModule > pCS;
};

}
