// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <TonkCppSDK.hpp> // tonk
#include <CaptureProtocol.hpp> // capture_protocol
#include "BitField.hpp"

#include <string>
#include <mutex>
#include <atomic>

namespace core {


//------------------------------------------------------------------------------
// CameraOffsetMap

struct CameraOffsetRef;

class CameraOffsetMap
{
public:
    // Allocate the next free offset
    bool Allocate(unsigned count, std::vector<std::shared_ptr<CameraOffsetRef>>& refs);

    // Free an offset
    void Free(int offset);

protected:
    std::mutex Lock;
    CustomBitSet<65536> Used;

    // Zig-zag encoding
    inline uint32_t ZigZagEncode(int32_t offset)
    {
        // Zig-zag encoding to make the signed number positive
        return (offset << 1) ^ (offset >> 31);
    }
    inline int32_t ZigZagDecode(uint32_t zigzag)
    {
        // Undo zig-zag encoding to get signed number
        return (zigzag >> 1) ^ -static_cast<int32_t>(zigzag & 1);
    }
};


//------------------------------------------------------------------------------
// CameraOffsetRef

struct CameraOffsetRef
{
    ~CameraOffsetRef()
    {
        if (Map) {
            Map->Free(Offset);
        }
    }

    int Offset = 0;
    CameraOffsetMap* Map = nullptr;
};


//------------------------------------------------------------------------------
// RendezvousServerConnection

class RendezvousServer;

class RendezvousServerConnection : public tonk::SDKConnection
{
public:
    RendezvousServerConnection(RendezvousServer* server)
        : Server(server)
    {
    }

    bool IsCaptureServer() const
    {
        std::lock_guard<std::mutex> locker(Lock);
        return CaptureServerRegistered;
    }

    std::string GetCaptureServerName() const
    {
        std::lock_guard<std::mutex> locker(Lock);
        return CaptureServerName;
    }

    uint64_t GetGuid() const {
        return Guid;
    }

protected:
    void OnConnect() override;
    void OnData(
        uint32_t          channel,  ///< Channel number attached to each message by sender
        const uint8_t*       data,  ///< Pointer to a buffer containing the message data
        uint32_t            bytes   ///< Number of bytes in the message
    ) override;
    // Ignore unexpected secure data
    // Ignore unexpected Ticks
    void OnClose(
        const tonk::SDKJsonResult& reason
    ) override;

    void OnRegisterCaptureServer(const protos::MessageRegisterCaptureServer& msg);
    void OnConnectName(const protos::MessageConnectName& msg, const uint64_t* guids);
    void OnRequestTDMA(const protos::MessageRequestTDMA& msg);

    void SendConnectResult(protos::ConnectResult result, unsigned server_count = 0);
    void SendAssignTDMA(std::vector<int>& offsets);

private:
    RendezvousServer* Server = nullptr;
    std::string NetLocalName; // Copy of the server name local to net thread

    std::atomic<uint64_t> Guid = ATOMIC_VAR_INIT(0);

    // Members protected by lock
    mutable std::mutex Lock;
    std::string CaptureServerName;
    bool CaptureServerRegistered = false;

    std::vector<std::shared_ptr<CameraOffsetRef>> OffsetRefs;
};


//------------------------------------------------------------------------------
// RendezvousServer

class RendezvousServer : public tonk::SDKSocket
{
public:
    bool Initialize();
    void Shutdown();

    tonk::SDKConnectionList<RendezvousServerConnection> Connections;

    CameraOffsetMap Offsets;

protected:
    virtual tonk::SDKConnection* OnIncomingConnection(
        const TonkAddress& address ///< Address of the client requesting a connection
    );
    virtual tonk::SDKConnection* OnP2PConnectionStart(
        const TonkAddress& address ///< Address of the other peer we are to connect to
    );
};


} // namespace core
