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

void CreateDepthPassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _Skinned );

void CreateWireframePassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _Skinned );

void CreateNormalsPassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _Skinned );

void CreateHUDPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode );

void CreateLightPassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _Skinned, bool _DepthTest, bool _Translucent, EColorBlending _Blending );

void CreateLightPassLightmapPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _DepthTest, bool _Translucent, EColorBlending _Blending );

void CreateLightPassVertexLightPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, RenderCore::POLYGON_CULL _CullMode, bool _DepthTest, bool _Translucent, EColorBlending _Blending );

void CreateShadowMapPassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, bool _ShadowMasking, bool _Skinned );

void CreateFeedbackPassPipeline( TRef< RenderCore::IPipeline > * ppPipeline, const char * _SourceCode, bool _Skinned );

struct AShadeModelLit
{
    TRef< RenderCore::IPipeline > DepthPass;
    TRef< RenderCore::IPipeline > DepthPassSkinned;
    TRef< RenderCore::IPipeline > WireframePass;
    TRef< RenderCore::IPipeline > WireframePassSkinned;
    TRef< RenderCore::IPipeline > NormalsPass;
    TRef< RenderCore::IPipeline > NormalsPassSkinned;
    TRef< RenderCore::IPipeline > LightPassSimple;
    TRef< RenderCore::IPipeline > LightPassSkinned;
    TRef< RenderCore::IPipeline > LightPassLightmap;
    TRef< RenderCore::IPipeline > LightPassVertexLight;
    TRef< RenderCore::IPipeline > ShadowPass;
    TRef< RenderCore::IPipeline > ShadowPassSkinned;
    TRef< RenderCore::IPipeline > FeedbackPass;
    TRef< RenderCore::IPipeline > FeedbackPassSkinned;
};

struct AShadeModelUnlit
{
    TRef< RenderCore::IPipeline > DepthPass;
    TRef< RenderCore::IPipeline > DepthPassSkinned;
    TRef< RenderCore::IPipeline > WireframePass;
    TRef< RenderCore::IPipeline > WireframePassSkinned;
    TRef< RenderCore::IPipeline > NormalsPass;
    TRef< RenderCore::IPipeline > NormalsPassSkinned;
    TRef< RenderCore::IPipeline > LightPassSimple;
    TRef< RenderCore::IPipeline > LightPassSkinned;
    TRef< RenderCore::IPipeline > ShadowPass;
    TRef< RenderCore::IPipeline > ShadowPassSkinned;
    TRef< RenderCore::IPipeline > FeedbackPass;
    TRef< RenderCore::IPipeline > FeedbackPassSkinned;
};

struct AShadeModelHUD
{
    TRef< RenderCore::IPipeline > HUDPipeline;
};