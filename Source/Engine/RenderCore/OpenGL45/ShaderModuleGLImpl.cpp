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

#include "ShaderModuleGLImpl.h"
#include "DeviceGLImpl.h"
#include "ImmediateContextGLImpl.h"
#include "LUT.h"
#include "GL/glew.h"

#include <Core/Public/CString.h>
#include <Core/Public/Logger.h>

namespace RenderCore {

AShaderModuleGLImpl::AShaderModuleGLImpl( ADeviceGLImpl * _Device, SShaderBinaryData const * _BinaryData )
    : IShaderModule( _Device ), pDevice( _Device )
{
    GLuint id = CreateShaderProgramBin( _BinaryData );
    if ( !id ) {
        return;
    }

    SetHandleNativeGL( id );
    Type = _BinaryData->ShaderType;

    pDevice->TotalShaderModules++;
}

AShaderModuleGLImpl::AShaderModuleGLImpl( ADeviceGLImpl * _Device, SHADER_TYPE _ShaderType, unsigned int _NumSources, const char * const * _Sources )
    : IShaderModule( _Device ), pDevice( _Device )
{
#if 0
    ShaderBinaryData binaryData;

    if ( !CreateShaderBinaryData( pDevice, _ShaderType, _NumSources, _Sources, &binaryData ) ) {
        GLogger.Printf( "AShaderModuleGLImpl::ctor: couldn't create shader binary data\n" );
        return;
    }

    GLuint id = CreateShaderProgramBin( &binaryData );

    DestroyShaderBinaryData( pDevice, &binaryData );

    if ( !id ) {
        return;
    }

    SetHandleNativeGL( id );
    Type = _BinaryData->ShaderType;

    pDevice->TotalShaderModules++;
#else

    GLuint id = CreateShaderProgram( _ShaderType, _NumSources, _Sources, false );
    if ( !id ) {
        return;
    }

    SetHandleNativeGL( id );
    Type = _ShaderType;

    pDevice->TotalShaderModules++;
#endif
}

AShaderModuleGLImpl::~AShaderModuleGLImpl() {
    GLuint id = GetHandleNativeGL();

    if ( !id ) {
        return;
    }

    glDeleteProgram( id );
    pDevice->TotalShaderModules--;
}

static bool CheckLinkStatus( unsigned int NativeId, SHADER_TYPE ShaderType )
{
    GLint linkStatus = 0;
    glGetProgramiv( NativeId, GL_LINK_STATUS, &linkStatus );
    if ( !linkStatus ) {

        #define MAX_ERROR_LOG_LENGTH  2048

        char errorLog[ MAX_ERROR_LOG_LENGTH ];
        errorLog[0] = 0;

        GLint infoLogLength = 0;
        glGetProgramiv( NativeId, GL_INFO_LOG_LENGTH, &infoLogLength );
        glGetProgramInfoLog( NativeId, sizeof( errorLog ), nullptr, errorLog );
        if ( (GLint)sizeof( errorLog ) < infoLogLength ) {
            errorLog[ sizeof( errorLog ) - 4 ] = '.';
            errorLog[ sizeof( errorLog ) - 3 ] = '.';
            errorLog[ sizeof( errorLog ) - 2 ] = '.';
        }

        if ( errorLog[0] ) {
            switch ( ShaderType ) {
            case VERTEX_SHADER:
                GLogger.Printf( "VS: %s\n", errorLog );
                break;
            case FRAGMENT_SHADER:
                GLogger.Printf( "FS: %s\n", errorLog );
                break;
            case TESS_CONTROL_SHADER:
                GLogger.Printf( "TCS: %s\n", errorLog );
                break;
            case TESS_EVALUATION_SHADER:
                GLogger.Printf( "TES: %s\n", errorLog );
                break;
            case GEOMETRY_SHADER:
                GLogger.Printf( "GS: %s\n", errorLog );
                break;
            case COMPUTE_SHADER:
                GLogger.Printf( "CS: %s\n", errorLog );
                break;
            }
        }

        return false;
    }

    return true;
}

unsigned int AShaderModuleGLImpl::CreateShaderProgramBin( SShaderBinaryData const * _BinaryData )
{
    if ( _BinaryData->BinaryFormat == SHADER_BINARY_FORMAT_SPIR_V_ARB
         && !pDevice->IsFeatureSupported( FEATURE_SPIR_V ) )
    {
         GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgramBin: SPIR-V binary format is not supported by video driver\n" );
         return 0;
    }

    GLuint program = glCreateProgram();
    if ( !program ) {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgramBin: failed to create shader program\n" );
        return 0;
    }

    //glProgramParameteri( program, GL_PROGRAM_SEPARABLE, GL_TRUE );
    glProgramParameteri( program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_FALSE );

    if ( _BinaryData->BinaryFormat == SHADER_BINARY_FORMAT_SPIR_V_ARB ) {
        GLuint numSpecializationConstants = 0;
        const GLuint* pConstantIndex = 0;
        const GLuint* pConstantValue = 0;

        GLuint shader = glCreateShader( ShaderTypeLUT[_BinaryData->ShaderType] );
        if ( shader ) {
            glShaderBinary( 1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, _BinaryData->BinaryCode, _BinaryData->BinarySize );
            glSpecializeShaderARB( shader, "main", numSpecializationConstants, pConstantIndex, pConstantValue );

            GLint compiled = GL_FALSE;
            glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
            if ( compiled ) {
                glAttachShader( program, shader );
                glLinkProgram( program );
                glDetachShader( program, shader );
            }
            else {
                GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgramBin: invalid compile status\n" );
            }
            glDeleteShader( shader );
        }
    }
    else {
#if 1
        glProgramBinary( program, _BinaryData->BinaryFormat, _BinaryData->BinaryCode, (GLsizei)_BinaryData->BinarySize );
#else
        // Other path
        GLuint shader = glCreateShader( ShaderTypeLUT[_BinaryData->ShaderType] );
        if ( shader ) {
            glShaderBinary( 1, &shader, _BinaryData->BinaryFormat, _BinaryData->BinaryCode, _BinaryData->BinarySize );
            glCompileShader( shader );

            GLint compiled = GL_FALSE;
            glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
            if ( compiled ) {
                glAttachShader( program, shader );
                glLinkProgram( program );
                glDetachShader( program, shader );
            }
            else {
                GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgramBin: invalid compile status\n" );
            }
            glDeleteShader( shader );
        }
#endif
    }

    if ( !CheckLinkStatus( program, _BinaryData->ShaderType ) ) {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgramBin: invalid link status\n" );
        glDeleteProgram( program );
        return 0;
    }

    return program;
}

unsigned int AShaderModuleGLImpl::CreateShaderProgram( SHADER_TYPE _ShaderType,
                                                       int _NumStrings,
                                                       const char * const * _Strings,
                                                       bool bBinaryRetrievable )
{
    GLenum type = ShaderTypeLUT[_ShaderType];
    GLuint program;

#if 1
    program = glCreateShaderProgramv( type, _NumStrings, _Strings );  // v 4.1
    if ( !program ) {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgram: failed to create shader program\n" );
        return 0;
    }

    glProgramParameteri( program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, bBinaryRetrievable );
#else
    program = glCreateProgram();
    if ( !program ) {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgram: failed to create shader program\n" );
        return 0;
    }

    glProgramParameteri( program, GL_PROGRAM_SEPARABLE, GL_TRUE );
    glProgramParameteri( program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, bBinaryRetrievable );

    // Other path
    GLuint shader = glCreateShader( type );
    if ( shader ) {
        glShaderSource( shader, _NumStrings, _Strings, nullptr );
        glCompileShader( shader );
        GLint compiled = GL_FALSE;
        glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
        if ( compiled ) {
            glAttachShader( program, shader );
            glLinkProgram( program );
            glDetachShader( program, shader );
        }
        else {
            GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgram: invalid compile status\n" );
        }
        glDeleteShader( shader );
    }
#endif

    if ( !CheckLinkStatus( program, _ShaderType ) ) {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderProgram: invalid link status\n" );
        glDeleteProgram( program );
        return 0;
    }

    return program;
}

bool AShaderModuleGLImpl::CreateShaderBinaryData( ADeviceGLImpl * _Device,
                                                  SHADER_TYPE _ShaderType,
                                                  unsigned int _NumSources,
                                                  const char * const * _Sources,
                                                  SShaderBinaryData * _BinaryData )
{
    GLuint id;
    GLsizei binaryLength;
    GLsizei length;
    GLenum format;
    uint8_t * binary;

    Core::ZeroMem( _BinaryData, sizeof( *_BinaryData ) );

    id = CreateShaderProgram( _ShaderType, _NumSources, _Sources, true );
    if ( !id ) {
        return false;
    }

    glGetProgramiv( id, GL_PROGRAM_BINARY_LENGTH, &binaryLength );
    if ( binaryLength ) {
        binary = ( uint8_t * )_Device->GetAllocator().Allocate( binaryLength );

        glGetProgramBinary( id, binaryLength, &length, &format, binary );

        _BinaryData->BinaryCode = binary;
        _BinaryData->BinarySize = length;
        _BinaryData->BinaryFormat = format;
        _BinaryData->ShaderType = _ShaderType;
    }
    else {
        GLogger.Printf( "AShaderModuleGLImpl::CreateShaderBinaryData: failed to retrieve shader program binary data\n" );
        glDeleteProgram( id );
        return false;
    }

    glDeleteProgram( id );
    return true;
}

void AShaderModuleGLImpl::DestroyShaderBinaryData( ADeviceGLImpl * _Device, SShaderBinaryData * _BinaryData )
{
    if ( !_BinaryData->BinaryCode ) {
        return;
    }

    uint8_t * binary = ( uint8_t * )_BinaryData->BinaryCode;
    _Device->GetAllocator().Deallocate( binary );

    Core::ZeroMem( _BinaryData, sizeof( *_BinaryData ) );
}

}
