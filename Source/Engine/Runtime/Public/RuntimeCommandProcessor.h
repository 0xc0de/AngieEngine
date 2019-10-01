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

#include <Engine/Core/Public/String.h>

class FRuntimeCommandProcessor;

class IRuntimeCommandContext {
public:
    virtual void ExecuteCommand( FRuntimeCommandProcessor const & _Proc ) = 0;
};

class FRuntimeCommandProcessor final {
    AN_FORBID_COPY( FRuntimeCommandProcessor )

public:
    static constexpr int MAX_ARGS = 256;
    static constexpr int MAX_ARG_LEN = 256;

    FRuntimeCommandProcessor();

    void ClearBuffer();

    void Add( const char * _Text );

    void Insert( const char * _Text );

    void Execute( IRuntimeCommandContext & _Ctx );

    const char * GetArg( int i ) const { return Args[i]; }

    int GetArgsCount() const { return ArgsCount; }

    static bool IsValidCommandName( const char * _Name );

private:
    FString Cmdbuf;
    int CmdbufPos;
    char Args[MAX_ARGS][MAX_ARG_LEN];
    int ArgsCount;
};
