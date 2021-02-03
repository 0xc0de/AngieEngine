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

#include <World/Public/Resource/SoundResource.h>
#include <World/Public/AudioSystem.h>
#include <Audio/AudioDevice.h>

#include <Core/Public/Alloc.h>
#include <Core/Public/Logger.h>

static int RevisionGen = 0;

AN_CLASS_META( ASoundResource )

ASoundResource::ASoundResource()
{
    Revision = ++RevisionGen;

    Core::ZeroMem( &AudioFileInfo, sizeof( AudioFileInfo ) );
}

ASoundResource::~ASoundResource()
{
    Purge();
}

int ASoundResource::GetFrequency() const
{
    AAudioDevice * device = GAudioSystem.GetPlaybackDevice();

    // The frequency always matches the playback system
    return device->GetSampleRate();
}

int ASoundResource::GetSampleBits() const
{
    return AudioFileInfo.SampleBits;
}

int ASoundResource::GetSampleWidth() const
{
    return AudioFileInfo.SampleBits >> 3;
}

int ASoundResource::GetSampleStride() const
{
    return (AudioFileInfo.SampleBits >> 3) << (AudioFileInfo.Channels - 1);
}

int ASoundResource::GetChannels() const
{
    return AudioFileInfo.Channels;
}

int ASoundResource::GetFrameCount() const
{
    return AudioFileInfo.NumFrames;
}

float ASoundResource::GetDurationInSecounds() const
{
    return DurationInSeconds;
}

ESoundStreamType ASoundResource::GetStreamType() const
{
    return CurStreamType;
}

void ASoundResource::LoadInternalResource( const char * _Path )
{
    Purge();

    // TODO: ...
}

bool ASoundResource::LoadResource( IBinaryStream & Stream )
{
    AAudioDevice * device = GAudioSystem.GetPlaybackDevice();

    Purge();

    AN_ASSERT( !RawSamples );

    FileName = Stream.GetFileName();

    Decoder = GAudioSystem.FindAudioDecoder( Stream.GetFileName() );
    if ( !Decoder ) {
        return false;
    }

    CurStreamType = StreamType;

    bool mono = bForceMono || device->GetChannels() == 1;
    int deviceSampleRate = device->GetSampleRate();

    switch ( CurStreamType ) {
    case SOUND_STREAM_DISABLED:
    {
        if ( !Decoder->LoadFromFile( Stream, &AudioFileInfo, deviceSampleRate, mono, bForce8Bit, &RawSamples ) ) {
            return false;
        }

        break;
    }
    case SOUND_STREAM_FILE:
    {
        if ( !Decoder->LoadFromFile( Stream, &AudioFileInfo, deviceSampleRate, mono, bForce8Bit, nullptr ) ) {
            return false;
        }

        break;
    }
    case SOUND_STREAM_MEMORY:
    {
        if ( !Decoder->LoadFromFile( Stream, &AudioFileInfo, deviceSampleRate, mono, bForce8Bit, nullptr ) ) {
            return false;
        }

        Stream.SeekEnd( 0 );

        FileInMemorySize = Stream.Tell();
        FileInMemory = (byte *)GHeapMemory.Alloc( FileInMemorySize );

        Stream.Rewind();
        Stream.ReadBuffer( FileInMemory, FileInMemorySize );

        break;
    }
    default:
        AN_ASSERT(0);
        return false;
    }

    bLoaded = true;
    DurationInSeconds = (float)GetFrameCount() / GetFrequency();

    return true;
}

bool ASoundResource::InitializeFromData( const char * _Path, IAudioDecoder * _Decoder, const byte * _Data, size_t SizeInBytes )
{
    AAudioDevice * device = GAudioSystem.GetPlaybackDevice();

    Purge();

    AN_ASSERT( !RawSamples );

    FileName = _Path;

    Decoder = _Decoder;
    if ( !Decoder ) {
        return false;
    }

    CurStreamType = StreamType;
    if ( CurStreamType == SOUND_STREAM_FILE ) {
        CurStreamType = SOUND_STREAM_MEMORY;

        GLogger.Printf( "Using MemoryStreamed instead of FileStreamed as the file data is already in memory\n" );
    }

    bool mono = bForceMono || device->GetChannels() == 1;
    int deviceSampleRate = device->GetSampleRate();

    switch ( CurStreamType ) {
    case SOUND_STREAM_DISABLED:
    {
        AMemoryStream f;

        if ( !f.OpenRead( _Path, _Data, SizeInBytes ) ) {
            return false;
        }

        if ( !Decoder->LoadFromFile( f, &AudioFileInfo, deviceSampleRate, mono, bForce8Bit, &RawSamples ) ) {
            return false;
        }

        break;
    }
    case SOUND_STREAM_MEMORY:
    {
        AMemoryStream f;

        if ( !f.OpenRead( _Path, _Data, SizeInBytes ) ) {
            return false;
        }

        if ( !Decoder->LoadFromFile( f, &AudioFileInfo, deviceSampleRate, mono, bForce8Bit, nullptr ) ) {
            return false;
        }

        f.SeekEnd( 0 );

        FileInMemorySize = f.Tell();
        FileInMemory = (byte *)GHeapMemory.Alloc( FileInMemorySize );

        f.Rewind();
        f.ReadBuffer( FileInMemory, FileInMemorySize );

        break;
    }
    default:
        AN_ASSERT(0);
        return false;
    }

    bLoaded = true;
    DurationInSeconds = (float)GetFrameCount() / GetFrequency();

    return true;
}

bool ASoundResource::CreateAudioStreamInstance( TRef< IAudioStream > * ppInterface )
{
    if ( CurStreamType == SOUND_STREAM_DISABLED || !Decoder ) {
        return false;
    }

    Decoder->CreateAudioStream( ppInterface );

    bool bCreateResult = false;

    if ( *ppInterface ) {
        if ( CurStreamType == SOUND_STREAM_FILE ) {
            bCreateResult = (*ppInterface)->InitializeFileStream( GetFileName().CStr(),
                                                                  GetFrequency(),
                                                                  GetSampleBits(),
                                                                  GetChannels() );
        } else {
            bCreateResult = (*ppInterface)->InitializeMemoryStream( GetFileName().CStr(),
                                                                    GetFileInMemory(),
                                                                    GetFileInMemorySize(),
                                                                    GetFrequency(),
                                                                    GetSampleBits(),
                                                                    GetChannels() );
        }
    }

    if ( !bCreateResult ) {
        (*ppInterface).Reset();
        return false;
    }

    return (*ppInterface) != nullptr;
}

void ASoundResource::Purge()
{
    GHeapMemory.Free( RawSamples );
    RawSamples = nullptr;

    GHeapMemory.Free( FileInMemory );
    FileInMemory = nullptr;
    FileInMemorySize = 0;

    bLoaded = false;

    DurationInSeconds = 0;

    Decoder = nullptr;

    // Mark resource was changed
    Revision = ++RevisionGen;
}
