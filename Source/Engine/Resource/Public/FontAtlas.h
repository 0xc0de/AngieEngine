/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2019 Alexander Samusev.

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

#include "Texture.h"
#include <Engine/imgui/imgui.h>

class FFont : public FBaseObject {
    AN_CLASS( FFont, FBaseObject )

public:
    // Initialize from memory
    void InitializeFromMemoryTTF( const void * _SysMem, size_t _SizeInBytes, float _SizePixels, unsigned short const * _GlyphRanges = nullptr );

    // Initialize from memory compressed
    void InitializeFromMemoryCompressedTTF( const void * _SysMem, size_t _SizeInBytes, float _SizePixels, unsigned short const * _GlyphRanges = nullptr );

    // Initialize from memory compressed base85
    void InitializeFromMemoryCompressedBase85TTF( const char * _SysMem, float _SizePixels, unsigned short const * _GlyphRanges = nullptr );

    // Create font from string (FFont.***)
    void InitializeInternalResource( const char * _InternalResourceName ) override;

    // Initialize font from file
    bool InitializeFromFile( const char * _Path, bool _CreateDefultObjectIfFails = true ) override;

    // Purge model data
    void Purge();

    bool IsValid() const;

    int GetFontSize() const;

    ImFontGlyph const * FindGlyph( FWideChar c ) const;

    float GetCharAdvance( FWideChar c ) const;

    Float2 CalcTextSizeA( float _Size, float _MaxWidth, float _WrapWidth, const char * _TextBegin, const char * _TextEnd = nullptr, const char** _Remaining = nullptr ) const; // utf8

    const char * CalcWordWrapPositionA( float _Scale, const char * _Text, const char * _TextEnd, float _WrapWidth ) const;

    FWideChar const * CalcWordWrapPositionW( float _Scale, FWideChar const * _Text, FWideChar const * _TextEnd, float _WrapWidth ) const;

    void SetDisplayOffset( Float2 const & _Offset );

    Float2 const & GetDisplayOffset() const;

    void * GetImguiFontAtlas() { return &Atlas; }

    FTexture2D * GetTexture() { return AtlasTexture; }

    static void SetGlyphRanges( const unsigned short * _GlyphRanges );

    // Helpers.
    static const unsigned short * GetGlyphRangesDefault();                // Basic Latin, Extended Latin
    static const unsigned short * GetGlyphRangesKorean();                 // Default + Korean characters
    static const unsigned short * GetGlyphRangesJapanese();               // Default + Hiragana, Katakana, Half-Width, Selection of 1946 Ideographs
    static const unsigned short * GetGlyphRangesChineseFull();            // Default + Half-Width + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
    static const unsigned short * GetGlyphRangesChineseSimplifiedCommon();// Default + Half-Width + Japanese Hiragana/Katakana + set of 2500 CJK Unified Ideographs for common simplified Chinese
    static const unsigned short * GetGlyphRangesCyrillic();               // Default + about 400 Cyrillic characters
    static const unsigned short * GetGlyphRangesThai();                   // Default + Thai characters
    static const unsigned short * GetGlyphRangesVietnamese();             // Default + Vietname characters

protected:
    FFont() {}

private:
    void CreateTexture();

    ImFontAtlas Atlas;
    ImFont * Font;
    TRef< FTexture2D > AtlasTexture;
};
