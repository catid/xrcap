// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/**
 * Memory-mapped files are a fairly good compromise between performance and flexibility.
 * 
 * Compared with asynchronous io, memory-mapped files are:
 *  Much easier to implement in a portable way
 *  Automatically paged in and out of RAM
 *  Automatically read-ahead cached
 *
 * When asynch io is not available or blocking is acceptable then this is a
 * great alternative with low overhead and similar performance.
 * 
 * For random file access, use MappedView with a MappedFile that has been
 * opened with random_access = true.  Random access is usually used for a
 * database-like file type, which is better implemented using scatter/gather.
*/

#pragma once

#include "core.hpp"

#include <atomic>

namespace core {


//------------------------------------------------------------------------------
// Memory-mapped file

struct MappedView;

/// This represents a file on disk that will be mapped
struct MappedFile : NoCopy
{
    friend struct MappedView;

#if defined(_WIN32)
    /*HANDLE*/ void* File = nullptr;
#else
    int File = -1;
#endif

    bool ReadOnly = true;
    uint64_t Length = 0;

    inline bool IsValid() const { return Length != 0; }

    // Opens the file for shared read-only access with other applications
    // Returns false on error (file not found, etc)
    bool OpenRead(
        const char* path,
        bool read_ahead = false,
        bool no_cache = false);

    // Creates and opens the file for exclusive read/write access
    bool OpenWrite(
        const char* path,
        uint64_t size);

    // Resizes a file
    bool Resize(uint64_t size);

    void Close();

    MappedFile();
    ~MappedFile();
};


//------------------------------------------------------------------------------
// MappedView

/// View of a portion of the memory mapped file
struct MappedView : NoCopy
{
    void* Map = nullptr;
    MappedFile* File = nullptr;
    uint8_t* Data = nullptr;
    uint64_t Offset = 0;
    uint32_t Length = 0;

    // Returns false on error
    bool Open(MappedFile* file);

    // Returns 0 on error, 0 length means whole file
    uint8_t* MapView(uint64_t offset = 0, uint32_t length = 0);

    void Close();

    MappedView();
    ~MappedView();
};


//------------------------------------------------------------------------------
// MappedReadOnlySmallFile

/// Convenience wrapper around MappedFile/MappedView for reading small files
struct MappedReadOnlySmallFile : NoCopy
{
    /// Returns true if the file could be read, or false.
    /// File will be kept open until this object goes out of scope or Close()
    bool Read(const char* path);

    /// Release the file early
    void Close();


    CORE_INLINE const uint8_t* GetData()
    {
        return View.Data;
    }
    CORE_INLINE uint32_t GetDataBytes()
    {
        return View.Length;
    }

    // Ordered so that View goes out of scope first:

    MappedFile File;
    MappedView View;
};


//------------------------------------------------------------------------------
// Helpers

/// Write the provided buffer to the file at the given path
bool WriteBufferToFile(const char* path, const void* data, uint64_t bytes);


} // namespace core
