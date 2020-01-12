/*

Graphics Hardware Interface (GHI) is part of Angie Engine Source Code

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

#include "GHIBasic.h"

namespace GHI {

class Device;
class RenderPass;
class ShaderModule;

enum {
    DEFAULT_STENCIL_READ_MASK  = 0xff,
    DEFAULT_STENCIL_WRITE_MASK = 0xff
};

//
// Blending state
//

enum BLEND_OP : uint8_t
{
    BLEND_OP_ADD,              /// Rr=RssR+RddR Gr=GssG+GddG Br=BssB+BddB 	Ar=AssA+AddA
    BLEND_OP_SUBTRACT,         /// Rr=RssR−RddR Gr=GssG−GddG Br=BssB−BddB 	Ar=AssA−AddA
    BLEND_OP_REVERSE_SUBTRACT, /// Rr=RddR−RssR Gr=GddG−GssG Br=BddB−BssB 	Ar=AddA−AssA
    BLEND_OP_MIN,              /// Rr=min(Rs,Rd) Gr=min(Gs,Gd) Br=min(Bs,Bd) 	Ar=min(As,Ad)
    BLEND_OP_MAX               /// Rr=max(Rs,Rd) Gr=max(Gs,Gd) Br=max(Bs,Bd) 	Ar=max(As,Ad)
};

enum BLEND_FUNC : uint8_t
{
    BLEND_FUNC_ZERO,                /// ( 0,  0,  0,  0 )
    BLEND_FUNC_ONE,                 /// ( 1,  1,  1,  1 )

    BLEND_FUNC_SRC_COLOR,           /// ( Rs0 / kr,   Gs0 / kg,   Bs0 / kb,   As0 / ka )
    BLEND_FUNC_INV_SRC_COLOR,       /// ( 1,  1,  1,  1 ) - ( Rs0 / kr,   Gs0 / kg,   Bs0 / kb,   As0 / ka )

    BLEND_FUNC_DST_COLOR,           /// ( Rd0 / kr,   Gd0 / kg,   Bd0 / kb,   Ad0 / ka )
    BLEND_FUNC_INV_DST_COLOR,       /// ( 1,  1,  1,  1 ) - ( Rd0 / kr,   Gd0 / kg,   Bd0 / kb,   Ad0 / ka )

    BLEND_FUNC_SRC_ALPHA,           /// ( As0 / kA,   As0 / kA,   As0 / kA,   As0 / kA )
    BLEND_FUNC_INV_SRC_ALPHA,       /// ( 1,  1,  1,  1 ) - ( As0 / kA,   As0 / kA,   As0 / kA,   As0 / kA )

    BLEND_FUNC_DST_ALPHA,           /// ( Ad / kA,    Ad / kA,    Ad / kA,    Ad / kA )
    BLEND_FUNC_INV_DST_ALPHA,       /// ( 1,  1,  1,  1 ) - ( Ad / kA,    Ad / kA,    Ad / kA,    Ad / kA )

    BLEND_FUNC_CONSTANT_COLOR,      /// ( Rc, Gc, Bc, Ac )
    BLEND_FUNC_INV_CONSTANT_COLOR,  /// ( 1,  1,  1,  1 ) - ( Rc, Gc, Bc, Ac )

    BLEND_FUNC_CONSTANT_ALPHA,      /// ( Ac, Ac, Ac, Ac )
    BLEND_FUNC_INV_CONSTANT_ALPHA,  /// ( 1,  1,  1,  1 ) - ( Ac, Ac, Ac, Ac )

    BLEND_FUNC_SRC_ALPHA_SATURATE,  /// ( i,  i,  i,  1 )

    BLEND_FUNC_SRC1_COLOR,          /// ( Rs1 / kR,   Gs1 / kG,   Bs1 / kB,   As1 / kA )
    BLEND_FUNC_INV_SRC1_COLOR,      /// ( 1,  1,  1,  1 ) - ( Rs1 / kR,   Gs1 / kG,   Bs1 / kB,   As1 / kA )

    BLEND_FUNC_SRC1_ALPHA,          /// ( As1 / kA,   As1 / kA,   As1 / kA,   As1 / kA )
    BLEND_FUNC_INV_SRC1_ALPHA       /// ( 1,  1,  1,  1 ) - ( As1 / kA,   As1 / kA,   As1 / kA,   As1 / kA )
};

enum BLENDING_PRESET : uint8_t
{
    BLENDING_NO_BLEND,
    BLENDING_ALPHA,
    BLENDING_COLOR_ADD,
    BLENDING_MULTIPLY,
    BLENDING_SOURCE_TO_DEST,
    BLENDING_ADD_MUL,
    BLENDING_ADD_ALPHA,

    BLENDING_MAX_PRESETS
};

enum LOGIC_OP : uint8_t
{
    LOGIC_OP_COPY,
    LOGIC_OP_COPY_INV,
    LOGIC_OP_CLEAR,
    LOGIC_OP_SET,
    LOGIC_OP_NOOP,
    LOGIC_OP_INVERT,
    LOGIC_OP_AND,
    LOGIC_OP_NAND,
    LOGIC_OP_OR,
    LOGIC_OP_NOR,
    LOGIC_OP_XOR,
    LOGIC_OP_EQUIV,
    LOGIC_OP_AND_REV,
    LOGIC_OP_AND_INV,
    LOGIC_OP_OR_REV,
    LOGIC_OP_OR_INV
};

enum COLOR_WRITE_MASK : uint8_t
{
    COLOR_WRITE_DISABLED = 0,
    COLOR_WRITE_R_BIT = 1,
    COLOR_WRITE_G_BIT = 2,
    COLOR_WRITE_B_BIT = 4,
    COLOR_WRITE_A_BIT = 8,
    COLOR_WRITE_RGBA = COLOR_WRITE_R_BIT|COLOR_WRITE_G_BIT|COLOR_WRITE_B_BIT|COLOR_WRITE_A_BIT,
    COLOR_WRITE_RGB = COLOR_WRITE_R_BIT|COLOR_WRITE_G_BIT|COLOR_WRITE_B_BIT
};

struct RenderTargetBlendingInfo {
    struct Operation_t {
        BLEND_OP       ColorRGB;
        BLEND_OP       Alpha;
    } Op;

    struct Function_t {
        BLEND_FUNC     SrcFactorRGB;
        BLEND_FUNC     DstFactorRGB;
        BLEND_FUNC     SrcFactorAlpha;
        BLEND_FUNC     DstFactorAlpha;
    } Func;

    bool               bBlendEnable;
    COLOR_WRITE_MASK   ColorWriteMask;

    // General blend equation:
    // if ( BlendEnable ) {
    //     ResultColorRGB = ( SourceColor.rgb * SrcFactorRGB ) Op.ColorRGB ( DestColor.rgb * DstFactorRGB )
    //     ResultAlpha    = ( SourceColor.a * SrcFactorAlpha ) Op.Alpha    ( DestColor.a * DstFactorAlpha )
    // } else {
    //     ResultColorRGB = SourceColor.rgb;
    //     ResultAlpha    = SourceColor.a;
    // }

    void SetDefaults() {
        Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
        Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
        Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ZERO;
        bBlendEnable = false;
        ColorWriteMask = COLOR_WRITE_RGBA;
    }

    void SetBlendingPreset( BLENDING_PRESET _Preset );
};

struct BlendingStateInfo {
    bool     bSampleAlphaToCoverage;
    bool     bIndependentBlendEnable;
    LOGIC_OP LogicOp;
    RenderTargetBlendingInfo RenderTargetSlots[MAX_COLOR_ATTACHMENTS];

    void SetDefaults() {
        bSampleAlphaToCoverage = false;
        bIndependentBlendEnable = false;
        LogicOp = LOGIC_OP_COPY;
        RenderTargetSlots[0].SetDefaults();
    }
};

//
// Rasterizer state
//

enum POLYGON_FILL : uint8_t
{
    POLYGON_FILL_SOLID  = 0,
    POLYGON_FILL_WIRE   = 1
};

enum POLYGON_CULL : uint8_t
{
    POLYGON_CULL_BACK      = 0,
    POLYGON_CULL_FRONT     = 1,
    POLYGON_CULL_DISABLED  = 2
};

struct RasterizerStateInfo {
    POLYGON_FILL    FillMode;
    POLYGON_CULL    CullMode;
    bool            bFrontClockwise;

    struct {
        /*
                       _
                      |       MaxDepthSlope x Slope + r * Bias,           if Clamp = 0 or NaN;
                      |
        DepthOffset = <   min(MaxDepthSlope x Slope + r * Bias, Clamp),   if Clamp > 0;
                      |
                      |_  max(MaxDepthSlope x Slope + r * Bias, Clamp),   if Clamp < 0.

        */
        float       Slope;
        int         Bias;
        float       Clamp;
    } DepthOffset;

    bool            bDepthClampEnable;           // If enabled, the −wc ≤ zc ≤ wc plane equation is ignored by view volume clipping
                                                // (effectively, there is no near or far plane clipping). See viewport->MinDepth, viewport->MaxDepth.
    bool            bScissorEnable;
    bool            bMultisampleEnable;
    bool            bAntialiasedLineEnable;
    bool            bRasterizerDiscard;          // If enabled, primitives are discarded after the optional transform feedback stage, but before rasterization

    void SetDefaults() {
        FillMode = POLYGON_FILL_SOLID;
        CullMode = POLYGON_CULL_BACK;
        bFrontClockwise = false;
        DepthOffset.Slope = 0;
        DepthOffset.Bias = 0;
        DepthOffset.Clamp = 0;
        bDepthClampEnable = false;
        bScissorEnable = false;
        bMultisampleEnable = false;
        bAntialiasedLineEnable = false;
        bRasterizerDiscard = false;
    }
};

//
// Depth-Stencil state
//

enum DEPTH_WRITE_MASK : uint8_t
{
    DEPTH_WRITE_DISABLE   = 0,
    DEPTH_WRITE_ENABLE    = 1
};

enum STENCIL_OP : uint8_t
{
    STENCIL_OP_KEEP      = 0,
    STENCIL_OP_ZERO      = 1,
    STENCIL_OP_REPLACE   = 2,
    STENCIL_OP_INCR_SAT  = 3,
    STENCIL_OP_DECR_SAT  = 4,
    STENCIL_OP_INVERT    = 5,
    STENCIL_OP_INCR      = 6,
    STENCIL_OP_DECR      = 7
};

struct StencilTestInfo {
    STENCIL_OP         StencilFailOp;
    STENCIL_OP         DepthFailOp;
    STENCIL_OP         DepthPassOp;
    COMPARISON_FUNCTION StencilFunc;
    //int              Reference;

    void SetDefaults() {
        StencilFailOp = STENCIL_OP_KEEP;
        DepthFailOp = STENCIL_OP_KEEP;
        DepthPassOp = STENCIL_OP_KEEP;
        StencilFunc = CMPFUNC_ALWAYS;
        //Reference = 0;
    }
};

struct DepthStencilStateInfo {
    bool              bDepthEnable;
    DEPTH_WRITE_MASK  DepthWriteMask;
    COMPARISON_FUNCTION DepthFunc;
    bool              bStencilEnable;
    uint8_t           StencilReadMask;
    uint8_t           StencilWriteMask;
    StencilTestInfo   FrontFace;
    StencilTestInfo   BackFace;

    void SetDefaults() {
        bDepthEnable = true;
        DepthWriteMask = DEPTH_WRITE_ENABLE;
        DepthFunc = CMPFUNC_LESS;
        bStencilEnable = false;
        StencilReadMask = DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = DEFAULT_STENCIL_WRITE_MASK;
        FrontFace.SetDefaults();
        BackFace.SetDefaults();
    }
};

// Vertex bindings and attributes

#define GHI_VertexAttribType_NormalizedBit          ( 1<<7 )
#define GHI_VertexAttribType_CountBit( Count )      ( (((Count)-1) & 3 )<<5 )
#define GHI_5BitNumber( Number )                    ( (Number) & 31 )

enum VERTEX_ATTRIB_COMPONENT : uint8_t
{
    COMPONENT_BYTE        = GHI_5BitNumber(0),
    COMPONENT_UBYTE       = GHI_5BitNumber(1),
    COMPONENT_SHORT       = GHI_5BitNumber(2),
    COMPONENT_USHORT      = GHI_5BitNumber(3),
    COMPONENT_INT         = GHI_5BitNumber(4),
    COMPONENT_UINT        = GHI_5BitNumber(5),
    COMPONENT_HALF        = GHI_5BitNumber(6),
    COMPONENT_FLOAT       = GHI_5BitNumber(7),
    COMPONENT_DOUBLE      = GHI_5BitNumber(8)

    // TODO: add here other types

    // MAX = 31
};

enum VERTEX_ATTRIB_TYPE : uint8_t
{
    /// Signed byte
    VAT_BYTE1             = COMPONENT_BYTE | GHI_VertexAttribType_CountBit(1),
    VAT_BYTE2             = COMPONENT_BYTE | GHI_VertexAttribType_CountBit(2),
    VAT_BYTE3             = COMPONENT_BYTE | GHI_VertexAttribType_CountBit(3),
    VAT_BYTE4             = COMPONENT_BYTE | GHI_VertexAttribType_CountBit(4),
    VAT_BYTE1N            = VAT_BYTE1 | GHI_VertexAttribType_NormalizedBit,
    VAT_BYTE2N            = VAT_BYTE2 | GHI_VertexAttribType_NormalizedBit,
    VAT_BYTE3N            = VAT_BYTE3 | GHI_VertexAttribType_NormalizedBit,
    VAT_BYTE4N            = VAT_BYTE4 | GHI_VertexAttribType_NormalizedBit,

    /// Unsigned byte
    VAT_UBYTE1            = COMPONENT_UBYTE | GHI_VertexAttribType_CountBit(1),
    VAT_UBYTE2            = COMPONENT_UBYTE | GHI_VertexAttribType_CountBit(2),
    VAT_UBYTE3            = COMPONENT_UBYTE | GHI_VertexAttribType_CountBit(3),
    VAT_UBYTE4            = COMPONENT_UBYTE | GHI_VertexAttribType_CountBit(4),
    VAT_UBYTE1N           = VAT_UBYTE1 | GHI_VertexAttribType_NormalizedBit,
    VAT_UBYTE2N           = VAT_UBYTE2 | GHI_VertexAttribType_NormalizedBit,
    VAT_UBYTE3N           = VAT_UBYTE3 | GHI_VertexAttribType_NormalizedBit,
    VAT_UBYTE4N           = VAT_UBYTE4 | GHI_VertexAttribType_NormalizedBit,

    /// Signed short (16 bit integer)
    VAT_SHORT1            = COMPONENT_SHORT | GHI_VertexAttribType_CountBit(1),
    VAT_SHORT2            = COMPONENT_SHORT | GHI_VertexAttribType_CountBit(2),
    VAT_SHORT3            = COMPONENT_SHORT | GHI_VertexAttribType_CountBit(3),
    VAT_SHORT4            = COMPONENT_SHORT | GHI_VertexAttribType_CountBit(4),
    VAT_SHORT1N           = VAT_SHORT1 | GHI_VertexAttribType_NormalizedBit,
    VAT_SHORT2N           = VAT_SHORT2 | GHI_VertexAttribType_NormalizedBit,
    VAT_SHORT3N           = VAT_SHORT3 | GHI_VertexAttribType_NormalizedBit,
    VAT_SHORT4N           = VAT_SHORT4 | GHI_VertexAttribType_NormalizedBit,

    /// Unsigned short (16 bit integer)
    VAT_USHORT1           = COMPONENT_USHORT | GHI_VertexAttribType_CountBit(1),
    VAT_USHORT2           = COMPONENT_USHORT | GHI_VertexAttribType_CountBit(2),
    VAT_USHORT3           = COMPONENT_USHORT | GHI_VertexAttribType_CountBit(3),
    VAT_USHORT4           = COMPONENT_USHORT | GHI_VertexAttribType_CountBit(4),
    VAT_USHORT1N          = VAT_USHORT1 | GHI_VertexAttribType_NormalizedBit,
    VAT_USHORT2N          = VAT_USHORT2 | GHI_VertexAttribType_NormalizedBit,
    VAT_USHORT3N          = VAT_USHORT3 | GHI_VertexAttribType_NormalizedBit,
    VAT_USHORT4N          = VAT_USHORT4 | GHI_VertexAttribType_NormalizedBit,

    /// 32-bit signed integer
    VAT_INT1              = COMPONENT_INT | GHI_VertexAttribType_CountBit(1),
    VAT_INT2              = COMPONENT_INT | GHI_VertexAttribType_CountBit(2),
    VAT_INT3              = COMPONENT_INT | GHI_VertexAttribType_CountBit(3),
    VAT_INT4              = COMPONENT_INT | GHI_VertexAttribType_CountBit(4),
    VAT_INT1N             = VAT_INT1 | GHI_VertexAttribType_NormalizedBit,
    VAT_INT2N             = VAT_INT2 | GHI_VertexAttribType_NormalizedBit,
    VAT_INT3N             = VAT_INT3 | GHI_VertexAttribType_NormalizedBit,
    VAT_INT4N             = VAT_INT4 | GHI_VertexAttribType_NormalizedBit,

    /// 32-bit unsigned integer
    VAT_UINT1             = COMPONENT_UINT | GHI_VertexAttribType_CountBit(1),
    VAT_UINT2             = COMPONENT_UINT | GHI_VertexAttribType_CountBit(2),
    VAT_UINT3             = COMPONENT_UINT | GHI_VertexAttribType_CountBit(3),
    VAT_UINT4             = COMPONENT_UINT | GHI_VertexAttribType_CountBit(4),
    VAT_UINT1N            = VAT_UINT1 | GHI_VertexAttribType_NormalizedBit,
    VAT_UINT2N            = VAT_UINT2 | GHI_VertexAttribType_NormalizedBit,
    VAT_UINT3N            = VAT_UINT3 | GHI_VertexAttribType_NormalizedBit,
    VAT_UINT4N            = VAT_UINT4 | GHI_VertexAttribType_NormalizedBit,

    /// 16-bit floating point
    VAT_HALF1             = COMPONENT_HALF | GHI_VertexAttribType_CountBit(1),       // only with IsHalfFloatVertexSupported
    VAT_HALF2             = COMPONENT_HALF | GHI_VertexAttribType_CountBit(2),       // only with IsHalfFloatVertexSupported
    VAT_HALF3             = COMPONENT_HALF | GHI_VertexAttribType_CountBit(3),       // only with IsHalfFloatVertexSupported
    VAT_HALF4             = COMPONENT_HALF | GHI_VertexAttribType_CountBit(4),       // only with IsHalfFloatVertexSupported

    /// 32-bit floating point
    VAT_FLOAT1            = COMPONENT_FLOAT | GHI_VertexAttribType_CountBit(1),
    VAT_FLOAT2            = COMPONENT_FLOAT | GHI_VertexAttribType_CountBit(2),
    VAT_FLOAT3            = COMPONENT_FLOAT | GHI_VertexAttribType_CountBit(3),
    VAT_FLOAT4            = COMPONENT_FLOAT | GHI_VertexAttribType_CountBit(4),

    /// 64-bit floating point
    VAT_DOUBLE1           = COMPONENT_DOUBLE | GHI_VertexAttribType_CountBit(1),
    VAT_DOUBLE2           = COMPONENT_DOUBLE | GHI_VertexAttribType_CountBit(2),
    VAT_DOUBLE3           = COMPONENT_DOUBLE | GHI_VertexAttribType_CountBit(3),
    VAT_DOUBLE4           = COMPONENT_DOUBLE | GHI_VertexAttribType_CountBit(4)
};

enum VERTEX_ATTRIB_MODE : uint8_t
{
    VAM_FLOAT,
    VAM_DOUBLE,
    VAM_INTEGER,
};

enum VERTEX_INPUT_RATE : uint8_t
{
    INPUT_RATE_PER_VERTEX   = 0,
    INPUT_RATE_PER_INSTANCE = 1
};

struct VertexBindingInfo {
    uint8_t             InputSlot;          /// vertex buffer binding
    uint32_t            Stride;             /// vertex stride
    VERTEX_INPUT_RATE   InputRate;          /// per vertex / per instance
};

struct VertexAttribInfo {
    const char *        SemanticName;
    uint32_t            Location;
    uint32_t            InputSlot;          /// vertex buffer binding
    VERTEX_ATTRIB_TYPE  Type;
    VERTEX_ATTRIB_MODE  Mode;               /// float / double / integer

    uint32_t            InstanceDataStepRate; /// Only for INPUT_RATE_PER_INSTANCE. The number of instances to draw using same
                                              /// per-instance data before advancing in the buffer by one element. This value must
                                              /// by 0 for an element that contains per-vertex data (InputRate = INPUT_RATE_PER_VERTEX)

    uint32_t           Offset;              /// attribute offset

    /// Number of vector components 1,2,3,4
    int NumComponents() const { return ( ( Type >> 5 ) & 3 ) + 1; }

    /// Type of vector components COMPONENT_BYTE, COMPONENT_SHORT, COMPONENT_HALF, COMPONENT_FLOAT, etc.
    VERTEX_ATTRIB_COMPONENT TypeOfComponent() const { return (VERTEX_ATTRIB_COMPONENT)GHI_5BitNumber( Type ); }

    /// Components are normalized
    bool IsNormalized() const { return !!(Type & GHI_VertexAttribType_NormalizedBit); }
};

template< typename TString >
TString ShaderStringForVertexAttribs( VertexAttribInfo const * _VertexAttribs, int _NumVertexAttribs ) {
    // TODO: modify to compile time?

    TString s;
    const char * attribType;
    int attribIndex = 0;
    char location[ 16 ];

    const char * Types[ 4 ][ 4 ] = {
        { "vec1",  "vec2",  "vec3",  "vec4" },      // Float types
        { "dvec1", "dvec2", "dvec3", "dvec4" },     // Double types
        { "ivec1", "ivec2", "ivec3", "ivec4" },     // Integer types
        { "uvec1", "uvec2", "uvec3", "uvec4" }      // Unsigned types
    };

    for ( VertexAttribInfo const * attrib = _VertexAttribs; attrib < &_VertexAttribs[ _NumVertexAttribs ]; attrib++, attribIndex++ ) {

        VERTEX_ATTRIB_COMPONENT typeOfComponent = attrib->TypeOfComponent();

        if ( attrib->Mode == VAM_INTEGER && ( typeOfComponent == COMPONENT_UBYTE || typeOfComponent == COMPONENT_USHORT || typeOfComponent == COMPONENT_UINT ) ) {
            attribType = Types[ 3 ][ attrib->NumComponents() - 1 ];
        } else {
            attribType = Types[ attrib->Mode ][ attrib->NumComponents() - 1 ];
        }

        snprintf( location, sizeof( location ), "%d", attrib->Location );

        s += "layout( location = ";
        s += location;
        s += " ) in ";
        s += attribType;
        s += " ";
        s += attrib->SemanticName;
        s += ";\n";
    }

    return s;
}

enum SHADER_STAGE_FLAG_BITS : uint32_t
{
    SHADER_STAGE_VERTEX_BIT                     = 0x00000001,
    SHADER_STAGE_FRAGMENT_BIT                   = 0x00000002,
    SHADER_STAGE_GEOMETRY_BIT                   = 0x00000004,
    SHADER_STAGE_TESSELLATION_CONTROL_BIT       = 0x00000008,
    SHADER_STAGE_TESSELLATION_EVALUATION_BIT    = 0x00000010,
    SHADER_STAGE_COMPUTE_BIT                    = 0x00000020,
    //SHADER_STAGE_ALL_GRAPHICS                 = 0x0000001F,
    //SHADER_STAGE_ALL                          = 0x7FFFFFFF
};

struct ShaderStageInfo {
    SHADER_STAGE_FLAG_BITS  Stage;
    ShaderModule const *    pModule;
};

enum PRIMITIVE_TOPOLOGY : uint8_t
{
    PRIMITIVE_UNDEFINED                    = 0,
    PRIMITIVE_POINTS                       = 1,
    PRIMITIVE_LINES                        = 2,
    PRIMITIVE_LINE_STRIP                   = 3,
    PRIMITIVE_LINE_LOOP                    = 4,
    PRIMITIVE_TRIANGLES                    = 5,
    PRIMITIVE_TRIANGLE_STRIP               = 6,
    PRIMITIVE_TRIANGLE_FAN                 = 7,
    PRIMITIVE_LINES_ADJ                    = 8,
    PRIMITIVE_LINE_STRIP_ADJ               = 9,
    PRIMITIVE_TRIANGLES_ADJ                = 10,
    PRIMITIVE_TRIANGLE_STRIP_ADJ           = 11,
    PRIMITIVE_PATCHES_1   = 12,
    PRIMITIVE_PATCHES_2   = PRIMITIVE_PATCHES_1 + 1,
    PRIMITIVE_PATCHES_3   = PRIMITIVE_PATCHES_1 + 2,
    PRIMITIVE_PATCHES_4   = PRIMITIVE_PATCHES_1 + 3,
    PRIMITIVE_PATCHES_5   = PRIMITIVE_PATCHES_1 + 4,
    PRIMITIVE_PATCHES_6   = PRIMITIVE_PATCHES_1 + 5,
    PRIMITIVE_PATCHES_7   = PRIMITIVE_PATCHES_1 + 6,
    PRIMITIVE_PATCHES_8   = PRIMITIVE_PATCHES_1 + 7,
    PRIMITIVE_PATCHES_9   = PRIMITIVE_PATCHES_1 + 8,
    PRIMITIVE_PATCHES_10  = PRIMITIVE_PATCHES_1 + 9,
    PRIMITIVE_PATCHES_11  = PRIMITIVE_PATCHES_1 + 10,
    PRIMITIVE_PATCHES_12  = PRIMITIVE_PATCHES_1 + 11,
    PRIMITIVE_PATCHES_13  = PRIMITIVE_PATCHES_1 + 12,
    PRIMITIVE_PATCHES_14  = PRIMITIVE_PATCHES_1 + 13,
    PRIMITIVE_PATCHES_15  = PRIMITIVE_PATCHES_1 + 14,
    PRIMITIVE_PATCHES_16  = PRIMITIVE_PATCHES_1 + 15,
    PRIMITIVE_PATCHES_17  = PRIMITIVE_PATCHES_1 + 16,
    PRIMITIVE_PATCHES_18  = PRIMITIVE_PATCHES_1 + 17,
    PRIMITIVE_PATCHES_19  = PRIMITIVE_PATCHES_1 + 18,
    PRIMITIVE_PATCHES_20  = PRIMITIVE_PATCHES_1 + 19,
    PRIMITIVE_PATCHES_21  = PRIMITIVE_PATCHES_1 + 20,
    PRIMITIVE_PATCHES_22  = PRIMITIVE_PATCHES_1 + 21,
    PRIMITIVE_PATCHES_23  = PRIMITIVE_PATCHES_1 + 22,
    PRIMITIVE_PATCHES_24  = PRIMITIVE_PATCHES_1 + 23,
    PRIMITIVE_PATCHES_25  = PRIMITIVE_PATCHES_1 + 24,
    PRIMITIVE_PATCHES_26  = PRIMITIVE_PATCHES_1 + 25,
    PRIMITIVE_PATCHES_27  = PRIMITIVE_PATCHES_1 + 26,
    PRIMITIVE_PATCHES_28  = PRIMITIVE_PATCHES_1 + 27,
    PRIMITIVE_PATCHES_29  = PRIMITIVE_PATCHES_1 + 28,
    PRIMITIVE_PATCHES_30  = PRIMITIVE_PATCHES_1 + 29,
    PRIMITIVE_PATCHES_31  = PRIMITIVE_PATCHES_1 + 30,
    PRIMITIVE_PATCHES_32  = PRIMITIVE_PATCHES_1 + 31
};

struct PipelineInputAssemblyInfo {
    PRIMITIVE_TOPOLOGY Topology;
    bool               bPrimitiveRestart;  /// has no effect on non-indexed drawing commands
};

struct PipelineCreateInfo {
    PipelineInputAssemblyInfo const * pInputAssembly;
    BlendingStateInfo const *         pBlending;
    RasterizerStateInfo const *       pRasterizer;
    DepthStencilStateInfo const *     pDepthStencil;
    uint32_t                          NumStages;
    ShaderStageInfo const *           pStages;
    uint32_t                          NumVertexBindings;
    VertexBindingInfo const *         pVertexBindings;
    uint32_t                          NumVertexAttribs;
    VertexAttribInfo const *          pVertexAttribs;
    RenderPass *                      pRenderPass;
    int                               Subpass;
};

class Pipeline /*final*/ : public NonCopyable, IObjectInterface {

   friend class CommandBuffer;

public:
    Pipeline();
    ~Pipeline();

    void Initialize( PipelineCreateInfo const & _CreateInfo );
    void Deinitialize();

    void * GetHandle() const { return Handle; }

private:
    Device *              pDevice;
    void *                Handle;
    struct VertexArrayObject * VAO;
    BlendingStateInfo const * BlendingState;
    RasterizerStateInfo const * RasterizerState;
    DepthStencilStateInfo const * DepthStencilState;
    //unsigned int        CurrentIndexBuffer;
    unsigned int          IndexBufferType;        // type of current binded index buffer (uin16 or uint32_t)
    size_t                IndexBufferTypeSizeOf;  // size of one index
    unsigned int          IndexBufferOffset;      // offset in current binded index buffer
    unsigned int          PrimitiveTopology;
    int                   NumPatchVertices;
    bool                  bPrimitiveRestartEnabled;
    RenderPass *          pRenderPass;
    int                   Subpass;
};

}
