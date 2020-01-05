// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureServer.hpp"

#include <core_string.hpp>
#include <core_logging.hpp>

#include <sodium.h>
#include <xxhash.h>

namespace core {


//------------------------------------------------------------------------------
// Tools

static uint8_t CaptureModeToCode(CaptureMode mode)
{
    static_assert((int)CaptureMode::Count == 4, "Update this");
    switch (mode)
    {
    case CaptureMode::Disabled:
        return static_cast<uint8_t>( protos::Mode_Disabled );
    case CaptureMode::Calibration:
        return static_cast<uint8_t>( protos::Mode_Calibration );
    case CaptureMode::CaptureLowQual:
        return static_cast<uint8_t>( protos::Mode_CaptureLowQual );
    case CaptureMode::CaptureHighQual:
        return static_cast<uint8_t>( protos::Mode_CaptureHighQual );
    default:
        break;
    }
    spdlog::error("FIXME: Invalid capture mode code");
    return static_cast<uint8_t>( protos::Mode_Disabled );
}

static uint8_t CaptureStatusToCode(CaptureStatus status)
{
    static_assert((int)CaptureStatus::Count == 7, "Update this");
    switch (status)
    {
    case CaptureStatus::Idle:
        return static_cast<uint8_t>( protos::StatusCode_Idle );
    case CaptureStatus::Initializing:
        return static_cast<uint8_t>( protos::StatusCode_Initializing );
    case CaptureStatus::Capturing:
        return static_cast<uint8_t>( protos::StatusCode_Capturing );
    case CaptureStatus::NoCameras:
        return static_cast<uint8_t>( protos::StatusCode_NoCameras );
    case CaptureStatus::BadUsbConnection:
        return static_cast<uint8_t>( protos::StatusCode_BadUsbConnection );
    case CaptureStatus::FirmwareVersionMismatch:
        return static_cast<uint8_t>( protos::StatusCode_FirmwareVersionMismatch );
    case CaptureStatus::SyncCableMisconfigured:
        return static_cast<uint8_t>( protos::StatusCode_SyncCableMisconfigured );
    default:
        break;
    }
    spdlog::error("FIXME: Invalid capture status code");
    return static_cast<uint8_t>( protos::StatusCode_FirmwareVersionMismatch );
}

static uint8_t CameraStatusToCode(CameraStatus status)
{
    static_assert((int)CameraStatus::Count == 6, "Update this");
    switch (status)
    {
    case CameraStatus::Idle:
        return static_cast<uint8_t>( protos::CameraCode_Idle );
    case CameraStatus::Initializing:
        return static_cast<uint8_t>( protos::CameraCode_Initializing );
    case CameraStatus::StartFailed:
        return static_cast<uint8_t>( protos::CameraCode_StartFailed );
    case CameraStatus::Capturing:
        return static_cast<uint8_t>( protos::CameraCode_Capturing );
    case CameraStatus::ReadFailed:
        return static_cast<uint8_t>( protos::CameraCode_ReadFailed );
    case CameraStatus::SlowWarning:
        return static_cast<uint8_t>( protos::CameraCode_SlowWarning );
    default:
        break;
    }
    spdlog::error("FIXME: Invalid camera status code");
    return static_cast<uint8_t>( protos::CameraCode_ReadFailed );
}


//------------------------------------------------------------------------------
// ViewerConnection

void ViewerConnection::OnConnect()
{
    const TonkStatusEx status = GetStatusEx();
    NetLocalName = fmt::format("[Client {}:{}]", status.Remote.NetworkString, status.Remote.UDPPort);
    spdlog::info("{} Client connected", NetLocalName);
}

void ViewerConnection::OnData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (bytes < 1) {
        spdlog::error("{} Empty message", NetLocalName);
        return;
    }

    if (channel == protos::kChannelRendezvous) {
        switch (data[0])
        {
        case protos::MessageType_ConnectName:
            if (bytes >= sizeof(protos::MessageConnectName)) {
                OnConnectName(*reinterpret_cast<const protos::MessageConnectName*>( data ));
                // Ignore guid list
            }
            break;
        default:
            spdlog::error("{} Unsupported rendezvous message from client", NetLocalName);
            return;
        }
    }
    else if (channel == protos::kChannelAuthentication) {
        switch (data[0])
        {
        case protos::MessageType_AuthClientReply:
            if (++AuthReplyCount > 1) {
                spdlog::error("{} Ignoring AuthClientReply # {}", NetLocalName, AuthReplyCount);
                return;
            }
            if (bytes == sizeof(protos::MessageAuthClientReply)) {
                OnAuthClientReply(*reinterpret_cast<const protos::MessageAuthClientReply*>(data));
            }
            break;
        case protos::MessageType_AuthClientProof:
            if (++AuthClientProofCount > 1) {
                spdlog::error("{} Ignoring AuthClientProof # {}", NetLocalName, AuthClientProofCount);
                return;
            }
            if (bytes == sizeof(protos::MessageAuthClientProof)) {
                OnAuthClientProof(*reinterpret_cast<const protos::MessageAuthClientProof*>(data));
            }
            break;
        default:
            spdlog::error("{} Invalid pre-auth message from client", NetLocalName);
            return;
        }
    }
    else {
        spdlog::error("{} Invalid channel", NetLocalName);
        return;
    }
}

void ViewerConnection::OnSecureData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (bytes < 1) {
        spdlog::error("{} Empty message", NetLocalName);
        return;
    }

    if (channel == TonkChannel_Unordered) {
        switch (data[0])
        {
        case protos::MessageType_RequestKeyframe:
            OnRequestKeyframe();
            break;
        default:
            spdlog::error("{} Unsupported unordered message from client", NetLocalName);
            return;
        }
    }
    else if (channel == protos::kChannelControl) {
        switch (data[0])
        {
        case protos::MessageType_SetCompression:
            if (bytes == sizeof(protos::MessageSetCompression)) {
                OnSetCompression(*reinterpret_cast<const protos::MessageSetCompression*>(data));
            }
            break;
        case protos::MessageType_SetMode:
            if (bytes == sizeof(protos::MessageSetMode)) {
                OnSetMode(*reinterpret_cast<const protos::MessageSetMode*>(data));
            }
            break;
        case protos::MessageType_SetExposure:
            if (bytes == sizeof(protos::MessageSetExposure)) {
                OnSetExposure(*reinterpret_cast<const protos::MessageSetExposure*>(data));
            }
            break;
        case protos::MessageType_SetClip:
            if (bytes == sizeof(protos::MessageSetClip)) {
                OnSetClip(*reinterpret_cast<const protos::MessageSetClip*>(data));
            }
            break;
        case protos::MessageType_SetLighting:
            if (bytes == sizeof(protos::MessageSetLighting)) {
                OnSetLighting(*reinterpret_cast<const protos::MessageSetLighting*>(data));
            }
            break;
        case protos::MessageType_Extrinsics:
            if (bytes == sizeof(protos::MessageExtrinsics)) {
                OnExtrinsics(*reinterpret_cast<const protos::MessageExtrinsics*>(data));
            }
            break;
        default:
            spdlog::error("{} Invalid post-auth message from client", NetLocalName);
            return;
        }
    }
    else {
        spdlog::error("{} Invalid channel", NetLocalName);
        return;
    }
}

void ViewerConnection::OnTick(
    uint64_t          nowUsec   ///< Current timestamp in microseconds
)
{
    // Every 500 ms:
    if (nowUsec - LastUpdateUsec > 500 * 1000 && IsAuthenticated()) {
        LastUpdateUsec = nowUsec;

        CaptureMode mode = Capture->GetConfiguration()->Mode;
        CaptureStatus status = Capture->GetStatus();
        std::vector<CameraStatus> cameras = Capture->GetCameraStatus();
        int camera_count = static_cast<int>( cameras.size() );
        if (camera_count > protos::kMaxCameras) {
            camera_count = protos::kMaxCameras;
        }

        protos::MessageStatus msg{};
        msg.Mode = CaptureModeToCode(mode);
        msg.CaptureStatus = CaptureStatusToCode(status);
        msg.CameraCount = static_cast<uint32_t>( camera_count );
        for (int i = 0; i < camera_count; ++i) {
            msg.CameraStatus[i] = CameraStatusToCode(cameras[i]);
        }

        tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
        if (!send_result) {
            spdlog::error("{} Send status update failed: {}", NetLocalName, send_result.ToString());
        }
    }

    // FIXME: Seems to have the wrong estimate
    //auto status = GetStatus();
    //if (status.ReliableQueueMsec < 1000)
    {
        std::shared_ptr<ImageBatch> batch;
        {
            std::lock_guard<std::mutex> locker(BatchesLock);
            if (!Batches.empty())
            {
                batch = Batches.front();
                Batches.pop_front();
            }
        }
        if (batch) {
            SendBatch(batch);
        }
    }
}

void ViewerConnection::OnClose(
    const tonk::SDKJsonResult& reason
)
{
    spdlog::warn("{} Viewer disconnected: {}", NetLocalName, reason.ToString());
    Server->Connections.Remove(this);
}

void ViewerConnection::OnConnectName(const protos::MessageConnectName& msg)
{
    std::string server_name = Server->GetServerName();
    std::string name = protos::SanitizeString(msg.Name, sizeof(msg.Name));

    if (0 != StrCaseCompare(name.c_str(), server_name.c_str())) {
        spdlog::warn("{} Client provided an incorrect server name", NetLocalName);
        SendConnectResult(protos::ConnectResult_WrongName, 0);
        return;
    }

    SendConnectResult(protos::ConnectResult_Direct, Server->GetGuid());

    // Avoid starting auth more than once
    if (StartedAuthSteps) {
        return;
    }
    StartedAuthSteps = true;

    protos::MessageAuthServerHello hello;

    const uint8_t* StoredData = Server->GetPakeStoredData();

    const int result = crypto_spake_step0(
        &Pake,
        hello.PublicData,
        StoredData);
    if (result != 0) {
        spdlog::error("crypto_spake_step0 failed");
        return;
    }
    spdlog::info("{} Client provided the correct server name: Sending Hello: H(StoredData):{} H(PublicData):{}", NetLocalName,
        HexString(XXH64(StoredData, crypto_spake_STOREDBYTES, 0)),
        HexString(XXH64(hello.PublicData, sizeof(hello.PublicData), 0)));

    SendAuthServerHello(hello);
}

void ViewerConnection::OnAuthClientReply(const protos::MessageAuthClientReply& msg)
{
    const uint8_t* StoredData = Server->GetPakeStoredData();

    spdlog::info("{} OnAuthClientReply: H(StoredData):{} H(Response1):{}", NetLocalName,
        HexString(XXH64(StoredData, crypto_spake_STOREDBYTES, 0)),
        HexString(XXH64(msg.Response1, sizeof(msg.Response1), 0)));

    protos::MessageAuthServerProof proof;

    const uint64_t t0 = GetTimeUsec();

    const int result = crypto_spake_step2(
        &Pake,
        proof.Response2,
        AUTH_CLIENT_STRING, strlen(AUTH_CLIENT_STRING),
        AUTH_SERVER_STRING, strlen(AUTH_SERVER_STRING),
        StoredData,
        msg.Response1);
    if (result != 0) {
        spdlog::error("{} crypto_spake_step2 rejected client message", NetLocalName);
        SendAuthResult(protos::AuthResult_Deny);
        return;
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("{} Sending server proof of password knowledge: Generated in {} msec. H(proof.Response2):{}", NetLocalName,
        (t1 - t0) / 1000.f,
        HexString(XXH64(proof.Response2, sizeof(proof.Response2), 0)));

    SendAuthServerProof(proof);
}

void ViewerConnection::OnAuthClientProof(const protos::MessageAuthClientProof& msg)
{
    spdlog::info("{} OnAuthClientProof: H(Response3):{}", NetLocalName,
        HexString(XXH64(msg.Response3, sizeof(msg.Response3), 0)));

    const uint64_t t0 = GetTimeUsec();

    const int result = crypto_spake_step4(
        &Pake,
        &SharedKeys,
        msg.Response3);
    if (result != 0) {
        spdlog::error("{} crypto_spake_step4 rejected client proof", NetLocalName);
        SendAuthResult(protos::AuthResult_Deny);
        return;
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("{} Client provided proof of knowing the correct password: Processed in {} msec. H(sk):{} H(ck):{}", NetLocalName,
        (t1 - t0) / 1000.f,
        HexString(XXH64(SharedKeys.server_sk, sizeof(SharedKeys.server_sk), 0)),
        HexString(XXH64(SharedKeys.client_sk, sizeof(SharedKeys.client_sk), 0)));

    // Send all remaining data using the new shared encryption key
    SetKeys(crypto_spake_SHAREDKEYBYTES, SharedKeys.server_sk, SharedKeys.client_sk, TonkKeyBehavior_Immediate);

    SendAuthResult(protos::AuthResult::AuthResult_Accept);

    // We start sending frame data after this
    Authenticated = true;
}

void ViewerConnection::OnRequestKeyframe()
{
    spdlog::debug("{} Client requested keyframe", NetLocalName);
    Capture->GetConfiguration()->NeedsKeyframe = true;
}

void ViewerConnection::OnSetCompression(const protos::MessageSetCompression& msg)
{
    const auto& compression = msg.Settings;

    spdlog::info("{} Client updated compression: color video={} bitrate={} depth video={} denoise={}",
        NetLocalName, (unsigned)compression.ColorVideo, compression.ColorBitrate,
        (unsigned)compression.DepthVideo, (unsigned)compression.DenoisePercent);

    Capture->GetConfiguration()->SetCompression(compression);
}

void ViewerConnection::OnSetMode(const protos::MessageSetMode& msg)
{
    switch (msg.Mode)
    {
    default:
        spdlog::error("{} Unknown mode requested", NetLocalName);
        break;
    case protos::Mode_Disabled:
        spdlog::info("{} Viewer set mode: Disabled", NetLocalName);
        Capture->SetMode(CaptureMode::Disabled);
        break;
    case protos::Mode_Calibration:
        spdlog::info("{} Viewer set mode: Calibration", NetLocalName);
        Capture->SetMode(CaptureMode::Calibration);
        break;
    case protos::Mode_CaptureLowQual:
        spdlog::info("{} Viewer set mode: Capture (Low Quality)", NetLocalName);
        Capture->SetMode(CaptureMode::CaptureLowQual);
        break;
    case protos::Mode_CaptureHighQual:
        spdlog::info("{} Viewer set mode: Capture (High Quality)", NetLocalName);
        Capture->SetMode(CaptureMode::CaptureHighQual);
        break;
    }
}

void ViewerConnection::OnSetExposure(const protos::MessageSetExposure& msg)
{
    if (msg.AutoEnabled) {
        spdlog::info("{} Viewer enabled auto-exposure", NetLocalName);
    } else {
        spdlog::info("{} Viewer set manual exposure={} awb={}",
            NetLocalName, msg.ExposureUsec, msg.AutoWhiteBalanceUsec);
    }

    Capture->GetConfiguration()->SetExposure(msg);
}

void ViewerConnection::OnSetLighting(const protos::MessageSetLighting& msg)
{
    spdlog::info("{} Viewer set lighting: camera={} brightness={} saturation={}",
        NetLocalName, msg.CameraIndex, msg.Brightness, msg.Saturation);

    Capture->GetConfiguration()->SetLighting(msg);
}

void ViewerConnection::OnSetClip(const protos::MessageSetClip& msg)
{
    if (!msg.Enabled) {
        spdlog::info("{} Viewer disabled clip", NetLocalName);
    } else {
        spdlog::info("{} Viewer enabled clip radius={} floor={} ceiling={}",
            NetLocalName, msg.ClipRadiusMeters, msg.ClipFloorMeters, msg.ClipCeilingMeters);
    }

    Capture->GetConfiguration()->SetClip(msg);
}

void ViewerConnection::OnExtrinsics(const protos::MessageExtrinsics& msg)
{
    if (msg.CameraIndex >= protos::kMaxCameras) {
        return;
    }
    Capture->GetConfiguration()->SetExtrinsics(msg.CameraIndex, msg.Extrinsics);
}

void ViewerConnection::SendAuthServerHello(protos::MessageAuthServerHello& msg)
{
    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelAuthentication);
    if (!send_result) {
        spdlog::error("{} SendAuthServerHello failed: {}", NetLocalName, send_result.ToString());
    }
}

void ViewerConnection::SendAuthServerProof(protos::MessageAuthServerProof& msg)
{
    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelAuthentication);
    if (!send_result) {
        spdlog::error("{} SendAuthServerProof failed: {}", NetLocalName, send_result.ToString());
    }
}

void ViewerConnection::SendAuthResult(protos::AuthResult ar)
{
    protos::MessageAuthResult reply;
    reply.Result = static_cast<uint8_t>( ar );

    tonk::SDKResult send_result = Send(&reply, sizeof(reply), protos::kChannelAuthentication);
    if (!send_result) {
        spdlog::error("{} SendAuthResult failed: {}", NetLocalName, send_result.ToString());
    }
}

void ViewerConnection::SendConnectResult(protos::ConnectResult cr, uint64_t guid)
{
    protos::MessageConnectResult reply;
    reply.Result = static_cast<uint8_t>( cr );
    reply.ServerGuid = guid;
    reply.ServerCount = 1;

    tonk::SDKResult send_result = Send(&reply, sizeof(reply), protos::kChannelRendezvous);
    if (!send_result) {
        spdlog::error("{} SendConnectResult failed: {}", NetLocalName, send_result.ToString());
    }
}

void ViewerConnection::SendCalibration(
    unsigned camera,
    const core::CameraCalibration& calibration)
{
    spdlog::info("{} Sending calibration for camera {}", NetLocalName, camera);

    protos::MessageCalibration msg;
    msg.CameraIndex = static_cast<uint32_t>( camera );
    msg.Calibration = calibration;

    tonk::SDKResult result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!result) {
        spdlog::error("{} SendCalibration failed: {}", NetLocalName, result.ToString());
    }
}

void ViewerConnection::SendExtrinsics(
    unsigned camera,
    const protos::CameraExtrinsics& extrinsics)
{
    spdlog::info("{} Sending extrinsics for camera {}", NetLocalName, camera);

    protos::MessageExtrinsics msg;
    msg.CameraIndex = static_cast<uint32_t>( camera );
    msg.Extrinsics = extrinsics;

    tonk::SDKResult result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!result) {
        spdlog::error("{} SendExtrinsics failed: {}", NetLocalName, result.ToString());
    }
}

void ViewerConnection::SendVideoInfo(protos::MessageVideoInfo& info)
{
    spdlog::info("{} Sending video info Bitrate={} VideoType={}", NetLocalName, info.Bitrate, (int)info.VideoType);

    tonk::SDKResult result = Send(&info, sizeof(info), protos::kChannelControl);
    if (!result) {
        spdlog::error("{} SendVideoInfo failed: {}", NetLocalName, result.ToString());
    }
}

void ViewerConnection::QueueBatch(std::shared_ptr<ImageBatch> batch)
{
    std::lock_guard<std::mutex> locker(BatchesLock);
    if (Batches.size() >= 30) {
        auto status = GetStatus();
        spdlog::error("Client connection too slow: BPS={} RelQMsec={}", status.AppBPS, status.ReliableQueueMsec);
        return;
    }
    Batches.push_back(batch);
}

void ViewerConnection::SendBatch(std::shared_ptr<ImageBatch> batch)
{
    if (batch->Images.empty()) {
        spdlog::error("{} FIXME: SendBatch is empty", NetLocalName);
        return;
    }

    const uint32_t video_info_epoch = batch->VideoInfoEpoch;
    if (VideoInfoEpoch.exchange(video_info_epoch) != video_info_epoch)
    {
        spdlog::info("{} Delivering updated video info to peer", NetLocalName);
        SendVideoInfo(batch->VideoInfo);
    }

    const int max_payload = 16000;

    tonk::SDKResult result = Send(&batch->StreamInfo, sizeof(batch->StreamInfo), protos::kChannelControl);
    if (!result) {
        spdlog::error("{} SendVideoInfo failed: {}", NetLocalName, result.ToString());
    }

    const int image_count = static_cast<int>(batch->Images.size());
    for (int image_index = 0; image_index < image_count; ++image_index)
    {
        auto& image = batch->Images[image_index];

        protos::MessageFrameHeader header;
        header.IsFinalFrame = (image_index == image_count - 1) ? 1 : 0;
        header.FrameNumber = image->FrameNumber;
        header.BackReference = batch->Keyframe ? 0 : -1;
        header.CameraIndex = static_cast<uint32_t>( image->DeviceIndex );
        header.ImageBytes = static_cast<uint32_t>( image->CompressedImage.size() );
        header.DepthBytes = static_cast<uint32_t>( image->CompressedDepth.size() );
        for (int i = 0; i < 3; ++i) {
            header.Accelerometer[i] = image->AccelerationSample[i];
        }
        header.ExposureUsec = static_cast<uint32_t>( image->ColorExposureUsec );
        header.AutoWhiteBalanceUsec = image->ColorWhiteBalanceUsec;
        header.ISOSpeed = image->ColorIsoSpeed;
        header.Brightness = image->Brightness;
        header.Saturation = image->Saturation;

        result = Send(&header, sizeof(header), protos::kChannelControl);
        if (!result) {
            spdlog::error("{} SendBatch failed: {}", NetLocalName, result.ToString());
            return;
        }

        // Send imagery:

        {
            uint8_t* data = image->CompressedImage.data();
            int bytes = static_cast<int>( image->CompressedImage.size() );

            while (bytes > 0) {
                int copy_bytes = bytes;
                if (copy_bytes > max_payload) {
                    copy_bytes = max_payload;
                }

                result = Send(data, copy_bytes, protos::kChannelImage);
                if (!result) {
                    spdlog::error("{} SendBatch failed: {}", NetLocalName, result.ToString());
                    return;
                }

                data += copy_bytes;
                bytes -= copy_bytes;
            }
        }

        // Send depth:

        {
            uint8_t* data = image->CompressedDepth.data();
            int bytes = static_cast<int>( image->CompressedDepth.size() );

            while (bytes > 0) {
                int copy_bytes = bytes;
                if (copy_bytes > max_payload) {
                    copy_bytes = max_payload;
                }

                result = Send(data, copy_bytes, protos::kChannelDepth);
                if (!result) {
                    spdlog::error("{} SendBatch failed: {}", NetLocalName, result.ToString());
                    return;
                }

                data += copy_bytes;
                bytes -= copy_bytes;
            }
        }
    }
}


//------------------------------------------------------------------------------
// RendezvousConnection

void RendezvousConnection::OnConnect()
{
    const TonkStatusEx status = GetStatusEx();
    NetLocalName = fmt::format("[Rendezvous {}:{}]", status.Remote.NetworkString, status.Remote.UDPPort);
    spdlog::info("{} Rendezvous server connected", NetLocalName);

    const std::string name = Server->GetServerName();

    protos::MessageRegisterCaptureServer msg;
    msg.Guid = Server->GetGuid();
    core::SafeCopyCStr(msg.Name, sizeof(msg.Name), name.c_str());

    tonk::SDKResult result = Send(&msg, sizeof(msg), protos::kChannelRendezvous);
    if (!result) {
        spdlog::error("{} SendRegisterCaptureServer failed: {}", NetLocalName, result.ToString());
    }
}

void RendezvousConnection::OnData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (bytes <= 0) {
        return;
    }
    if (channel == protos::kChannelRendezvous) {
        switch (data[0])
        {
        case protos::MessageType_AssignTDMA:
            if (bytes >= sizeof(protos::MessageAssignTDMA)) {
                const protos::MessageAssignTDMA* msg = reinterpret_cast<const protos::MessageAssignTDMA*>( data );
                if (bytes == sizeof(protos::MessageAssignTDMA) + msg->CameraCount * sizeof(int16_t)) {
                    const int16_t* offsets = reinterpret_cast<const int16_t*>( data + sizeof(protos::MessageAssignTDMA) );
                    OnAssignTDMA(*msg, offsets);
                } else {
                    spdlog::info("{} Unexpected type from rendezvous server: {} bytes on channel {}", NetLocalName, bytes, channel);
                }
            }
            break;
        default:
            spdlog::info("{} Unexpected type from rendezvous server: {} bytes on channel {}", NetLocalName, bytes, channel);
            break;
        }
    }
}

void RendezvousConnection::OnClose(
    const tonk::SDKJsonResult& reason
)
{
    spdlog::warn("{} Disconnected from rendezvous server: {}", NetLocalName, reason.ToString());
    Server->OnRendezvousClose();
}

void RendezvousConnection::SendRequestTDMA(unsigned camera_count)
{
    protos::MessageRequestTDMA msg;
    msg.CameraCount = static_cast<uint8_t>( camera_count );

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelRendezvous);
    if (!send_result) {
        spdlog::error("{} Send request TDMA failed: {}", NetLocalName, send_result.ToString());
    }
}

void RendezvousConnection::OnAssignTDMA(const protos::MessageAssignTDMA& msg, const int16_t* offsets)
{
    const unsigned count = msg.CameraCount;
    std::vector<int> slots(count);

    spdlog::info("{} Got TDMA slot assignment from rendezvous server: count={}", NetLocalName, count);

    for (unsigned i = 0; i < count; ++i) {
        slots[i] = offsets[i];
    }

    Capture->SetTdmaSlots(slots);
}


//------------------------------------------------------------------------------
// CaptureServer

bool CaptureServer::Initialize(
    CaptureManager* capture,
    const std::string& server_name,
    const std::string& rendezvous_server_host,
    int rendezvous_server_port,
    const std::string& stored_data_base64,
    int port,
    bool enable_multi_server)
{
    Terminated = false;

    Capture = capture;
    CaptureServerName = server_name;
    RendezvousServerHostname = rendezvous_server_host;
    RendezvousServerPort = rendezvous_server_port;
    EnableMultiServer = enable_multi_server;

    // Try a few times to generate a non-zero number for Guid
    Guid = 0;
    for (int zero_retry = 0; zero_retry < 4; ++zero_retry) {
        TonkResult guid_result = tonk_random(&Guid, sizeof(Guid));
        if (TONK_FAILED(guid_result)) {
            spdlog::error("tonk_random failed to generate guid");
            return false;
        }
        if (Guid != 0) {
            break;
        }
    }
    if (Guid == 0) {
        spdlog::warn("Failed to get a non-zero GUID: Using time instead");
        Guid = GetTimeUsec();
    }

    if (sodium_init() < 0) {
        spdlog::error("sodium_init failed");
        return false;
    }

    bool valid_password = false;

    // Read password hash
    if (stored_data_base64.empty()) {
        spdlog::error("Empty server password provided");
    } else {
        const char* b64 = stored_data_base64.c_str();
        const int encoded_bytes = static_cast<int>(stored_data_base64.size());
        const int stored_data_bytes = GetByteCountFromBase64(b64, encoded_bytes);
        if (stored_data_bytes == crypto_spake_STOREDBYTES) {
            const int written = ReadBase64(b64, encoded_bytes, StoredData);
            if (written == crypto_spake_STOREDBYTES) {
                spdlog::info("Successfully read stored password data. H(StoredData):{}",
                    HexString(XXH64(StoredData, sizeof(StoredData), 0)));
                valid_password = true;
            } else {
                spdlog::error("Invalid length={} password hash did not decode", stored_data_bytes);
            }
        } else {
            spdlog::error("Invalid length={} password hash provided", stored_data_bytes);
        }
    }

    if (!valid_password) {
        spdlog::warn("No password provided: Using an empty password.");
        const int store_result = crypto_spake_server_store(
            StoredData,
            "", 0,
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE);
        if (store_result != 0) {
            spdlog::error("crypto_spake_server_store failed");
        } else {
            spdlog::info("Generated empty password. H(StoredData):{}",
                HexString(XXH64(StoredData, sizeof(StoredData), 0)));
        }
    }

    // Set netcode configuration
    tonk::SDKSocket::Config.UDPListenPort = static_cast<uint32_t>( port );
    tonk::SDKSocket::Config.MaximumClients = 10;
    tonk::SDKSocket::Config.TimerIntervalUsec = 10000; // 10 msec
    tonk::SDKSocket::Config.Flags = \
        TONK_FLAGS_ENABLE_UPNP |
        TONK_FLAGS_DISABLE_COMPRESSION |
        TONK_FLAGS_DISABLE_FEC_BW_PROBES |
        TONK_FLAGS_DISABLE_BW_PROBES;
        //TONK_FLAGS_DISABLE_CC;
    tonk::SDKSocket::Config.BandwidthLimitBPS = protos::kBandwidthLimitBPS;

    tonk::SDKJsonResult result = tonk::SDKSocket::Create();
    if (!result) {
        spdlog::error("Unable to create socket: {}", result.ToString());
        return false;
    }

    Thread = std::make_shared<std::thread>(&CaptureServer::Loop, this);
    Worker.Initialize(kMaxQueuedVideoSends);

    return true;
}

void CaptureServer::Loop()
{
    while (!Terminated) {
        Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CaptureServer::Shutdown()
{
    Terminated = true;
    JoinThread(Thread);
    Worker.Shutdown();

    {
        std::lock_guard<std::mutex> locker(RendezvousLock);
        Rendezvous.reset();
    }

    spdlog::info("Destroying capture server...");
    tonk::SDKSocket::BlockingDestroy();
    spdlog::info("...Capture server destroyed");
}

void CaptureServer::Tick()
{
    if (Terminated) {
        return;
    }

    const uint64_t now_usec = GetTimeUsec();

    // Check for rendezvous client reconnect here
    if (now_usec - LastTickUsec > 2 * 1000 * 1000) {
        LastTickUsec = now_usec;

        std::lock_guard<std::mutex> locker(RendezvousLock);
        if (Rendezvous) {
            //spdlog::debug("Still connected to rendezvous server");
        } else {
            spdlog::warn("Not connected to rendezvous server");

            Rendezvous = std::make_shared<RendezvousConnection>(Capture, this);

            tonk::SDKJsonResult result = Connect(
                Rendezvous.get(),
                RendezvousServerHostname,
                static_cast<uint16_t>( RendezvousServerPort ));
            if (!result) {
                spdlog::warn("Failed to connect to rendezvous server: {}", result.ToString());
            } else {
                spdlog::debug("Connecting to rendezvous server...");
            }
        }
    }

    const bool want_video = (Connections.GetCount() > 0);
    Capture->GetConfiguration()->VideoNeeded = want_video;

    if (EnableMultiServer)
    {
        const unsigned camera_count = GetAttachedK4CameraCount();
        const unsigned tdma_count = Capture->GetTdmaSlotCount();
        if (tdma_count != camera_count)
        {
            if (now_usec - LastTdmaRequestUsec > 1000 * 1000) {
                LastTdmaRequestUsec = now_usec;

                std::shared_ptr<RendezvousConnection> rendezvous;
                {
                    std::lock_guard<std::mutex> locker(RendezvousLock);
                    rendezvous = Rendezvous;
                }
                if (rendezvous)
                {
                    spdlog::info("Camera count changed: detected={} tdma={} Requesting new TDMA slots...", camera_count, tdma_count);
                    rendezvous->SendRequestTDMA(camera_count);
                }
                else
                {
                    spdlog::warn("Rendezvous server not connected: Delaying capture in multi-server mode until it can be contacted");
                }
            }
        }
    }
}

void CaptureServer::OnRendezvousClose()
{
    // Reset TDMA slots so we will delay capture until we update this
    Capture->SetTdmaSlots();

    std::lock_guard<std::mutex> locker(RendezvousLock);
    Rendezvous.reset();
}

tonk::SDKConnection* CaptureServer::OnIncomingConnection(
    const TonkAddress& address ///< Address of the client requesting a connection
)
{
    CORE_UNUSED(address);
    auto ptr = std::make_shared<ViewerConnection>(this, Capture);

    // Insert into connection list to prevent it from going out of scope
    Connections.Insert(ptr.get());

    return ptr.get();
}

tonk::SDKConnection* CaptureServer::OnP2PConnectionStart(
    const TonkAddress& address ///< Address of the other peer we are to connect to
)
{
    CORE_UNUSED(address);
    auto ptr = std::make_shared<ViewerConnection>(this, Capture);

    // Insert into connection list to prevent it from going out of scope
    Connections.Insert(ptr.get());

    return ptr.get();
}

void CaptureServer::BroadcastVideo(std::shared_ptr<ImageBatch>& batch)
{
    const bool success = Worker.SubmitWork([this, batch]()
    {
        auto connections = Connections.GetList();
        if (connections.empty()) {
            return;
        }
        if (batch->Images.empty()) {
            return;
        }

        RuntimeConfiguration* runtime_config = Capture->GetConfiguration();

        const uint32_t capture_config_epoch = runtime_config->CaptureConfigEpoch;
        const uint32_t extrinsics_epoch = runtime_config->ExtrinsicsEpoch;

        for (auto& connection : connections)
        {
            if (!connection->IsAuthenticated()) {
                continue;
            }

            // If we need to update the capture configuration for this one:
            if (connection->CaptureConfigEpoch.exchange(capture_config_epoch) != capture_config_epoch)
            {
                spdlog::info("Delivering updated capture configuration data to peer");
                auto calibration_data = Capture->GetCameraCalibration();
                const int device_count = static_cast<int>( calibration_data.size() );
                for (int device_index = 0; device_index < device_count; ++device_index) {
                    connection->SendCalibration(device_index, calibration_data[device_index]);
                }
            }

            if (connection->ExtrinsicsConfigEpoch.exchange(extrinsics_epoch) != extrinsics_epoch)
            {
                std::vector<protos::CameraExtrinsics> extrinsics = runtime_config->GetExtrinsics();
                const int device_count = static_cast<int>( extrinsics.size() );

                for (int device_index = 0; device_index < device_count; ++device_index)
                {
                    spdlog::info("Delivering updated extrinsics data to peer for camera={}/{}", device_index, device_count);
                    connection->SendExtrinsics(device_index, extrinsics[device_index]);
                } // next device
            } // end if extrinsics

            connection->QueueBatch(batch);
        } // next connection
    });

    if (!success) {
        spdlog::warn("Computer too slow: Video broadcast thread cannot keep up with the video batches! Dropped a batch, forcing a keyframe");
        Capture->GetConfiguration()->NeedsKeyframe = true;
    }
}


} // namespace core
