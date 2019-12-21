// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "capture_client.h" // C API
#include "CaptureDecoder.hpp"
#include "DejitterQueue.hpp"

#include <core_logging.hpp> // core library
#include <TonkCppSDK.hpp>
#include <crypto_spake.h> // sodium

#include <mutex>
#include <thread>

namespace core {


//------------------------------------------------------------------------------
// Constants

static const int kMaxQueuedBatchParsing = 3;


//------------------------------------------------------------------------------
// CaptureConnection

class NetClient;

class CaptureConnection : public tonk::SDKConnection
{
public:
    CaptureConnection(NetClient* client)
        : Client(client)
    {
    }

    // API functions
    void SendSetCompression(protos::MessageSetCompression& compression);
    void SendSetMode(protos::Modes mode);
    void SendKeyframeRequest();
    void SetExposure(
        int32_t auto_enabled,
        uint32_t exposure_usec,
        uint32_t awb_usec);
    void SetClip(
        int32_t enabled,
        float radius_meters,
        float floor_meters,
        float ceiling_meters);
    void SetExtrinsics(
        uint32_t camera_index,
        const protos::CameraExtrinsics& extrinsics);
    void SetCompression(const protos::CompressionSettings& compression);
    void SetLighting(
        uint32_t camera_index,
        float brightness,
        float saturation);

    std::atomic<bool> IsAuthenticated = ATOMIC_VAR_INIT(false);
    std::atomic<uint64_t> ServerGuid = ATOMIC_VAR_INIT(0);

    protos::MessageStatus GetLastStatus() const
    {
        std::lock_guard<std::mutex> locker(StatusLock);
        return LastStatus;
    }

    std::atomic<XrcapStreamState> State = ATOMIC_VAR_INIT(XrcapStreamState_Idle);

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

    void OnConnectResult(const protos::MessageConnectResult& msg);
    void OnAuthServerHello(const protos::MessageAuthServerHello& msg);
    void OnAuthServerProof(const protos::MessageAuthServerProof& msg);
    void OnAuthResult(const protos::MessageAuthResult& msg);
    void OnStatus(const protos::MessageStatus& msg);
    void OnCalibration(const protos::MessageCalibration& msg);
    void OnExtrinsics(const protos::MessageExtrinsics& msg);
    void OnVideoInfo(const protos::MessageVideoInfo& msg);
    void OnBatchInfo(const protos::MessageBatchInfo& msg);
    void OnFrameHeader(const protos::MessageFrameHeader& msg);
    void OnImageData(const uint8_t* data, int bytes);
    void OnDepthData(const uint8_t* data, int bytes);

    void SendConnectName(const std::string& name);

private:
    NetClient* Client = nullptr;
    std::string NetLocalName;

    mutable std::mutex StatusLock;
    protos::MessageStatus LastStatus{};

    bool IsDirect = false;

    uint64_t LastConnectRequestUsec = 0;

    bool NeedsToSendConnectName = true;

    crypto_spake_client_state Pake{};
    crypto_spake_shared_keys SharedKeys{};
    int AuthServerHelloCount = 0;
    int AuthServerProofCount = 0;
    int AuthResultCount = 0;

    std::shared_ptr<core::CameraCalibration> Calibration[protos::kMaxCameras];
    std::shared_ptr<protos::CameraExtrinsics> Extrinsics[protos::kMaxCameras];
    std::shared_ptr<FrameInfo> Frame;
    std::shared_ptr<protos::MessageBatchInfo> BatchInfo;
    std::shared_ptr<protos::MessageVideoInfo> VideoInfo;

    // One decoder for each camera in the received batch
    std::vector<std::shared_ptr<DecoderPipeline>> Decoders;

    void OnFrame(std::shared_ptr<FrameInfo> frame);

    void ResetMapped();
};


//------------------------------------------------------------------------------
// NetClient

class NetClient : protected tonk::SDKSocket
{
public:
    bool Initialize(
        std::shared_ptr<DejitterQueue> playback_queue,
        const char* server_address,
        int32_t server_port,
        const char* server_name,
        const char* password);
    void Shutdown();

    std::string GetServerName() const
    {
        return ServerName;
    }
    std::string GetPassword() const
    {
        return Password;
    }

    void OnConnect(CaptureConnection* connection);
    void OnConnectionClosed(CaptureConnection* connection);

    // Returns false if connection should be denied
    bool CheckDirectConnectUnique(CaptureConnection* connection);

    tonk::SDKConnectionList<CaptureConnection> Connections;

    std::shared_ptr<DejitterQueue> PlaybackQueue;

protected:
    virtual tonk::SDKConnection* OnIncomingConnection(
        const TonkAddress& address ///< Address of the client requesting a connection
    );
    virtual tonk::SDKConnection* OnP2PConnectionStart(
        const TonkAddress& address ///< Address of the other peer we are to connect to
    );

private:
    std::string ServerAddress;
    uint16_t ServerPort;
    std::string ServerName;
    std::string Password;

    std::mutex Lock;

    // This is the connection that we initiate ourselves
    std::shared_ptr<CaptureConnection> PrimaryConnection;

    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    void Loop();
};


} // namespace core
