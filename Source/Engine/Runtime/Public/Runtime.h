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

#include <Engine/Core/Public/Atomic.h>
#include <Engine/Core/Public/String.h>
#include <Engine/Core/Public/PodQueue.h>
#include <Engine/Core/Public/Utf8.h>
#include "AsyncJobManager.h"

//enum EVerticalSyncMode {
//    VSync_Disabled,
//    VSync_Adaptive,
//    VSync_Fixed,
//    VSync_Half
//};

struct SMonitorVideoMode {
    int Width;
    int Height;
    int RedBits;
    int GreenBits;
    int BlueBits;
    int RefreshRate;
};

#define GAMMA_RAMP_SIZE 1024

struct SPhysicalMonitorInternal {
    void * Pointer;
    unsigned short InitialGammaRamp[ GAMMA_RAMP_SIZE * 3 ];
    unsigned short GammaRamp[ GAMMA_RAMP_SIZE * 3 ];
    bool bGammaRampDirty;
};

struct SPhysicalMonitor {
    char MonitorName[128];
    int PhysicalWidthMM;
    int PhysicalHeightMM;
    float DPI_X;
    float DPI_Y;
    int PositionX;
    int PositionY;
    int GammaRampSize;
    SPhysicalMonitorInternal Internal; // for internal use
    int VideoModesCount;
    SMonitorVideoMode VideoModes[1];
};

struct SJoystick {
    int NumAxes;
    int NumButtons;
    bool bGamePad;
    bool bConnected;
    int Id;
};

/** CPU features */
struct SCPUInfo {
    bool OS_AVX : 1;
    bool OS_AVX512 : 1;
    bool OS_64bit : 1;

    bool Intel : 1;
    bool AMD : 1;

    // Simd 128 bit
    bool SSE : 1;
    bool SSE2 : 1;
    bool SSE3 : 1;
    bool SSSE3 : 1;
    bool SSE41 : 1;
    bool SSE42 : 1;
    bool SSE4a : 1;
    bool AES : 1;
    bool SHA : 1;

    // Simd 256 bit
    bool AVX : 1;
    bool XOP : 1;
    bool FMA3 : 1;
    bool FMA4 : 1;
    bool AVX2 : 1;

    // Simd 512 bit
    bool AVX512_F : 1;
    bool AVX512_CD : 1;
    bool AVX512_PF : 1;
    bool AVX512_ER : 1;
    bool AVX512_VL : 1;
    bool AVX512_BW : 1;
    bool AVX512_DQ : 1;
    bool AVX512_IFMA : 1;
    bool AVX512_VBMI : 1;

    // Features
    bool x64 : 1;
    bool ABM : 1;
    bool MMX : 1;
    bool RDRAND : 1;
    bool BMI1 : 1;
    bool BMI2 : 1;
    bool ADX : 1;
    bool MPX : 1;
    bool PREFETCHWT1 : 1;
};

enum EEventType {
    ET_Unknown,
    ET_RuntimeUpdateEvent,
    ET_KeyEvent,
    ET_MouseButtonEvent,
    ET_MouseWheelEvent,
    ET_MouseMoveEvent,
    ET_JoystickAxisEvent,
    ET_JoystickButtonEvent,
    ET_JoystickStateEvent,
    ET_CharEvent,
    ET_MonitorConnectionEvent,
    ET_CloseEvent,
    ET_FocusEvent,
    ET_VisibleEvent,
    ET_WindowPosEvent,
    ET_ChangedVideoModeEvent,

    ET_SetVideoModeEvent,
    ET_SetWindowDefsEvent,
    ET_SetWindowPosEvent,
    ET_SetInputFocusEvent,
    //ET_SetRenderFeaturesEvent,
    ET_SetCursorModeEvent
};

enum EInputEvent {
    IE_Release,
    IE_Press,
    IE_Repeat
};

struct SRuntimeUpdateEvent {
    int InputEventCount;
};

struct SKeyEvent {
    int Key;
    int Scancode;       // Not used, reserved for future
    int ModMask;
    int Action;         // EInputEvent
};

struct SMouseButtonEvent {
    int Button;
    int ModMask;
    int Action;         // EInputEvent
};

struct SMouseWheelEvent {
    double WheelX;
    double WheelY;
};

struct SMouseMoveEvent {
    float X;
    float Y;
};

struct SJoystickAxisEvent {
    int Joystick;
    int Axis;
    float Value;
};

struct SJoystickButtonEvent {
    int Joystick;
    int Button;
    int Action;         // EInputEvent
};

struct SJoystickStateEvent {
    int Joystick;
    int NumAxes;
    int NumButtons;
    bool bGamePad;
    bool bConnected;
};

struct SCharEvent {
    FWideChar UnicodeCharacter;
    int ModMask;
};

struct SMonitorConnectionEvent {
    int Handle;
    bool bConnected;
};

struct SCloseEvent {
};

struct SFocusEvent {
    bool bFocused;
};

struct SVisibleEvent {
    bool bVisible;
};

struct SWindowPosEvent {
    int PositionX;
    int PositionY;
};

struct SChangedVideoModeEvent {
    unsigned short Width;
    unsigned short Height;
    unsigned short PhysicalMonitor;
    byte RefreshRate;
    bool bFullscreen;
    char Backend[32];
};

struct SSetVideoModeEvent {
    unsigned short Width;
    unsigned short Height;
    unsigned short PhysicalMonitor;
    byte RefreshRate;
    bool bFullscreen;
    char Backend[32];
};

struct SSetWindowDefsEvent {
    byte Opacity;
    bool bDecorated;
    bool bAutoIconify;
    bool bFloating;
    char Title[32];
};

struct SSetWindowPosEvent {
    int PositionX;
    int PositionY;
};

struct SSetInputFocusEvent {

};

//struct SSetRenderFeaturesEvent {
//    int VSyncMode;
//};

struct SSetCursorModeEvent {
    bool bDisabledCursor;
};

struct SEvent {
    int Type;
    double TimeStamp;   // in seconds

    union {
        // Runtime output events
        SRuntimeUpdateEvent RuntimeUpdateEvent;
        SKeyEvent KeyEvent;
        SMouseButtonEvent MouseButtonEvent;
        SMouseWheelEvent MouseWheelEvent;
        SMouseMoveEvent MouseMoveEvent;
        SCharEvent CharEvent;
        SJoystickAxisEvent JoystickAxisEvent;
        SJoystickButtonEvent JoystickButtonEvent;
        SJoystickStateEvent JoystickStateEvent;
        SMonitorConnectionEvent MonitorConnectionEvent;
        SCloseEvent CloseEvent;
        SFocusEvent FocusEvent;
        SVisibleEvent VisibleEvent;
        SWindowPosEvent WindowPosEvent;
        SChangedVideoModeEvent ChangedVideoModeEvent;

        // Runtime input events
        SSetVideoModeEvent SetVideoModeEvent;
        SSetWindowDefsEvent SetWindowDefsEvent;
        SSetWindowPosEvent SetWindowPosEvent;
        SSetInputFocusEvent SetInputFocusEvent;
        //SSetRenderFeaturesEvent SetRenderFeaturesEvent;
        SSetCursorModeEvent SetCursorModeEvent;
    } Data;
};

using AEventQueue = TPodQueue< SEvent >;

enum
{
    RENDER_FRONTEND_JOB_LIST,
    RENDER_BACKEND_JOB_LIST,
    MAX_RUNTIME_JOB_LISTS
};

struct SRenderFrame;

/** Runtime public class */
class ANGIE_API ARuntime
{
    AN_SINGLETON( ARuntime )

public:
    /** Is cheats allowed for the game. This allow to change runtime variables with flag VAR_CHEAT */
    bool bCheatsAllowed = true;

    /** Is game server. This allow to change runtime variables with flag VAR_SERVERONLY */
    bool bServerActive = false;

    /** Is in game. This blocks changing runtime variables with flag VAR_NOINGAME */
    bool bInGameStatus = false;

    /** Application command line args count */
    int GetArgc();

    /** Application command line args */
    const char * const *GetArgv();

    /** Check is argument exists in application command line. Return argument index or -1 if argument was not found. */
    int CheckArg( const char * _Arg );

    /** Return application working directory */
    AString const & GetWorkingDir();

    /** Return application exacutable name */
    const char * GetExecutableName();

    /** Return event queue for reading */
    AEventQueue * ReadEvents_GameThread();

    /** Return event queue for writing */
    AEventQueue * WriteEvents_GameThread();

    /** Get render frame data */
    SRenderFrame * GetFrameData();

    /** Allocate frame memory */
    void * AllocFrameMem( size_t _SizeInBytes );

    /** Return frame memory size in bytes */
    size_t GetFrameMemorySize() const;

    /** Return used frame memory in bytes */
    size_t GetFrameMemoryUsed() const;

    /** Return used frame memory on previous frame, in bytes */
    size_t GetFrameMemoryUsedPrev() const;

    // Is vertical synchronization control supported
    //bool IsVSyncSupported();

    // Is vertical synchronization tearing supported
    //bool IsAdaptiveVSyncSupported();

    /** Get CPU info */
    SCPUInfo const & GetCPUInfo() const;

    /** Get physical monitors count */
    int GetPhysicalMonitorsCount();

    /** Return primary monitor */
    SPhysicalMonitor const * GetPrimaryMonitor();

    /** Get physical monitor by handle */
    SPhysicalMonitor const * GetMonitor( int _Handle );

    /** Get physical monitor by name */
    SPhysicalMonitor const * GetMonitor( const char * _MonitorName );

    /** Check is monitor connected */
    bool IsMonitorConnected( int _Handle );

    /** Change monitor gamma */
    void SetMonitorGammaCurve( int _Handle, float _Gamma );

    /** Change monitor gamma */
    void SetMonitorGamma( int _Handle, float _Gamma );

    /** Change monitor gamma */
    void SetMonitorGammaRamp( int _Handle, const unsigned short * _GammaRamp );

    /** Get current monitor gamma */
    void GetMonitorGammaRamp( int _Handle, unsigned short * _GammaRamp, int & _GammaRampSize );

    /** Restore original monitor gamma */
    void RestoreMonitorGamma( int _Handle );

    /** Get joystick name by handle */
    AString GetJoystickName( int _Joystick );

    /** Get joystick name */
    AString GetJoystickName( const SJoystick * _Joystick ) { return GetJoystickName( _Joystick->Id ); }

    /** Sleep current thread */
    void WaitSeconds( int _Seconds );

    /** Sleep current thread */
    void WaitMilliseconds( int _Milliseconds );

    /** Sleep current thread */
    void WaitMicroseconds( int _Microseconds );

    /** Get current time in seconds since application start */
    int64_t SysSeconds();

    /** Get current time in seconds since application start */
    double SysSeconds_d();

    /** Get current time in milliseconds since application start */
    int64_t SysMilliseconds();

    /** Get current time in milliseconds since application start */
    double SysMilliseconds_d();

    /** Get current time in microseconds since application start */
    int64_t SysMicroseconds();

    /** Get current time in microseconds since application start */
    double SysMicroseconds_d();

    /** Get time stamp at beggining of the frame */
    int64_t SysFrameTimeStamp();

    /** Load dynamic library (.dll or .so) */
    void * LoadDynamicLib( const char * _LibraryName );

    /** Unload dynamic library (.dll or .so) */
    void UnloadDynamicLib( void * _Handle );

    /** Get address of procedure in dynamic library */
    void * GetProcAddress( void * _Handle, const char * _ProcName );

    /** Helper. Get address of procedure in dynamic library */
    template< typename T >
    bool GetProcAddress( void * _Handle, T ** _ProcPtr, const char * _ProcName ) {
        return nullptr != ( (*_ProcPtr) = (T *)GetProcAddress( _Handle, _ProcName ) );
    }

    /** Set clipboard text */
    void SetClipboard( const char * _Utf8String );

    /** Set clipboard text */
    void SetClipboard( AString const & _Clipboard ) { SetClipboard( _Clipboard.CStr() ); }

    /** Get clipboard text */
    const char * GetClipboard();

    /** Terminate the application */
    void PostTerminateEvent();
};

extern ANGIE_API ARuntime & GRuntime;
extern ANGIE_API AAsyncJobManager GAsyncJobManager;
extern ANGIE_API AAsyncJobList * GRenderFrontendJobList;
extern ANGIE_API AAsyncJobList * GRenderBackendJobList;
