// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <list>

#include <core_logging.hpp> // core
#include <TonkCppSDK.hpp> // tonk
#include <CaptureProtocol.hpp> // capture_protocol
#include <CaptureManager.hpp> // capture
#include <crypto_spake.h> // sodium

namespace core {


//------------------------------------------------------------------------------
// Constants

// Maximum number of video clips to send before we start dropping some
static const int kMaxQueuedVideoSends = 3;


//------------------------------------------------------------------------------
// ViewerConnection

class CaptureServer;

class ViewerConnection : public tonk::SDKConnection
{
public:
    ViewerConnection(CaptureServer* server, CaptureManager* capture)
        : Server(server)
        , Capture(capture)
    {
    }

    bool IsAuthenticated() const
    {
        return Authenticated;
    }

    // Epoch for capture configuration data this connection has been provided
    std::atomic<uint32_t> CaptureConfigEpoch = ATOMIC_VAR_INIT(0);

    // Epoch for extrinsics data this connection has been provided
    std::atomic<uint32_t> ExtrinsicsConfigEpoch = ATOMIC_VAR_INIT(0);

    // Epoch for video info when compression parameters change
    std::atomic<uint32_t> VideoInfoEpoch = ATOMIC_VAR_INIT(0);

    void SendCalibration(
        unsigned camera,
        const core::CameraCalibration& calibration);
    void SendExtrinsics(
        unsigned camera,
        const protos::CameraExtrinsics& extrinsics);
    void SendVideoInfo(protos::MessageVideoInfo& info);
    void QueueBatch(std::shared_ptr<ImageBatch> batch);

protected:
    void OnConnect() override;
    void OnData(
        uint32_t          channel,  ///< Channel number attached to each message by sender
        const uint8_t*       data,  ///< Pointer to a buffer containing the message data
        uint32_t            bytes   ///< Number of bytes in the message
    ) override;
    void OnSecureData(
        uint32_t          channel,  ///< Channel number attached to each message by sender
        const uint8_t*       data,  ///< Pointer to a buffer containing the message data
        uint32_t            bytes   ///< Number of bytes in the message
    ) override;
    void OnTick(
        uint64_t          nowUsec   ///< Current timestamp in microseconds
    ) override;
    void OnClose(
        const tonk::SDKJsonResult& reason
    ) override;

    void OnConnectName(const protos::MessageConnectName& msg);
    void OnAuthClientReply(const protos::MessageAuthClientReply& msg);
    void OnAuthClientProof(const protos::MessageAuthClientProof& msg);
    void OnSetCompression(const protos::MessageSetCompression& msg);
    void OnSetMode(const protos::MessageSetMode& msg);
    void OnSetExposure(const protos::MessageSetExposure& msg);
    void OnSetClip(const protos::MessageSetClip& msg);
    void OnSetLighting(const protos::MessageSetLighting& msg);
    void OnExtrinsics(const protos::MessageExtrinsics& msg);
    void OnRequestKeyframe();

    void SendAuthServerHello(protos::MessageAuthServerHello& msg);
    void SendAuthServerProof(protos::MessageAuthServerProof& msg);
    void SendConnectResult(protos::ConnectResult cr, uint64_t guid);
    void SendAuthResult(protos::AuthResult ar);

    void SendBatch(std::shared_ptr<ImageBatch> batch);

private:
    CaptureServer* Server = nullptr;
    CaptureManager* Capture = nullptr;
    std::string NetLocalName;

    // Last time the camera status was sent
    uint64_t LastUpdateUsec = 0;

    // PAKE state
    bool StartedAuthSteps = false;
    crypto_spake_server_state Pake{};
    crypto_spake_shared_keys SharedKeys{};
    int AuthReplyCount = 0;
    int AuthClientProofCount = 0;

    std::atomic<bool> Authenticated = ATOMIC_VAR_INIT(false);

    std::mutex BatchesLock;
    std::list<std::shared_ptr<ImageBatch>> Batches;
};


//------------------------------------------------------------------------------
// RendezvousConnection

class CaptureServer;

class RendezvousConnection : public tonk::SDKConnection
{
public:
    RendezvousConnection(CaptureManager* capture, CaptureServer* server)
        : Capture(capture)
        , Server(server)
    {
    }

    void SendRequestTDMA(unsigned camera_count);

protected:
    void OnConnect() override;
    void OnData(
        uint32_t          channel,  ///< Channel number attached to each message by sender
        const uint8_t*       data,  ///< Pointer to a buffer containing the message data
        uint32_t            bytes   ///< Number of bytes in the message
    ) override;
    // Ignore secure data (unexpected)
    // Ignore Ticks
    void OnClose(
        const tonk::SDKJsonResult& reason
    ) override;

    void OnAssignTDMA(const protos::MessageAssignTDMA& msg, const int16_t* offsets);

private:
    CaptureManager* Capture = nullptr;
    CaptureServer* Server = nullptr;
    std::string NetLocalName;
};


//------------------------------------------------------------------------------
// CaptureServer

class CaptureServer : public tonk::SDKSocket
{
public:
    bool Initialize(
        CaptureManager* capture,
        const std::string& server_name,
        const std::string& rendezvous_server_host,
        int rendezvous_server_port,
        const std::string& stored_data_base64,
        int port,
        bool enable_multi_server);
    void Shutdown();

    std::string GetServerName() const
    {
        return CaptureServerName;
    }
    const uint8_t* GetPakeStoredData() const
    {
        return StoredData;
    }
    uint64_t GetGuid() const
    {
        return Guid;
    }

    tonk::SDKConnectionList<ViewerConnection> Connections;

    void OnRendezvousClose();

    void BroadcastVideo(std::shared_ptr<ImageBatch>& batch);

protected:
    virtual tonk::SDKConnection* OnIncomingConnection(
        const TonkAddress& address ///< Address of the client requesting a connection
    );
    virtual tonk::SDKConnection* OnP2PConnectionStart(
        const TonkAddress& address ///< Address of the other peer we are to connect to
    );

private:
    CaptureManager* Capture = nullptr;
    std::string CaptureServerName;
    std::string RendezvousServerHostname;
    int RendezvousServerPort = 0;
    uint64_t Guid = 0;
    bool EnableMultiServer = false;

    // PAKE data
    uint8_t StoredData[crypto_spake_STOREDBYTES];

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;
    uint64_t LastTickUsec = 0;

    uint64_t LastTdmaRequestUsec = 0;

    std::mutex RendezvousLock;
    std::shared_ptr<RendezvousConnection> Rendezvous;

    // Background thread that sends video data to clients
    WorkerQueue Worker;

    void Loop();
    void Tick();
};


} // namespace core
