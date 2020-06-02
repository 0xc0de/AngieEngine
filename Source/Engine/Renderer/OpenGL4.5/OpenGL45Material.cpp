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

#include "OpenGL45Material.h"

using namespace GHI;

namespace OpenGL45 {

void ADepthPass::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _Skinned ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    //bsd.RenderTargetSlots[0].ColorWriteMask = COLOR_WRITE_DISABLED;

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();
    dssd.DepthFunc = CMPFUNC_GEQUAL;//CMPFUNC_GREATER;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexSkin );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = _Skinned ? 2 : 1;
    pipelineCI.pVertexBindings = vertexBinding;

    static const VertexAttribInfo vertexAttribsSkinned[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        },
        {
            "InJointIndices",
            4,              // location
            1,              // buffer input slot
            VAT_UBYTE4,
            VAM_INTEGER,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexSkin, JointIndices )
        },
        {
            "InJointWeights",
            5,              // location
            1,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexSkin, JointWeights )
        }
    };

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        }
    };

    if ( _Skinned ) {
        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribsSkinned );
        pipelineCI.pVertexAttribs = vertexAttribsSkinned;
    } else {
        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    }

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_DEPTH\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo stages[] = { vs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    Initialize( pipelineCI );
}

void AWireframePass::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _Skinned ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexSkin );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = _Skinned ? 2 : 1;
    pipelineCI.pVertexBindings = vertexBinding;

    if ( _Skinned ) {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            },
            {
                "InJointIndices",
                4,              // location
                1,              // buffer input slot
                VAT_UBYTE4,
                VAM_INTEGER,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointIndices )
            },
            {
                "InJointWeights",
                5,              // location
                1,              // buffer input slot
                VAT_UBYTE4N,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointWeights )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    } else {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    }

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule, geometryShaderModule, fragmentShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_WIREFRAME\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_WIREFRAME\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( GEOMETRY_SHADER, &geometryShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_WIREFRAME\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo gs = {};
    gs.Stage = SHADER_STAGE_GEOMETRY_BIT;
    gs.pModule = &geometryShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, gs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    Initialize( pipelineCI );
}

void ANormalsPass::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _Skinned ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexSkin );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = _Skinned ? 2 : 1;
    pipelineCI.pVertexBindings = vertexBinding;

    if ( _Skinned ) {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            },
            {
                "InJointIndices",
                4,              // location
                1,              // buffer input slot
                VAT_UBYTE4,
                VAM_INTEGER,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointIndices )
            },
            {
                "InJointWeights",
                5,              // location
                1,              // buffer input slot
                VAT_UBYTE4N,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointWeights )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    } else {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    }

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule, geometryShaderModule, fragmentShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_NORMALS\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_NORMALS\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( GEOMETRY_SHADER, &geometryShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_NORMALS\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_POINTS;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo gs = {};
    gs.Stage = SHADER_STAGE_GEOMETRY_BIT;
    gs.pModule = &geometryShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, gs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    Initialize( pipelineCI );
}

void AColorPassHUD::Create( const char * _SourceCode ) {
    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_DISABLED;
    rsd.bScissorEnable = true;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    bsd.RenderTargetSlots[0].SetBlendingPreset( BLENDING_ALPHA );

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

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, fs };

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

    Initialize( pipelineCI );
}

static GHI::BLENDING_PRESET GetBlendingPreset( EColorBlending _Blending ) {
    switch ( _Blending ) {
    case COLOR_BLENDING_ALPHA:
        return GHI::BLENDING_ALPHA;
    case COLOR_BLENDING_DISABLED:
        return GHI::BLENDING_NO_BLEND;
    case COLOR_BLENDING_PREMULTIPLIED_ALPHA:
        return GHI::BLENDING_PREMULTIPLIED_ALPHA;
    case COLOR_BLENDING_COLOR_ADD:
        return GHI::BLENDING_COLOR_ADD;
    case COLOR_BLENDING_MULTIPLY:
        return GHI::BLENDING_MULTIPLY;
    case COLOR_BLENDING_SOURCE_TO_DEST:
        return GHI::BLENDING_SOURCE_TO_DEST;
    case COLOR_BLENDING_ADD_MUL:
        return GHI::BLENDING_ADD_MUL;
    case COLOR_BLENDING_ADD_ALPHA:
        return GHI::BLENDING_ADD_ALPHA;
    default:
        AN_ASSERT( 0 );
    }
    return GHI::BLENDING_NO_BLEND;
}

void AColorPass::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _Skinned, bool _DepthTest, bool _Translucent, EColorBlending _Blending ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    if ( _Translucent ) {        
        bsd.RenderTargetSlots[0].SetBlendingPreset( GetBlendingPreset( _Blending ) );
    }

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

#ifdef DEPTH_PREPASS
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;
    if ( _Translucent ) {
        dssd.DepthFunc = CMPFUNC_GREATER;
    } else {
        dssd.DepthFunc = CMPFUNC_EQUAL;
    }
#else
    dssd.DepthFunc = CMPFUNC_GREATER;
#endif
    dssd.bDepthEnable = _DepthTest;

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderModule vertexShaderModule, fragmentShaderModule;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexSkin );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    if ( _Skinned ) {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            },
            {
                "InJointIndices",
                4,              // location
                1,              // buffer input slot
                VAT_UBYTE4,
                VAM_INTEGER,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointIndices )
            },
            {
                "InJointWeights",
                5,              // location
                1,              // buffer input slot
                VAT_UBYTE4N,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertexSkin, JointWeights )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;

        AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

        GShaderSources.Clear();
        GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
        GShaderSources.Add( "#define SKINNED_MESH\n" );
        GShaderSources.Add( vertexAttribsShaderString.CStr() );
        GShaderSources.Add( _SourceCode );
        GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

        GShaderSources.Clear();
        GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
        GShaderSources.Add( "#define SKINNED_MESH\n" );
        GShaderSources.Add( _SourceCode );
        GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

        pipelineCI.NumVertexBindings = 2;
    } else {
        static const VertexAttribInfo vertexAttribs[] = {
            {
                "InPosition",
                0,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Position )
            },
            {
                "InTexCoord",
                1,              // location
                0,              // buffer input slot
                VAT_FLOAT2,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, TexCoord )
            },
            {
                "InTangent",
                2,              // location
                0,              // buffer input slot
                VAT_FLOAT4,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Tangent )
            },
            {
                "InNormal",
                3,              // location
                0,              // buffer input slot
                VAT_FLOAT3,
                VAM_FLOAT,
                0,              // InstanceDataStepRate
                GHI_STRUCT_OFS( SMeshVertex, Normal )
            }
        };

        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;

        AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

        GShaderSources.Clear();
        GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
        GShaderSources.Add( vertexAttribsShaderString.CStr() );
        GShaderSources.Add( _SourceCode );
        GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

        GShaderSources.Clear();
        GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
        GShaderSources.Add( _SourceCode );
        GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

        pipelineCI.NumVertexBindings = 1;
    }

    pipelineCI.pVertexBindings = vertexBinding;

    Initialize( pipelineCI );
}

void AColorPassLightmap::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _DepthTest, bool _Translucent, EColorBlending _Blending ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    if ( _Translucent ) {
        bsd.RenderTargetSlots[0].SetBlendingPreset( GetBlendingPreset( _Blending ) );
    }

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

#ifdef DEPTH_PREPASS
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;
    if ( _Translucent ) {
        dssd.DepthFunc = CMPFUNC_GREATER;
    } else {
        dssd.DepthFunc = CMPFUNC_EQUAL;
    }
#else
    dssd.DepthFunc = CMPFUNC_GREATER;
#endif
    dssd.bDepthEnable = _DepthTest;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        },
        {
            "InLightmapTexCoord",
            4,              // location
            1,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexUV, TexCoord )
        }
    };

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( "#define USE_LIGHTMAP\n" );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( "#define USE_LIGHTMAP\n" );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexUV );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    Initialize( pipelineCI );
}

void AColorPassVertexLight::Create( const char * _SourceCode, GHI::POLYGON_CULL _CullMode, bool _DepthTest, bool _Translucent, EColorBlending _Blending ) {
    PipelineCreateInfo pipelineCI = {};

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = _CullMode;
    rsd.bScissorEnable = SCISSOR_TEST;

    BlendingStateInfo bsd;
    bsd.SetDefaults();
    if ( _Translucent ) {
        bsd.RenderTargetSlots[0].SetBlendingPreset( GetBlendingPreset( _Blending ) );
    }

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

#ifdef DEPTH_PREPASS
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;
    if ( _Translucent ) {
        dssd.DepthFunc = CMPFUNC_GREATER;
    } else {
        dssd.DepthFunc = CMPFUNC_EQUAL;
    }
#else
    dssd.DepthFunc = CMPFUNC_GREATER;
#endif
    dssd.bDepthEnable = _DepthTest;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        },
        {
            "InVertexLight",
            4,              // location
            1,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexLight, VertexLight )
        }
    };

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( pipelineCI.pVertexAttribs, pipelineCI.NumVertexAttribs );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( "#define USE_VERTEX_LIGHT\n" );
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_COLOR\n" );
    GShaderSources.Add( "#define USE_VERTEX_LIGHT\n" );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLES;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pBlending = &bsd;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = &fragmentShaderModule;

    ShaderStageInfo stages[] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexLight );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    Initialize( pipelineCI );
}

void AShadowMapPass::Create( const char * _SourceCode, bool _ShadowMasking, bool _Skinned ) {
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

    VertexBindingInfo vertexBinding[2] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( SMeshVertex );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    vertexBinding[1].InputSlot = 1;
    vertexBinding[1].Stride = sizeof( SMeshVertexSkin );
    vertexBinding[1].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = _Skinned ? 2 : 1;
    pipelineCI.pVertexBindings = vertexBinding;

    static const VertexAttribInfo vertexAttribsSkinned[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        },
        {
            "InJointIndices",
            4,              // location
            1,              // buffer input slot
            VAT_UBYTE4,
            VAM_INTEGER,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexSkin, JointIndices )
        },
        {
            "InJointWeights",
            5,              // location
            1,              // buffer input slot
            VAT_UBYTE4N,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertexSkin, JointWeights )
        }
    };

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Position )
        },
        {
            "InTexCoord",
            1,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, TexCoord )
        },
        {
            "InTangent",
            2,              // location
            0,              // buffer input slot
            VAT_FLOAT4,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Tangent )
        },
        {
            "InNormal",
            3,              // location
            0,              // buffer input slot
            VAT_FLOAT3,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            GHI_STRUCT_OFS( SMeshVertex, Normal )
        }
    };

    if ( _Skinned ) {
        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribsSkinned );
        pipelineCI.pVertexAttribs = vertexAttribsSkinned;
    } else {
        pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
        pipelineCI.pVertexAttribs = vertexAttribs;
    }

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
    GShaderSources.Add( "#define MATERIAL_PASS_SHADOWMAP\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( VERTEX_SHADER, &vertexShaderModule );

    ShaderStageInfo & vs = stages[pipelineCI.NumStages++];
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = &vertexShaderModule;

    GShaderSources.Clear();
    GShaderSources.Add( "#define MATERIAL_PASS_SHADOWMAP\n" );
    if ( _Skinned ) {
        GShaderSources.Add( "#define SKINNED_MESH\n" );
    }
    GShaderSources.Add( _SourceCode );
    GShaderSources.Build( GEOMETRY_SHADER, &geometryShaderModule );

    ShaderStageInfo & gs = stages[pipelineCI.NumStages++];
    gs.Stage = SHADER_STAGE_GEOMETRY_BIT;
    gs.pModule = &geometryShaderModule;

    bool bVSM = false;

#if defined SHADOWMAP_VSM || defined SHADOWMAP_EVSM
    bVSM = true;
#endif

    if ( _ShadowMasking || bVSM ) {
        GShaderSources.Clear();
        GShaderSources.Add( "#define MATERIAL_PASS_SHADOWMAP\n" );
        if ( _ShadowMasking ) {
            GShaderSources.Add( "#define SHADOW_MASKING\n" );
        }
        if ( _Skinned ) {
            GShaderSources.Add( "#define SKINNED_MESH\n" );
        }
        GShaderSources.Add( _SourceCode );
        GShaderSources.Build( FRAGMENT_SHADER, &fragmentShaderModule );

        ShaderStageInfo & fs = stages[pipelineCI.NumStages++];
        fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
        fs.pModule = &fragmentShaderModule;
    }

    pipelineCI.pStages = stages;

    Initialize( pipelineCI );
}

}
