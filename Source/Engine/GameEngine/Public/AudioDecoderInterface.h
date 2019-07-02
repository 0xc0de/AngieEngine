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

#include "BaseObject.h"

class ANGIE_API IAudioStreamInterface : public FBaseObject {
    AN_CLASS( IAudioStreamInterface, FBaseObject )

public:
    virtual bool InitializeFileStream( const char * _FileName );

    virtual bool InitializeMemoryStream( const byte * _EncodedData, int _EncodedDataLength );

    virtual void StreamRewind();

    virtual void StreamSeek( int _PositionInSamples );

    virtual int StreamDecodePCM( short * _Buffer, int _NumShorts );

protected:
    IAudioStreamInterface() {}
};

class ANGIE_API IAudioDecoderInterface : public FBaseObject {
    AN_CLASS( IAudioDecoderInterface, FBaseObject )

public:        
    virtual IAudioStreamInterface * CreateAudioStream();

    // Decode file to memory
    virtual bool DecodePCM( const char * _FileName, int * _SamplesCount, int * _Channels, int * _SampleRate, int * _BitsPerSample, /* optional */ short ** _PCM );

    // Decode from raw memory
    virtual bool DecodePCM( const char * _FileName, const byte * _Data, size_t _DataLength, int * _SamplesCount, int * _Channels, int * _SampleRate, int * _BitsPerSample, short ** _PCM );

    // Read encoded data from file
    virtual bool ReadEncoded( const char * _FileName, int * _SamplesCount, int * _Channels, int * _SampleRate, int * _BitsPerSample, byte ** _EncodedData, size_t * _EncodedDataLength );

    // Read encoded data from raw memory
    virtual bool ReadEncoded( const char * _FileName, const byte * _Data, size_t _DataLength, int * _SamplesCount, int * _Channels, int * _SampleRate, int * _BitsPerSample, byte ** _EncodedData, size_t * _EncodedDataLength );

protected:
    IAudioDecoderInterface() {}
};
