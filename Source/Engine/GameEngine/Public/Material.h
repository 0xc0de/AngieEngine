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

#include <Engine/GameEngine/Public/BaseObject.h>
#include <Engine/GameEngine/Public/RenderFrontend.h>
#include <Engine/GameEngine/Public/Texture.h>

class ANGIE_API FMaterial : public FBaseObject, public IRenderProxyOwner {
    AN_CLASS( FMaterial, FBaseObject )

public:
    void Initialize( FMaterialBuildData const * _Data );

    EMaterialType GetType() const { return Type; }

    FRenderProxy_Material * GetRenderProxy() { return RenderProxy; }

    int GetNumUniformVectors() const { return NumUniformVectors; }

protected:
    FMaterial();
    ~FMaterial();

    // IRenderProxyOwner interface
    void OnLost() override { /* ... */ }

private:
    FRenderProxy_Material * RenderProxy;
    EMaterialType Type;
    int NumUniformVectors;
};

class FMaterialInstance : public FBaseObject {
    AN_CLASS( FMaterialInstance, FBaseObject )

    friend class FRenderFrontend;

public:

    TRef< FMaterial > Material;


    union {;
        float Uniforms[16];
        Float4 UniformVectors[4];
    };

    void SetTexture( int _TextureSlot, FTexture * _Texture );

protected:

    FMaterialInstance() {}
    ~FMaterialInstance() {}

private:
    TRef< FTexture > Textures[MAX_MATERIAL_TEXTURES];

    int VisMarker;
    FMaterialInstanceFrameData * FrameData;
};
