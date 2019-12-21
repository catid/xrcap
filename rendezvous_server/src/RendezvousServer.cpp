// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "RendezvousServer.hpp"
#include <core_logging.hpp> // core
#include <core_string.hpp>

namespace core {


//------------------------------------------------------------------------------
// CameraOffsetMap

bool CameraOffsetMap::Allocate(unsigned count, std::vector<std::shared_ptr<CameraOffsetRef>>& refs)
{
    refs.clear();

    std::lock_guard<std::mutex> locker(Lock);

    unsigned search_offset = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned zigzag = Used.FindFirstClear(search_offset);
        if (zigzag >= Used.kValidBits) {
            return false;
        }
        Used.Set(zigzag);
        search_offset = zigzag + 1;

        const int offset = ZigZagDecode(zigzag);

        std::shared_ptr<CameraOffsetRef> ref = std::make_shared<CameraOffsetRef>();
        ref->Offset = offset;
        ref->Map = this;
        refs.push_back(ref);
    }

    return true;
}

void CameraOffsetMap::Free(int offset)
{
    const unsigned zigzag = ZigZagEncode(offset);
    if (zigzag >= Used.kValidBits) {
        return;
    }
    std::lock_guard<std::mutex> locker(Lock);
    Used.Clear(zigzag);
}


//------------------------------------------------------------------------------
// RendezvousServerConnection

void RendezvousServerConnection::OnConnect()
{
    const TonkStatusEx status = GetStatusEx();
    NetLocalName = fmt::format("[Peer {}:{}]", status.Remote.NetworkString, status.Remote.UDPPort);
    spdlog::info("{} Peer connected", NetLocalName);
}

void RendezvousServerConnection::OnData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (channel != protos::kChannelRendezvous) {
        spdlog::error("{} Non-rendezvous message ignored from client", NetLocalName);
        return;
    }
    if (bytes <= 0) {
        spdlog::error("{} Empty message from client", NetLocalName);
        return;
    }

    switch (data[0])
    {
    case protos::MessageType_RegisterCaptureServer:
        if (bytes == sizeof(protos::MessageRegisterCaptureServer)) {
            OnRegisterCaptureServer(*reinterpret_cast<const protos::MessageRegisterCaptureServer*>( data ));
        }
        break;
    case protos::MessageType_RequestTDMA:
        if (bytes == sizeof(protos::MessageRequestTDMA)) {
            OnRequestTDMA(*reinterpret_cast<const protos::MessageRequestTDMA*>( data ));
        }
        break;
    case protos::MessageType_ConnectName:
        if (bytes >= sizeof(protos::MessageConnectName)) {
            const protos::MessageConnectName* name = reinterpret_cast<const protos::MessageConnectName*>( data );
            if (bytes == sizeof(protos::MessageConnectName) + sizeof(uint64_t) * name->IgnoreGuidCount) {
                OnConnectName(*name, reinterpret_cast<const uint64_t*>( data + sizeof(protos::MessageConnectName) ));
            }
        }
        break;
    default:
        spdlog::error("{} Unsupported message from client", NetLocalName);
        return;
    }
}

void RendezvousServerConnection::OnClose(
    const tonk::SDKJsonResult& reason
)
{
    spdlog::warn("{} Peer disconnected: {}", NetLocalName, reason.ToString());
    Server->Connections.Remove(this);
}

void RendezvousServerConnection::OnRegisterCaptureServer(
    const protos::MessageRegisterCaptureServer& msg)
{
    {
        std::lock_guard<std::mutex> locker(Lock);
        CaptureServerRegistered = true;
        NetLocalName = CaptureServerName = protos::SanitizeString(msg.Name, sizeof(msg.Name));
    }

    const TonkStatusEx status = GetStatusEx();
    NetLocalName = fmt::format("[Server {}:{}] ({})",
        status.Remote.NetworkString, status.Remote.UDPPort,
        NetLocalName);

    Guid = msg.Guid;

    spdlog::info("{} Capture server registered. GUID={}", NetLocalName, msg.Guid);
}

void RendezvousServerConnection::OnConnectName(
    const protos::MessageConnectName& msg, const uint64_t* guids)
{
    std::string name = protos::SanitizeString(msg.Name, sizeof(msg.Name));
    unsigned count = 0;

    auto connections = Server->Connections.GetList();
    for (auto& connection : connections)
    {
        if (!connection->IsCaptureServer()) {
            continue;
        }

        const std::string other = connection->GetCaptureServerName();
        if (0 != StrCaseCompare(other.c_str(), name.c_str())) {
            continue;
        }

        // Check if client already has this server connected:
        bool already_connected = false;
        for (unsigned i = 0; i < msg.IgnoreGuidCount; ++i) {
            const uint64_t guid = guids[i];
            if (guid == connection->GetGuid()) {
                already_connected = true;
                break;
            }
        }
        if (already_connected) {
            continue;
        }

        const TonkStatusEx server_status = connection->GetStatusEx();
        spdlog::info("{} Connecting server named `{}` at {}:{} to client",
            NetLocalName, name,
            server_status.Remote.NetworkString, server_status.Remote.UDPPort);

        // Start connecting peers:

        tonk::SDKResult result = this->P2PConnect(connection.get());
        if (!result) {
            spdlog::error("{} Failed to connect peers: {}", NetLocalName, result.ToString());
            SendConnectResult(protos::ConnectResult_NotReady);
            return;
        }

        ++count;
    }

    if (count > 0) {
        SendConnectResult(protos::ConnectResult_Connecting, count);
    } else {
        SendConnectResult(protos::ConnectResult_NotFound);
    }
}

void RendezvousServerConnection::OnRequestTDMA(const protos::MessageRequestTDMA& msg)
{
    const unsigned camera_count = msg.CameraCount;
    if (!Server->Offsets.Allocate(camera_count, OffsetRefs)) {
        spdlog::error("{} Allocate failed for {} cameras", NetLocalName, camera_count);
        return;
    }

    std::vector<int> offsets(camera_count);
    for (unsigned camera_index = 0; camera_index < camera_count; ++camera_index) {
        const int offset = OffsetRefs[camera_index]->Offset;
        offsets[camera_index] = offset;
        spdlog::info("{} Assigned TDMA slot {} to camera {}/{}", NetLocalName, offset, camera_index, camera_count);
    }

    SendAssignTDMA(offsets);
}

void RendezvousServerConnection::SendConnectResult(protos::ConnectResult cr, unsigned server_count)
{
    protos::MessageConnectResult reply;
    reply.Result = static_cast<uint8_t>( cr );
    reply.ServerCount = static_cast<uint16_t>( server_count );

    tonk::SDKResult send_result = Send(&reply, sizeof(reply), protos::kChannelRendezvous);
    if (!send_result) {
        spdlog::error("{} Send failed: {}", NetLocalName, send_result.ToString());
    }
}

void RendezvousServerConnection::SendAssignTDMA(std::vector<int>& offsets)
{
    size_t msg_len = sizeof(protos::MessageAssignTDMA) + sizeof(int16_t) * offsets.size();
    std::unique_ptr<uint8_t> msg_ptr(new uint8_t[msg_len]);
    protos::MessageAssignTDMA* msg = reinterpret_cast<protos::MessageAssignTDMA*>(msg_ptr.get());

    msg->Type = protos::MessageType_AssignTDMA;
    msg->CameraCount = static_cast<uint8_t>( offsets.size() );

    int16_t* offsets_ptr = reinterpret_cast<int16_t*>( msg_ptr.get() + sizeof(protos::MessageAssignTDMA) );
    for (size_t i = 0; i < offsets.size(); ++i) {
        offsets_ptr[i] = static_cast<int16_t>( offsets[i] );
    }

    tonk::SDKResult send_result = Send(msg_ptr.get(), msg_len, protos::kChannelRendezvous);
    if (!send_result) {
        spdlog::error("{} SendAssignTDMA status update failed: {}", NetLocalName, send_result.ToString());
    }
}


//------------------------------------------------------------------------------
// RendezvousServer

bool RendezvousServer::Initialize()
{
    const int port = protos::kRendezvousServerPort;

    // Set netcode configuration
    tonk::SDKSocket::Config.UDPListenPort = static_cast<uint32_t>( port );
    tonk::SDKSocket::Config.MaximumClients = 10;
    // TONK_FLAGS_ENABLE_UPNP - UPnP disabled for rendezvous server
    // TONK_FLAGS_DISABLE_COMPRESSION - Compressed allowed
    tonk::SDKSocket::Config.Flags = TONK_FLAGS_DISABLE_CC;
    // 10KB/s bandwidth limit
    tonk::SDKSocket::Config.BandwidthLimitBPS = 10 * 1000;

    tonk::SDKJsonResult result = tonk::SDKSocket::Create();
    if (!result) {
        spdlog::error("Unable to create socket: {}", result.ToString());
        return false;
    }

    return true;
}

void RendezvousServer::Shutdown()
{
    spdlog::info("Shutting down socket...");
    tonk::SDKSocket::BlockingDestroy();
    spdlog::info("..Socket destroyed");
}

tonk::SDKConnection* RendezvousServer::OnIncomingConnection(
    const TonkAddress& address ///< Address of the client requesting a connection
)
{
    CORE_UNUSED(address);

    auto ptr = std::make_shared<RendezvousServerConnection>(this);

    // Insert into connection list to prevent it from going out of scope
    Connections.Insert(ptr.get());

    return ptr.get();
}

tonk::SDKConnection* RendezvousServer::OnP2PConnectionStart(
    const TonkAddress& address ///< Address of the other peer we are to connect to
)
{
    CORE_UNUSED(address);

    return nullptr; // Disabled
}


} // namespace core
