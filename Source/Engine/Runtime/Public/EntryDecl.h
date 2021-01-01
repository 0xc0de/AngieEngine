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

#include "GameModuleCallback.h"

#include <Core/Public/BaseTypes.h>

struct SEntryDecl
{
    const char * GameTitle;
    const char * RootPath;
    AClassMeta const * ModuleClass;
};

/** Runtime entry point */
void Runtime( const char * _CommandLine, SEntryDecl const & _EntryDecl );

/** Runtime entry point */
void Runtime( int _Argc, char ** _Argv, SEntryDecl const & _EntryDecl );

#ifdef AN_OS_WIN32
#include <Core/Public/WindowsDefs.h>
#include <shellapi.h>
static void RunEngineWin32( SEntryDecl const & _EntryDecl ) {
#if defined( AN_DEBUG ) && defined( AN_COMPILER_MSVC )
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
    //int argc;
    //LPWSTR * argv = ::CommandLineToArgvW( ::GetCommandLineW(), &argc );
    Runtime( ::GetCommandLineA(), _EntryDecl );
    //Runtime( argc, argv, _CreateGameModule );
    //LocalFree( argv );
}
#define AN_ENTRY_DECL( _EntryDecl ) \
int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow ) { \
    RunEngineWin32( _EntryDecl ); \
    return 0; \
}
#else
#define AN_ENTRY_DECL( _EntryDecl ) \
int main( int argc, char *argv[] ) { \
    Runtime( argc, argv, _EntryDecl ); \
    return 0; \
}
#endif
