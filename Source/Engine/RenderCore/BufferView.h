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

#include "Buffer.h"
#include "Texture.h"

namespace RenderCore {

enum BUFFER_VIEW_PIXEL_FORMAT : uint8_t
{
    BUFFER_VIEW_PIXEL_FORMAT_R8 = TEXTURE_FORMAT_R8,
    BUFFER_VIEW_PIXEL_FORMAT_R16 = TEXTURE_FORMAT_R16,
    BUFFER_VIEW_PIXEL_FORMAT_R16F = TEXTURE_FORMAT_R16F,
    BUFFER_VIEW_PIXEL_FORMAT_R32F = TEXTURE_FORMAT_R32F,
    BUFFER_VIEW_PIXEL_FORMAT_R8I = TEXTURE_FORMAT_R8I,
    BUFFER_VIEW_PIXEL_FORMAT_R16I = TEXTURE_FORMAT_R16I,
    BUFFER_VIEW_PIXEL_FORMAT_R32I = TEXTURE_FORMAT_R32I,
    BUFFER_VIEW_PIXEL_FORMAT_R8UI = TEXTURE_FORMAT_R8UI,
    BUFFER_VIEW_PIXEL_FORMAT_R16UI = TEXTURE_FORMAT_R16UI,
    BUFFER_VIEW_PIXEL_FORMAT_R32UI = TEXTURE_FORMAT_R32UI,
    BUFFER_VIEW_PIXEL_FORMAT_RG8 = TEXTURE_FORMAT_RG8,
    BUFFER_VIEW_PIXEL_FORMAT_RG16 = TEXTURE_FORMAT_RG16,
    BUFFER_VIEW_PIXEL_FORMAT_RG16F = TEXTURE_FORMAT_RG16F,
    BUFFER_VIEW_PIXEL_FORMAT_RG32F = TEXTURE_FORMAT_RG32F,
    BUFFER_VIEW_PIXEL_FORMAT_RG8I = TEXTURE_FORMAT_RG8I,
    BUFFER_VIEW_PIXEL_FORMAT_RG16I = TEXTURE_FORMAT_RG16I,
    BUFFER_VIEW_PIXEL_FORMAT_RG32I = TEXTURE_FORMAT_RG32I,
    BUFFER_VIEW_PIXEL_FORMAT_RG8UI = TEXTURE_FORMAT_RG8UI,
    BUFFER_VIEW_PIXEL_FORMAT_RG16UI = TEXTURE_FORMAT_RG16UI,
    BUFFER_VIEW_PIXEL_FORMAT_RG32UI = TEXTURE_FORMAT_RG32UI,
    BUFFER_VIEW_PIXEL_FORMAT_RGB32F = TEXTURE_FORMAT_RGB32F,
    BUFFER_VIEW_PIXEL_FORMAT_RGB32I = TEXTURE_FORMAT_RGB32I,
    BUFFER_VIEW_PIXEL_FORMAT_RGB32UI = TEXTURE_FORMAT_RGB32UI,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA8 = TEXTURE_FORMAT_RGBA8,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA16 = TEXTURE_FORMAT_RGBA16,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA16F = TEXTURE_FORMAT_RGBA16F,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA32F = TEXTURE_FORMAT_RGBA32F,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA8I = TEXTURE_FORMAT_RGBA8I,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA16I = TEXTURE_FORMAT_RGBA16I,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA32I = TEXTURE_FORMAT_RGBA32I,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA8UI = TEXTURE_FORMAT_RGBA8UI,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA16UI = TEXTURE_FORMAT_RGBA16UI,
    BUFFER_VIEW_PIXEL_FORMAT_RGBA32UI = TEXTURE_FORMAT_RGBA32UI
};

struct SBufferViewCreateInfo
{
    BUFFER_VIEW_PIXEL_FORMAT Format;
    size_t Offset;
    size_t SizeInBytes;
};

class IBufferView : public ITextureBase
{
public:
    IBufferView( IDevice * Device ) : ITextureBase( Device ) {}

    virtual size_t GetBufferOffset( uint16_t _Lod ) const = 0;
    virtual size_t GetBufferSizeInBytes( uint16_t _Lod ) const = 0;

protected:
    TRef< IBuffer > Buffer;
};

}
