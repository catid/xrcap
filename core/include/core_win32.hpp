// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "core.hpp"

#include <string>

#ifndef _WIN32
#error "Header should only be included on Windows"
#endif

// It is common practice to define WIN32_LEAN_AND_MEAN to reduce compile times.
// However this then requires us to define our own NTSTATUS data type and other
// irritations throughout our code-base.
//#define WIN32_LEAN_AND_MEAN

// Prevents <Windows.h> from #including <Winsock.h>, as we use <Winsock2.h> instead.
#ifndef _WINSOCKAPI_
    #define DID_DEFINE_WINSOCKAPI
    #define _WINSOCKAPI_
#endif

// Prevents <Windows.h> from defining min() and max() macro symbols.
#define NOMINMAX

// We support Windows Windows 10 or newer.
#undef WINVER
#define WINVER 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00

#include <windows.h>

#undef NOMINMAX

#ifdef DID_DEFINE_WINSOCKAPI
    #undef _WINSOCKAPI_
    #undef DID_DEFINE_WINSOCKAPI
#endif

namespace core {


//-----------------------------------------------------------------------------
// AutoEvent
//
// Auto-close wrapper for a HANDLE that is invalid when NULL.
// For example, ::OpenProcess() returns NULL on failure.
class AutoEvent
{
public:
    AutoEvent(HANDLE handle = nullptr);
    ~AutoEvent();

    AutoEvent& operator=(HANDLE handle);

    HANDLE Get() const
    {
        return TheHandle;
    }

    bool Valid() const
    {
        return TheHandle != nullptr;
    }
    bool Invalid() const
    {
        return TheHandle == nullptr;
    }

    void Clear();

protected:
    HANDLE TheHandle = nullptr;
};


//-----------------------------------------------------------------------------
// AutoHandle
//
// Auto-close wrapper for a HANDLE that is invalid when INVALID_HANDLE_VALUE.
class AutoHandle
{
public:
    AutoHandle(HANDLE handle = INVALID_HANDLE_VALUE);
    ~AutoHandle();

    AutoHandle& operator=(HANDLE handle);

    HANDLE Get() const
    {
        return TheHandle;
    }

    bool Valid() const
    {
        return TheHandle != INVALID_HANDLE_VALUE;
    }
    bool Invalid() const
    {
        return TheHandle == INVALID_HANDLE_VALUE;
    }

    void Clear();

protected:
    HANDLE TheHandle = INVALID_HANDLE_VALUE;
};


//-----------------------------------------------------------------------------
// SharedMemoryFile

class SharedMemoryFile : NoCopy
{
public:
    ~SharedMemoryFile();

    bool Create(int fileBytes, const std::string& filename);
    bool Open(int fileBytes, const std::string& filename);
    void Close();
    uint8_t* GetFront() const
    {
        return Front;
    }

protected:
    AutoEvent File;
    uint8_t* Front = nullptr;
    int FileSizeBytes = -1;

    // Map file to memory
    bool mapFile();
};


//-----------------------------------------------------------------------------
// Module Tools

/// Get full path to a file living next to the current module
std::string GetFullFilePathFromRelative(const char* libraryFileNameWithExt);


//-----------------------------------------------------------------------------
// Error Tools

/// Convert windows error code to string
std::string WindowsErrorString(DWORD code);


//------------------------------------------------------------------------------
// Mutex

static const unsigned kMutexSpinCount = 1000;

struct CriticalSection
{
    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    CRITICAL_SECTION cs;

    CriticalSection() { ::InitializeCriticalSectionAndSpinCount(&cs, (DWORD)kMutexSpinCount); }
    ~CriticalSection() { ::DeleteCriticalSection(&cs); }
    bool TryEnter() { return ::TryEnterCriticalSection(&cs) != FALSE; }
    void Enter() { ::EnterCriticalSection(&cs); }
    void Leave() { ::LeaveCriticalSection(&cs); }
};

class CriticalLocker
{
public:
    CriticalLocker(CriticalSection& lock) {
        TheLock = &lock;
        if (TheLock)
            TheLock->Enter();
    }
    ~CriticalLocker() { Clear(); }
    bool TrySet(CriticalSection& lock) {
        Clear();
        if (!lock.TryEnter())
            return false;
        TheLock = &lock;
        return true;
    }
    void Set(CriticalSection& lock) {
        Clear();
        lock.Enter();
        TheLock = &lock;
    }
    void Clear() {
        if (TheLock)
            TheLock->Leave();
        TheLock = nullptr;
    }
protected:
    CriticalSection* TheLock;
};


} // namespace core
