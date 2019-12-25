// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "NetClient.hpp"

#include <core_string.hpp>

#include <sodium.h>
#include <xxhash.h>

namespace core {


//------------------------------------------------------------------------------
// StreamedBuffer

void StreamedBuffer::Reset(int bytes)
{
    ExpectedBytes = bytes;
    ReceivedBytes = 0;
    Data.clear();
    if (bytes > 0) {
        Data.resize(bytes);
        Complete = false;
    } else {
        Complete = true;
    }
}

bool StreamedBuffer::Accumulate(const uint8_t* data, int bytes)
{
    if (bytes <= 0) {
        spdlog::error("Ignoring empty data");
        return false;
    }
    const int remaining_bytes = ExpectedBytes - ReceivedBytes;
    if (remaining_bytes <= 0) {
        spdlog::error("Received data after complete: remaining_bytes={}", remaining_bytes);
        return false;
    }
    if (remaining_bytes < bytes) {
        spdlog::error("Received too many bytes: remaining_bytes={} < bytes={}", remaining_bytes, bytes);
        return false;
    }
    if (Data.empty()) {
        spdlog::error("Data empty");
        return false;
    }

    uint8_t* dest = Data.data();
    memcpy(dest + ReceivedBytes, data, bytes);
    ReceivedBytes += bytes;

    if (remaining_bytes > bytes) {
        return false;
    }

    Complete = true;
    return true;
}


//------------------------------------------------------------------------------
// CaptureConnection

void CaptureConnection::OnConnect()
{
    const TonkStatusEx status = GetStatusEx();
    NetLocalName = fmt::format("[Server {}:{}]", status.Remote.NetworkString, status.Remote.UDPPort);
    spdlog::info("{} Server connected", NetLocalName);

    // We now wait for the server to send its AuthHello message.
    // On tick we send requests in case the peer is a rendezvous server.
    NeedsToSendConnectName = true;

    Client->OnConnect(this);
}

void CaptureConnection::OnData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (bytes <= 0) {
        return;
    }

    if (channel == protos::kChannelRendezvous)
    {
        switch (data[0])
        {
        case protos::MessageType_ConnectResult:
            if (bytes == sizeof(protos::MessageConnectResult)) {
                OnConnectResult(*reinterpret_cast<const protos::MessageConnectResult*>(data));
            }
            break;
        default:
            spdlog::error("{} Ignored unexpected rendezvous message from peer", NetLocalName);
            break;
        }
    }
    else if (channel == protos::kChannelAuthentication)
    {
        switch (data[0])
        {
        case protos::MessageType_AuthServerHello:
            if (++AuthServerHelloCount > 1) {
                spdlog::error("{} Ignoring AuthServerHello # {}", NetLocalName, AuthServerHelloCount);
                return;
            }
            if (bytes == sizeof(protos::MessageAuthServerHello)) {
                OnAuthServerHello(*reinterpret_cast<const protos::MessageAuthServerHello*>(data));
            }
            break;
        case protos::MessageType_AuthServerProof:
            if (++AuthServerProofCount > 1) {
                spdlog::error("{} Ignoring AuthServerProof # {}", NetLocalName, AuthServerProofCount);
                return;
            }
            if (bytes == sizeof(protos::MessageAuthServerProof)) {
                OnAuthServerProof(*reinterpret_cast<const protos::MessageAuthServerProof*>(data));
            }
            break;
        case protos::MessageType_AuthResult:
            if (++AuthResultCount > 1) {
                spdlog::error("{} Ignoring AuthResult # {}", NetLocalName, AuthResultCount);
                return;
            }
            if (bytes == sizeof(protos::MessageAuthResult)) {
                OnAuthResult(*reinterpret_cast<const protos::MessageAuthResult*>(data));
            }
            break;
        default:
            spdlog::error("{} Ignored unexpected auth message from peer", NetLocalName);
            break;
        }
    }
}

void CaptureConnection::OnSecureData(
    uint32_t          channel,  ///< Channel number attached to each message by sender
    const uint8_t*       data,  ///< Pointer to a buffer containing the message data
    uint32_t            bytes   ///< Number of bytes in the message
)
{
    if (bytes <= 0) {
        return;
    }

    if (channel == protos::kChannelControl)
    {
        switch (data[0])
        {
        case protos::MessageType_Status:
            if (bytes == sizeof(protos::MessageStatus)) {
                OnStatus(*reinterpret_cast<const protos::MessageStatus*>(data));
            }
            break;
        case protos::MessageType_VideoInfo:
            if (bytes == sizeof(protos::MessageVideoInfo)) {
                OnVideoInfo(*reinterpret_cast<const protos::MessageVideoInfo*>(data));
            }
            break;
        case protos::MessageType_BatchInfo:
            if (bytes == sizeof(protos::MessageBatchInfo)) {
                OnBatchInfo(*reinterpret_cast<const protos::MessageBatchInfo*>(data));
            }
            break;
        case protos::MessageType_Calibration:
            if (bytes == sizeof(protos::MessageCalibration)) {
                OnCalibration(*reinterpret_cast<const protos::MessageCalibration*>(data));
            }
            break;
        case protos::MessageType_Extrinsics:
            if (bytes == sizeof(protos::MessageExtrinsics)) {
                OnExtrinsics(*reinterpret_cast<const protos::MessageExtrinsics*>(data));
            }
            break;
        case protos::MessageType_FrameHeader:
            if (bytes == sizeof(protos::MessageFrameHeader)) {
                OnFrameHeader(*reinterpret_cast<const protos::MessageFrameHeader*>(data));
            }
            break;
        default:
            spdlog::error("{} Ignored unexpected control message from peer", NetLocalName);
            break;
        }
    }
    else if (channel == protos::kChannelImage)
    {
        OnImageData(data, bytes);
    }
    else if (channel == protos::kChannelDepth)
    {
        OnDepthData(data, bytes);
    }
}

void CaptureConnection::OnTick(
    uint64_t          nowUsec   ///< Current timestamp in microseconds
)
{
    if (NeedsToSendConnectName)
    {
        // Every second:
        if (nowUsec - LastConnectRequestUsec > 1000 * 1000) {
            LastConnectRequestUsec = nowUsec;
            //spdlog::debug("{} Sending connect request for `{}`", NetLocalName, Client->GetServerName());
            SendConnectName(Client->GetServerName());
        }
    }
}

void CaptureConnection::OnClose(
    const tonk::SDKJsonResult& reason
)
{
    spdlog::warn("{} Disconnected from peer: {}", NetLocalName, reason.ToString());
    Client->OnConnectionClosed(this);
}

void CaptureConnection::OnConnectResult(const protos::MessageConnectResult& msg)
{
    switch (msg.Result)
    {
    case protos::ConnectResult_NotFound:
        //spdlog::info("{} Connect result: Server name not found - Need to retry as server may be restarting", NetLocalName);
        State = XrcapStreamState_ServerOffline;
        break;
    case protos::ConnectResult_NotReady:
        spdlog::info("{} Connect result: Server not ready - Need to retry as server may be restarting", NetLocalName);
        State = XrcapStreamState_ServerBusy;
        break;
    case protos::ConnectResult_Connecting:
        spdlog::info("{} Connect result: Attempting to relay connection to capture server", NetLocalName);
        State = XrcapStreamState_Relaying;
        break;
    case protos::ConnectResult_Direct:
        ServerGuid = msg.ServerGuid;

        spdlog::info("{} Connect result: Connected directly to the capture server with guid={}", NetLocalName, ServerGuid);
        if (!Client->CheckDirectConnectUnique(this)) {
            spdlog::warn("{} Closing extra connection to the same server", NetLocalName);
            Close();
            return;
        }
        State = XrcapStreamState_Authenticating;
        NeedsToSendConnectName = false; // Can stop sending this now
        break;
    }
}

void CaptureConnection::OnFrame(std::shared_ptr<FrameInfo> frame)
{
    frame->Guid = ServerGuid;

    const unsigned camera_count = frame->BatchInfo->CameraCount;
    if (Decoders.size() != camera_count) {
        Decoders.clear();
        Decoders.resize(camera_count);

        for (unsigned i = 0; i < camera_count; ++i) {
            Decoders[i] = std::make_shared<DecoderPipeline>();
        }
    }

    //spdlog::info("{} Test: camera={} frame={} boot={}", NetLocalName, camera_index, frame->BatchInfo->FrameNumber, frame->BatchInfo->VideoBootUsec);

    std::shared_ptr<DecodePipelineData> data = std::make_shared<DecodePipelineData>();
    data->Input = frame;
    data->Callback = [this](std::shared_ptr<DecodedFrame> decoded) {
        Client->PlaybackQueue->Insert(decoded);
    };

    const unsigned camera_index = frame->FrameHeader.CameraIndex;
    Decoders[camera_index]->Process(data);
}

void CaptureConnection::OnAuthServerHello(const protos::MessageAuthServerHello& msg)
{
    spdlog::info("{} OnAuthServerHello: H(PublicData)={}", NetLocalName,
        HexString(XXH64(msg.PublicData, sizeof(msg.PublicData), 0)));

    const int valid = crypto_spake_validate_public_data(
        msg.PublicData,
        crypto_pwhash_alg_default(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE);

    if (valid != 0) {
        spdlog::error("{} crypto_spake_validate_public_data failed", NetLocalName);
        Close();
        return;
    }

    protos::MessageAuthClientReply reply;

    std::string pw = Client->GetPassword();

    // FIXME: Do not log the password to disk
    //spdlog::info("Password from application: `{}` len={}", pw, pw.size());

    const uint64_t t0 = GetTimeUsec();

    const int step1 = crypto_spake_step1(
        &Pake,
        reply.Response1,
        msg.PublicData,
        pw.c_str(),
        pw.size());

    if (step1 != 0) {
        spdlog::error("{} crypto_spake_step1 failed: Server reply was invalid", NetLocalName);
        Close();
        return;
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("{}Generated response1 from public data in {} msec: H(Response1):{}", NetLocalName,
        (t1 - t0) / 1000.f,
        HexString(XXH64(reply.Response1, sizeof(reply.Response1), 0)));

    tonk::SDKResult send_result = Send(&reply, sizeof(reply), protos::kChannelAuthentication);
    if (!send_result) {
        spdlog::error("{} OnAuthServerHello: Send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::OnAuthServerProof(const protos::MessageAuthServerProof& msg)
{
    spdlog::info("{} OnAuthServerProof: H(Response2):{}", NetLocalName,
        HexString(XXH64(msg.Response2, sizeof(msg.Response2), 0)));

    protos::MessageAuthClientProof proof;

    const uint64_t t0 = GetTimeUsec();

    const int step3 = crypto_spake_step3(
        &Pake,
        proof.Response3,
        &SharedKeys,
        AUTH_CLIENT_STRING, strlen(AUTH_CLIENT_STRING),
        AUTH_SERVER_STRING, strlen(AUTH_SERVER_STRING),
        msg.Response2);

    if (step3 != 0) {
        spdlog::error("{} crypto_spake_step3 failed: Server password does not match our password", NetLocalName);
        State = XrcapStreamState_IncorrectPassword;
        Close();
        return;
    }

    const uint64_t t1 = GetTimeUsec();

    State = XrcapStreamState_Live;

    spdlog::info("{} Successfully authenticated: Verified the server knows the password in {} msec. H(sk):{} H(ck):{} H(Response3):{}", NetLocalName,
        (t1 - t0) / 1000.f,
        HexString(XXH64(proof.Response3, sizeof(proof.Response3), 0)),
        HexString(XXH64(SharedKeys.server_sk, sizeof(SharedKeys.server_sk), 0)),
        HexString(XXH64(SharedKeys.client_sk, sizeof(SharedKeys.client_sk), 0)));

    // Wait for peer to send us a valid encrypted message to start enabling encryption
    SetKeys(crypto_spake_SHAREDKEYBYTES, SharedKeys.client_sk, SharedKeys.server_sk, TonkKeyBehavior_WaitForPeer);

    IsAuthenticated = true;

    tonk::SDKResult send_result = Send(&proof, sizeof(proof), protos::kChannelAuthentication);
    if (!send_result) {
        spdlog::error("{} OnAuthServerProof: Send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::OnAuthResult(const protos::MessageAuthResult& msg)
{
    switch (msg.Result)
    {
    case protos::AuthResult_Accept:
        spdlog::info("{} Server accepted our password", NetLocalName);
        // No state update: We wait for the server to authenticate also
        break;
    default:
        spdlog::error("{} Invalid auth result from server", NetLocalName);
    case protos::AuthResult_Deny: // Fall-thru
        spdlog::info("{} Server denied us access: We thought password matched but server disagreed", NetLocalName);
        State = XrcapStreamState_IncorrectPassword;
        Close();
        break;
    }
}

void CaptureConnection::OnStatus(const protos::MessageStatus& msg)
{
    static_assert(protos::kMaxCameras <= XRCAP_PERSPECTIVE_COUNT, "Update this");

    std::lock_guard<std::mutex> locker(StatusLock);
    LastStatus = msg;
}

void CaptureConnection::OnVideoInfo(const protos::MessageVideoInfo& msg)
{
    VideoInfo = std::make_shared<protos::MessageVideoInfo>(msg);
}

void CaptureConnection::OnBatchInfo(const protos::MessageBatchInfo& msg)
{
    BatchInfo = std::make_shared<protos::MessageBatchInfo>(msg);

    // Convert boot time to local time
    const uint64_t remote_usec = BatchInfo->VideoBootUsec;
    BatchInfo->VideoBootUsec = FromRemoteTime(remote_usec);

    //spdlog::info("TEST: {} - {} [{}]", remote_usec, BatchInfo->VideoBootUsec, NetLocalName);
}

void CaptureConnection::OnCalibration(const protos::MessageCalibration& msg)
{
    if (msg.CameraIndex >= protos::kMaxCameras) {
        spdlog::error("{} Camera index {} out of range", NetLocalName, msg.CameraIndex);
        return;
    }

    spdlog::info("{} Set calibration for camera {} bytes={}", NetLocalName, msg.CameraIndex, sizeof(msg));

    Calibration[msg.CameraIndex] = std::make_shared<core::CameraCalibration>(msg.Calibration);
}

void CaptureConnection::OnExtrinsics(const protos::MessageExtrinsics& msg)
{
    if (msg.CameraIndex >= protos::kMaxCameras) {
        spdlog::error("{} Camera index {} out of range", NetLocalName, msg.CameraIndex);
        return;
    }

    spdlog::info("{} Updated extrinsics for camera {}: identity={}",
        NetLocalName, msg.CameraIndex,
        msg.Extrinsics.IsIdentity != 0);

    Extrinsics[msg.CameraIndex] = std::make_shared<protos::CameraExtrinsics>(msg.Extrinsics);
}

void CaptureConnection::OnFrameHeader(const protos::MessageFrameHeader& msg)
{
    if (!BatchInfo || !VideoInfo) {
        spdlog::error("{} Ignoring frame without batch or video info", NetLocalName);
        return;
    }

    if (msg.CameraIndex >= protos::kMaxCameras) {
        spdlog::error("{} Frame header camera index out of range", NetLocalName);
        return;
    }

    if (!Calibration[msg.CameraIndex]) {
        spdlog::error("{} Ignoring frame without camera calibration", NetLocalName);
        return;
    }

    // FIXME: Pool allocator for frames
    Frame.reset();
    Frame = std::make_shared<FrameInfo>();

#if 0
    spdlog::info("{} Receiving frame {} for camera {}/{} ImageBytes={} DepthBytes={}...", NetLocalName,
        BatchInfo->FrameNumber, msg.CameraIndex, BatchInfo->CameraCount, msg.ImageBytes, msg.DepthBytes);
#endif

    Frame->BatchInfo = BatchInfo;
    Frame->VideoInfo = VideoInfo;
    Frame->FrameHeader = msg;
    {
        std::lock_guard<std::mutex> locker(StatusLock);
        Frame->CaptureMode = static_cast<protos::Modes>( LastStatus.Mode );
    }
    Frame->Calibration = Calibration[msg.CameraIndex];
    Frame->Extrinsics = Extrinsics[msg.CameraIndex];
    Frame->StreamedImage.Reset(msg.ImageBytes);
    Frame->StreamedDepth.Reset(msg.DepthBytes);
}

void CaptureConnection::OnImageData(const uint8_t* data, int bytes)
{
    if (!Frame) {
        spdlog::error("{} Ignoring image data with no header", NetLocalName);
        return;
    }
    if (Frame->StreamedImage.Accumulate(data, bytes)) {
        if (Frame->StreamedDepth.Complete && Frame->StreamedImage.Complete) {
            OnFrame(Frame);
            Frame.reset();
        }
    }
}

void CaptureConnection::OnDepthData(const uint8_t* data, int bytes)
{
    if (!Frame) {
        spdlog::error("{} Ignoring depth data with no header", NetLocalName);
        return;
    }
    if (Frame->StreamedDepth.Accumulate(data, bytes)) {
        if (Frame->StreamedDepth.Complete && Frame->StreamedImage.Complete) {
            OnFrame(Frame);
            Frame.reset();
        }
    }
}

void CaptureConnection::SendKeyframeRequest()
{
    uint8_t msg[1];
    msg[0] = static_cast<uint8_t>( protos::MessageType_RequestKeyframe );

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), TonkChannel_Unordered);
    if (!send_result) {
        spdlog::error("{} SendKeyframeRequest send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SetExposure(
    int32_t auto_enabled,
    uint32_t exposure_usec,
    uint32_t awb_usec)
{
    protos::MessageSetExposure msg{};
    msg.AutoEnabled = auto_enabled;
    msg.ExposureUsec = exposure_usec;
    msg.AutoWhiteBalanceUsec = awb_usec;

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SetExposure send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SetClip(
    int32_t enabled,
    float radius_meters,
    float floor_meters,
    float ceiling_meters)
{
    protos::MessageSetClip msg{};
    msg.Enabled = enabled;
    msg.ClipRadiusMeters = radius_meters;
    msg.ClipFloorMeters = floor_meters;
    msg.ClipCeilingMeters = ceiling_meters;

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SetClip send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SetLighting(
    uint32_t camera_index,
    float brightness,
    float saturation)
{
    spdlog::info("{} Sending lighting: camera={} brightness={} saturation={}",
        NetLocalName, camera_index, brightness, saturation);

    protos::MessageSetLighting msg{};
    msg.CameraIndex = camera_index;
    msg.Brightness = brightness;
    msg.Saturation = saturation;

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SetLighting send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SetExtrinsics(
    uint32_t camera_index,
    const protos::CameraExtrinsics& extrinsics)
{
    spdlog::info("{} Sending extrinsics: camera={} identity={}",
        NetLocalName, camera_index,
        extrinsics.IsIdentity);

    protos::MessageExtrinsics msg{};
    msg.CameraIndex = camera_index;
    msg.Extrinsics = extrinsics;

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SetExtrinsics send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SetCompression(const protos::CompressionSettings& compression)
{
    spdlog::info("{} Sending compression: color video={} bitrate={} depth video={} denoise={}",
        NetLocalName, (unsigned)compression.ColorVideo, compression.ColorBitrate,
        (unsigned)compression.DepthVideo, (unsigned)compression.DenoisePercent);

    protos::MessageSetCompression msg{};
    msg.Settings = compression;

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SetCompression send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SendSetCompression(protos::MessageSetCompression& compression)
{
    tonk::SDKResult send_result = Send(&compression, sizeof(compression), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SendSetClip send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SendSetMode(protos::Modes mode)
{
    protos::MessageSetMode msg{};
    msg.Mode = static_cast<uint8_t>( mode );

    tonk::SDKResult send_result = Send(&msg, sizeof(msg), protos::kChannelControl);
    if (!send_result) {
        spdlog::error("{} SendSetMode send failed: {}", NetLocalName, send_result.ToString());
    }
}

void CaptureConnection::SendConnectName(const std::string& name)
{
    // Get a list of active guids
    std::vector<uint64_t> guids;
    auto connections = Client->Connections.GetList();
    for (auto conn : connections)
    {
        const uint64_t guid = conn->ServerGuid;
        guids.push_back(guid);
    }

    size_t msg_len = sizeof(protos::MessageConnectName) + sizeof(uint64_t) * guids.size();
    std::unique_ptr<uint8_t> msg_ptr(new uint8_t[msg_len]);
    protos::MessageConnectName* msg = reinterpret_cast<protos::MessageConnectName*>(msg_ptr.get());

    msg->Type = protos::MessageType_ConnectName;
    msg->IgnoreGuidCount = static_cast<uint16_t>( guids.size() );
    SafeCopyCStr(msg->Name, sizeof(msg->Name), name.c_str());

    uint64_t* guids_ptr = reinterpret_cast<uint64_t*>( msg_ptr.get() + sizeof(protos::MessageConnectName) );
    for (size_t i = 0; i < guids.size(); ++i) {
        guids_ptr[i] = guids[i];
    }

    tonk::SDKResult send_result = Send(msg_ptr.get(), msg_len, protos::kChannelRendezvous);
    if (!send_result) {
        spdlog::error("{} SendConnectName send failed: {}", NetLocalName, send_result.ToString());
    }
}


//------------------------------------------------------------------------------
// NetClient

bool NetClient::Initialize(
    std::shared_ptr<DejitterQueue> playback_queue,
    const char* server_address,
    int32_t server_port,
    const char* server_name,
    const char* password)
{
    PlaybackQueue = playback_queue;
    ServerAddress = server_address;
    ServerPort = static_cast<uint16_t>( server_port );
    ServerName = server_name;
    Password = password;

    if (sodium_init() < 0) {
        spdlog::error("sodium_init failed");
        return false;
    }

    // Set netcode configuration
    tonk::SDKSocket::Config.UDPListenPort = 0;
    tonk::SDKSocket::Config.MaximumClients = 10;
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

    Terminated = false;
    Thread = std::make_shared<std::thread>(&NetClient::Loop, this);
    return true;
}

void NetClient::Shutdown()
{
    const uint64_t t0 = GetTimeUsec();

    spdlog::info("NetClient::Shutdown started: Terminating background thread...");

    Terminated = true;
    JoinThread(Thread);

    spdlog::info("Destroying socket...");

    tonk::SDKSocket::BlockingDestroy();

    const uint64_t t1 = GetTimeUsec();

    spdlog::info("NetClient shutdown complete in {} msec", (t1 - t0) / 1000.f);
}

bool NetClient::CheckDirectConnectUnique(CaptureConnection* connection)
{
    const uint64_t server_guid = connection->ServerGuid;
    auto connections = Connections.GetList();
    for (auto& other : connections)
    {
        if (other.get() != connection && other->ServerGuid == server_guid) {
            spdlog::info("Direct connection achieved twice with the same guid={}", server_guid);
            return false;
        }
    }
    return true;
}

void NetClient::OnConnectionClosed(CaptureConnection* connection)
{
    // Clear favorite connection if needed
    {
        std::lock_guard<std::mutex> locker(Lock);
        if (connection == PrimaryConnection.get()) {
            PrimaryConnection.reset();
        }
    }

    Connections.Remove(connection);
}

tonk::SDKConnection* NetClient::OnIncomingConnection(
    const TonkAddress& address ///< Address of the client requesting a connection
)
{
    CORE_UNUSED(address);
    spdlog::error("Rejected incoming connection: We only accept peer2peer connections");
    return nullptr;
}

tonk::SDKConnection* NetClient::OnP2PConnectionStart(
    const TonkAddress& address ///< Address of the other peer we are to connect to
)
{
    CORE_UNUSED(address);
    auto ptr = std::make_shared<CaptureConnection>(this);

    // Insert into connection list to prevent it from going out of scope
    Connections.Insert(ptr.get());

    return ptr.get();
}

void NetClient::OnConnect(CaptureConnection* connection)
{
    Connections.Insert(connection);
}

void NetClient::Loop()
{
    SetCurrentThreadName("NetClient::Loop");

    uint64_t last_connect_usec = 0;

    while (!Terminated)
    {
        const uint64_t now_usec = GetTimeUsec();
        if (now_usec - last_connect_usec < 2 * 1000 * 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        last_connect_usec = now_usec;

        bool needs_connection = false;
        {
            std::lock_guard<std::mutex> locker(Lock);
            if (!PrimaryConnection) {
                needs_connection = true;
            }
        }
        if (needs_connection)
        {
            spdlog::info("Reconnecting...");

            auto connection = std::make_shared<CaptureConnection>(this);
            {
                std::lock_guard<std::mutex> locker(Lock);
                PrimaryConnection = connection;
            }

            tonk::SDKJsonResult result = tonk::SDKSocket::Connect(
                connection.get(),
                ServerAddress,
                ServerPort);
            if (!result) {
                spdlog::error("Connect failed fast: {}", result.ToString());
            } else {
                spdlog::info("Connection started with {} : {}", ServerAddress, ServerPort);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


} // namespace core
