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

#include "RenderCommon.h"

class ALightRenderer {
public:
    ALightRenderer();

    void AddPass( AFrameGraph & FrameGraph,
                  AFrameGraphTexture * DepthTarget,
                  AFrameGraphTexture * SSAOTexture,
                  AFrameGraphTexture * ShadowMapDepth0,
                  AFrameGraphTexture * ShadowMapDepth1,
                  AFrameGraphTexture * ShadowMapDepth2,
                  AFrameGraphTexture * ShadowMapDepth3,
                  AFrameGraphTexture * LinearDepth,
                  AFrameGraphTexture ** ppLight/*,
                  AFrameGraphTexture ** ppVelocity*/ );

private:
    void CreateLookupBRDF();

    bool BindMaterialLightPass( SRenderInstance const * Instance );
    void BindTexturesLightPass( SMaterialFrameData * _Instance );

    RenderCore::Sampler LightmapSampler;
    RenderCore::Sampler ReflectDepthSampler;
    RenderCore::Sampler ReflectSampler;
    RenderCore::Sampler VirtualTextureSampler;
    RenderCore::Sampler VirtualTextureIndirectionSampler;

    RenderCore::Sampler ShadowDepthSamplerPCF;
    RenderCore::Sampler ShadowDepthSamplerVSM;
    RenderCore::Sampler ShadowDepthSamplerEVSM;
    RenderCore::Sampler ShadowDepthSamplerPCSS0;
    RenderCore::Sampler ShadowDepthSamplerPCSS1;

    RenderCore::Sampler IESSampler;

    TRef< RenderCore::ITexture > LookupBRDF;
    RenderCore::Sampler LookupBRDFSampler;

    RenderCore::Sampler SSAOSampler;

    RenderCore::Sampler ClusterLookupSampler;
};
