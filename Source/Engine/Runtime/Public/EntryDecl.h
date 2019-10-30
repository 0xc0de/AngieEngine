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

#include "GameModuleCallback.h"

#include <Engine/Core/Public/BaseTypes.h>

/** Runtime entry point */
ANGIE_API void Runtime( const char * _CommandLine, FCreateGameModuleCallback _CreateGameModule );

/** Runtime entry point */
ANGIE_API void Runtime( int _Argc, char ** _Argv, FCreateGameModuleCallback _CreateGameModule );

#ifdef AN_OS_WIN32
#include <Engine/Core/Public/WindowsDefs.h>
static void RunEngineWin32( HINSTANCE hInstance, FCreateGameModuleCallback _CreateGameModule ) {
#if defined( AN_DEBUG ) && defined( AN_COMPILER_MSVC )
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
    Runtime( ::GetCommandLineA(), _CreateGameModule );
}
#define AN_ENTRY_DECL( _GameModuleClass ) \
int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow ) { \
    RunEngineWin32( hInstance, IGameModule::CreateGameModule< _GameModuleClass > ); \
    return 0; \
}
#else
#define AN_ENTRY_DECL( _GameModuleClass ) \
int main( int argc, char *argv[] ) { \
    Runtime( argc, argv, IGameModule::CreateGameModule< _GameModuleClass > ); \
    return 0; \
}
#endif
