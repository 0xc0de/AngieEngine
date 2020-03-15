/*

Angie Engine Source Code

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

#include <World/Public/Audio/AudioClip.h>
#include <World/Public/Audio/AudioSystem.h>

#include <Core/Public/Alloc.h>
#include <Core/Public/Logger.h>

#include "AudioSystemLocal.h"

#define DEFAULT_BUFFER_SIZE ( 1024 * 32 )

static int ResourceSerialIdGen = 0;

AN_CLASS_META( AAudioClip )

AAudioClip::AAudioClip() {
    BufferSize = DEFAULT_BUFFER_SIZE;
    SerialId = ++ResourceSerialIdGen;
    CurStreamType = StreamType = SOUND_STREAM_DISABLED;
}

AAudioClip::~AAudioClip() {
    Purge();
}

int AAudioClip::GetFrequency() const {
    return Frequency;
}

int AAudioClip::GetBitsPerSample() const {
    return BitsPerSample;
}

int AAudioClip::GetChannels() const {
    return Channels;
}

int AAudioClip::GetSamplesCount() const {
    return SamplesCount;
}

float AAudioClip::GetDurationInSecounds() const {
    return DurationInSeconds;
}

ESoundStreamType AAudioClip::GetStreamType() const {
    return CurStreamType;
}

void AAudioClip::SetBufferSize( int _BufferSize ) {
    BufferSize = Math::Clamp< int >( _BufferSize, AUDIO_MIN_PCM_BUFFER_SIZE, AUDIO_MAX_PCM_BUFFER_SIZE );
}

int AAudioClip::GetBufferSize() const {
    return BufferSize;
}

void AAudioClip::LoadInternalResource( const char * _Path ) {
    Purge();

    // TODO: ...
}

bool AAudioClip::LoadResource( AString const & _Path ) {
    Purge();

    AN_ASSERT( BufferHandle == 0 );

    FileName = _Path;

    Decoder = GAudioSystem.FindDecoder( _Path.CStr() );
    if ( !Decoder ) {
        return false;
    }

    CurStreamType = StreamType;

    switch ( CurStreamType ) {
    case SOUND_STREAM_DISABLED:
    {
        short * PCM;
        if ( !Decoder->DecodePCM( _Path.CStr(), &SamplesCount, &Channels, &Frequency, &BitsPerSample, &PCM ) ) {
            return false;
        }

        AN_ASSERT( SamplesCount > 0 );

        SAudioBufferUpload upload = {};
        upload.SamplesCount = SamplesCount;
        upload.BitsPerSample = BitsPerSample;
        upload.Frequency = Frequency;
        upload.PCM = PCM;
        upload.bStereo = Channels == 2;
        BufferHandle = CreateAudioBuffer( &upload );

        GZoneMemory.Free( PCM );
        break;
    }
    case SOUND_STREAM_FILE:
    {
        if ( !Decoder->DecodePCM( _Path.CStr(), &SamplesCount, &Channels, &Frequency, &BitsPerSample, nullptr ) ) {
            return false;
        }
        break;
    }
    case SOUND_STREAM_MEMORY:
    {
        if ( !Decoder->ReadEncoded( _Path.CStr(), &SamplesCount, &Channels, &Frequency, &BitsPerSample, &EncodedData, &EncodedDataLength ) ) {
            return false;
        }
        break;
    }
    default:
        AN_ASSERT(0);
        return false;
    }

    bLoaded = true;
    DurationInSeconds = (float)SamplesCount / Frequency;

    return true;
}


bool AAudioClip::InitializeFromData( const char * _Path, IAudioDecoderInterface * _Decoder, const byte * _Data, size_t _DataLength ) {
    Purge();

    AN_ASSERT( BufferHandle == 0 );

    FileName = _Path;

    Decoder = _Decoder;
    if ( !Decoder ) {
        return false;
    }

    CurStreamType = StreamType;
    if ( CurStreamType == SOUND_STREAM_FILE ) {
        CurStreamType = SOUND_STREAM_MEMORY;

        GLogger.Printf( "Using MemoryStreamed instead of FileStreamed becouse file data is already in memory\n" );
    }

    switch ( CurStreamType ) {
    case SOUND_STREAM_DISABLED:
    {
        short * PCM;
        if ( !Decoder->DecodePCM( _Path, _Data, _DataLength, &SamplesCount, &Channels, &Frequency, &BitsPerSample, &PCM ) ) {
            return false;
        }

        AN_ASSERT( SamplesCount > 0 );

        SAudioBufferUpload upload = {};
        upload.SamplesCount = SamplesCount;
        upload.BitsPerSample = BitsPerSample;
        upload.Frequency = Frequency;
        upload.PCM = PCM;
        upload.bStereo = Channels == 2;
        BufferHandle = CreateAudioBuffer( &upload );

        GZoneMemory.Free( PCM );
        break;
    }
    case SOUND_STREAM_MEMORY:
    {
        if ( !Decoder->ReadEncoded( _Path, _Data, _DataLength, &SamplesCount, &Channels, &Frequency, &BitsPerSample, &EncodedData, &EncodedDataLength ) ) {
            return false;
        }
        break;
    }
    default:
        AN_ASSERT(0);
        return false;
    }

    bLoaded = true;
    DurationInSeconds = (float)SamplesCount / Frequency;

    return true;
}

IAudioStreamInterface * AAudioClip::CreateAudioStreamInstance() {
    IAudioStreamInterface * streamInterface;

    if ( CurStreamType == SOUND_STREAM_DISABLED ) {
        return nullptr;
    }

    streamInterface = Decoder->CreateAudioStream();

    bool bCreateResult = false;

    if ( streamInterface ) {
        if ( CurStreamType == SOUND_STREAM_FILE ) {
            bCreateResult = streamInterface->InitializeFileStream( GetFileName().CStr() );
        } else {
            bCreateResult = streamInterface->InitializeMemoryStream( GetEncodedData(), GetEncodedDataLength() );
        }
    }

    if ( !bCreateResult ) {
        return nullptr;
    }

    return streamInterface;
}

void AAudioClip::Purge() {
    if ( BufferHandle ) {
        DeleteAudioBuffer( BufferHandle );
        BufferHandle = 0;
    }

    GZoneMemory.Free( EncodedData );
    EncodedData = nullptr;

    EncodedDataLength = 0;

    bLoaded = false;

    DurationInSeconds = 0;

    Decoder = nullptr;

    // Mark resource was changed
    SerialId = ++ResourceSerialIdGen;
}
