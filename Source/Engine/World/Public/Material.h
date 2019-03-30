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

#include <Engine/World/Public/BaseObject.h>
#include <Engine/World/Public/RenderFrontend.h>
#include <Engine/World/Public/Texture.h>

class ANGIE_API FMaterial : public FBaseObject, public IRenderProxyOwner {
    AN_CLASS( FMaterial, FBaseObject )

public:
    //void LoadObject( const char * _Path ) override;

    void Initialize( FMaterialBuildData const * _Data );

    EMaterialType GetType() const { return Type; }

    FRenderProxy_Material * GetRenderProxy() { return RenderProxy; }

protected:
    FMaterial();
    ~FMaterial();

    // IRenderProxyOwner interface
    void OnLost() override { /* ... */ }

private:
    FRenderProxy_Material * RenderProxy;
    EMaterialType Type;
};

class FMaterialInstance : public FBaseObject {
    AN_CLASS( FMaterialInstance, FBaseObject )

    friend class FRenderFrontend;

public:

    TRefHolder< FMaterial > Material;

    void SetTexture( int _TextureSlot, FTexture * _Texture );

protected:

    FMaterialInstance() {}
    ~FMaterialInstance() {}

private:
    TRefHolder< FTexture > Textures[MAX_MATERIAL_TEXTURES];

    int VisMarker;
    FMaterialInstanceFrameData * FrameData;
};
