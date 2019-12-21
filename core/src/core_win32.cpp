// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "core_win32.hpp"

#include <string>
#include <Aclapi.h>
#include <Sddl.h>

#include <sstream>
#include <iomanip>
using namespace std;

//#pragma comment(lib, "advapi32.lib")

namespace core {


//-----------------------------------------------------------------------------
// AutoHandle

AutoHandle::AutoHandle(HANDLE handle) :
    TheHandle(handle)
{
}

AutoHandle::~AutoHandle()
{
    Clear();
}

AutoHandle& AutoHandle::operator=(HANDLE handle)
{
    Clear();
    TheHandle = handle;
    return *this;
}

void AutoHandle::Clear()
{
    if (TheHandle)
    {
        ::CloseHandle(TheHandle);
        TheHandle = INVALID_HANDLE_VALUE;
    }
}


//-----------------------------------------------------------------------------
// AutoEvent

AutoEvent::AutoEvent(HANDLE handle) :
    TheHandle(handle)
{
}

AutoEvent::~AutoEvent()
{
    Clear();
}

AutoEvent& AutoEvent::operator=(HANDLE handle)
{
    Clear();
    TheHandle = handle;
    return *this;
}

void AutoEvent::Clear()
{
    if (TheHandle)
    {
        ::CloseHandle(TheHandle);
        TheHandle = nullptr;
    }
}


//-----------------------------------------------------------------------------
// SharedMemoryFile

SharedMemoryFile::~SharedMemoryFile()
{
    Close();
}

void SharedMemoryFile::Close()
{
    if (Front)
    {
        ::UnmapViewOfFile(Front);
        Front = nullptr;
    }

    File.Clear();
}

bool SharedMemoryFile::Create(int fileBytes, const std::string& filename)
{
    Close();

    FileSizeBytes = fileBytes;
    if (fileBytes <= 0)
    {
        CORE_DEBUG_BREAK(); // Invalid input
        return false;
    }

    std::vector<uint8_t> desc(SECURITY_DESCRIPTOR_MIN_LENGTH);
    SECURITY_ATTRIBUTES sa;
    sa.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)desc.data();
    InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    // ACL is set as NULL in order to allow all access to the object.
    SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, NULL, FALSE);
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;

    // Create the file
    File = ::CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        &sa,
        PAGE_READWRITE,
        0,
        fileBytes,
        filename.c_str());

    if (File.Invalid()) {
        return false;
    }

    // Map file to memory
    return mapFile();
}

bool SharedMemoryFile::Open(int fileBytes, const std::string& filename)
{
    Close();

    FileSizeBytes = fileBytes;
    if (fileBytes <= 0)
    {
        CORE_DEBUG_BREAK(); // Invalid input
        return false;
    }

    // Open file mapping
    File = ::OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, TRUE, filename.c_str());

    if (File.Invalid())
    {
        std::string str = "SharedMemoryFile: OpenFileMappingA failed for '";
        str += filename;
        str += "' err=";
        str += std::to_string(GetLastError());
        str += "'\n";
        ::OutputDebugStringA(str.c_str());
        return false;
    }

    // Map file to memory
    return mapFile();
}

bool SharedMemoryFile::mapFile()
{
    // Map file to memory
    Front = reinterpret_cast<uint8_t*>(::MapViewOfFile(
        File.Get(),
        FILE_MAP_READ | FILE_MAP_WRITE,
        0, // offset = 0
        0, // offset = 0
        FileSizeBytes));

    if (!Front)
    {
        std::string str = "SharedMemoryFile: MapViewOfFile failed err=";
        str += std::to_string(GetLastError());
        str += "'\n";
        ::OutputDebugStringA(str.c_str());
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
// Module Tools

std::string GetFullFilePathFromRelative(const char* libraryFileNameWithExt)
{
    // Get current module handle
    HMODULE hModule;
    if (!::GetModuleHandleExA(
        (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),
        (LPCSTR)(uintptr_t)&GetFullFilePathFromRelative, &hModule))
    {
        return libraryFileNameWithExt;
    }

    // Get current module file name
    static const DWORD kModulePathChars = 2000;
    char path[kModulePathChars];
    DWORD length = ::GetModuleFileNameA(hModule, path, kModulePathChars);
    if (length <= 0 || length >= kModulePathChars) {
        return libraryFileNameWithExt;
    }

    // Find first slash in the path
    for (DWORD ii = length - 1; ii > 0; --ii)
    {
        if (path[ii] == '\\' || path[ii] == '/')
        {
            // Append to the path
            path[ii + 1] = '\0';
            return std::string(path) + libraryFileNameWithExt;
        }
    }

    return libraryFileNameWithExt;
}


//-----------------------------------------------------------------------------
// Error Tools

std::string WindowsErrorString(DWORD code)
{
    ostringstream oss;
    oss << "0x" << hex << code;
    oss << "(" << dec << code << ") ";

    char* text = nullptr;
    DWORD len = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        code,
        0,
        (LPSTR)&text,
        (DWORD)16,
        nullptr);
    if (len > 0) {
        if (text[len - 1] == '\r' || text[len - 1] == '\n') {
            text[len - 1] = '\0';
            --len;
        }
        if (text[len - 1] == '\r' || text[len - 1] == '\n') {
            text[len - 1] = '\0';
            --len;
        }
        oss << text;
        ::LocalFree(text);
    }

    return oss.str();
}


} // namespace core
