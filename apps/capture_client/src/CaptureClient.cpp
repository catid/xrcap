// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureClient.hpp"

#include <core_string.hpp>

namespace core {


//------------------------------------------------------------------------------
// CaptureClient

static std::atomic<bool> IsSetTonkLogCallback = ATOMIC_VAR_INIT(false);

void CaptureClient::Connect(
    const char* server_address,
    int32_t server_port,
    const char* server_name,
    const char* password)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!IsSetTonkLogCallback) {
        SetTonkLogCallback([](const std::string& msg) {
            spdlog::debug("Tonk: {}", msg);
        });
        IsSetTonkLogCallback = true;
    }

    if (Client) {
        // If settings did not change:
        if (0 == StrCaseCompare(server_address, ServerAddress.c_str()) &&
            server_port == ServerPort &&
            0 == StrCaseCompare(server_name, ServerName.c_str()) &&
            0 == StrCaseCompare(password, Password.c_str()))
        {
            return;
        }

        spdlog::info("Resetting connection on Connect() with new parameters");
        Client->Shutdown();
        Client.reset();

        // Unhook from old playback queue (let it go out of scope in the background)
        PlaybackQueue.reset();
    }

    if (!PlaybackQueue)
    {
        PlaybackQueue = std::make_shared<DejitterQueue>();

        PlaybackQueue->Initialize([this](std::shared_ptr<DecodedBatch>& batch)
        {
            PlayFrame(batch);
        });
    }

    // Remember settings to keep API simple
    ServerAddress = server_address;
    ServerPort = server_port;
    ServerName = server_name;
    Password = password;
    LastMode = -1;

    Client = std::make_shared<NetClient>();

    const bool result = Client->Initialize(
        PlaybackQueue,
        server_address,
        server_port,
        server_name,
        password);

    if (!result) {
        Client->Shutdown();
        Client.reset();
        return;
    }

    spdlog::info("Connection started...");
}

void CaptureClient::PlayFrame(std::shared_ptr<DecodedBatch>& batch)
{
    if (batch->Frames.empty()) {
        return;
    }

    batch->EpochUsec = TimeConverter.Convert(batch->VideoBootUsec);

    {
        std::lock_guard<std::mutex> locker(FrameLock);
        LatestBatch = batch;
    }

    std::lock_guard<std::mutex> locker(WriterLock);

    // If we are writing data to disk:
    if (Writer && Writer->IsOpen())
    {
        {
            std::lock_guard<std::mutex> locker(RecordingStateLock);
            if (RecordingState.Paused) {
                return;
            }
        }

        Writer->WriteDecodedBatch(batch);

        {
            std::lock_guard<std::mutex> locker(RecordingStateLock);
            RecordingState.FileSizeBytes = Writer->GetFileBytes();
            RecordingState.VideoFrameCount = Writer->GetFrameCount();
            RecordingState.VideoDurationUsec = Writer->GetDurationUsec();
        }
    }
}

void CaptureClient::Get(XrcapFrame* frame, XrcapStatus* status)
{
    if (frame) {
        *frame = XrcapFrame();
        frame->Valid = 0;
        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i) {
            frame->Perspectives[i].Valid = 0;
        }
    }
    if (status) {
        *status = XrcapStatus();
        status->State = XrcapStreamState_Idle;
        status->Mode = XrcapStreamMode_Disabled;
        status->CameraCount = 0;
        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i) {
            status->CameraCodes[i] = XrcapCameraCodes_Idle;
        }
        status->BitsPerSecond = 0;
        status->CaptureStatus = XrcapCaptureStatus_Idle;
    }

    std::lock_guard<std::mutex> locker(ApiLock);

    status->State = XrcapStreamState_Idle;
    status->BitsPerSecond = 0;
    status->PacketlossRate = 0.f;
    status->TripUsec = 0;
    status->CameraCount = 0;

    if (Client)
    {
        auto connections = Client->Connections.GetList();
        for (auto& conn : connections)
        {
            auto status_ex = conn->GetStatusEx();
            status->BitsPerSecond += status_ex.IncomingBPS * 8;
            if (status_ex.IncomingLossRate > status->PacketlossRate) {
                status->PacketlossRate = status_ex.IncomingLossRate;
            }
            if (status_ex.TripUsec > status->TripUsec) {
                status->TripUsec = status_ex.TripUsec;
            }

            auto camera_status = conn->GetLastStatus();
            for (unsigned i = 0; i < camera_status.CameraCount; ++i)
            {
                if (camera_status.CameraCount >= XRCAP_PERSPECTIVE_COUNT) {
                    break;
                }
                status->CameraCodes[status->CameraCount++] = camera_status.CameraStatus[i];
            }

            // FIXME: Report per-server status
            if (status->Mode < camera_status.Mode) {
                status->Mode = camera_status.Mode;
            }
            if (status->CaptureStatus < camera_status.CaptureStatus) {
                status->CaptureStatus = camera_status.CaptureStatus;
            }
            const XrcapStreamState state = conn->State;
            if (status->State < state) {
                status->State = state;
            }
        }
    }

    GetFrame(frame);
}

void CaptureClient::GetFrame(XrcapFrame* output_frame)
{
    std::lock_guard<std::mutex> locker(FrameLock);
    if (!LatestBatch) {
        memset(output_frame, 0, sizeof(XrcapFrame));
        //frame->Valid = 0;
        return;
    }

    PinnedBatch = LatestBatch;

    output_frame->Valid = 1;
    output_frame->FrameNumber = FrameNumber++;
    output_frame->ExposureEpochUsec = PinnedBatch->EpochUsec;
    output_frame->VideoStartUsec = PinnedBatch->VideoBootUsec;
    for (auto& image : PinnedBatch->Frames)
    {
        const unsigned perspective_index = GetPerspectiveIndex(image);
        auto& perspective = output_frame->Perspectives[perspective_index];

        perspective.Valid = 1;

        perspective.Y = image->Y;
        perspective.UV = image->UV;
        perspective.Width = image->Width;
        perspective.Height = image->Height;
        perspective.ChromaWidth = image->ChromaWidth;        
        perspective.ChromaHeight = image->ChromaHeight;

        perspective.Indices = image->Indices.data();
        perspective.IndicesCount = image->IndicesCount;
        perspective.XyzuvVertices = image->XyzuvVertices.data();
        perspective.FloatsCount = image->FloatsCount;

        auto& frame_header = image->Info->FrameHeader;
        for (int i = 0; i < 3; ++i) {
            perspective.Accelerometer[i] = frame_header.Accelerometer[i];
        }
        perspective.ExposureUsec = frame_header.ExposureUsec;
        perspective.AutoWhiteBalanceUsec = frame_header.AutoWhiteBalanceUsec;
        perspective.ISOSpeed = frame_header.ISOSpeed;
        perspective.CameraIndex = frame_header.CameraIndex;
        perspective.Brightness = frame_header.Brightness;
        perspective.Saturation = frame_header.Saturation;

        perspective.Guid = image->Info->Guid;
        perspective.Calibration = (XrcapCameraCalibration*)image->Info->Calibration.get();
        perspective.Extrinsics = (XrcapExtrinsics*)image->Info->Extrinsics.get();
        if (!perspective.Extrinsics) {
            static XrcapExtrinsics identity = { 1 };
            perspective.Extrinsics = &identity;
        }
    }
}

unsigned CaptureClient::GetPerspectiveIndex(std::shared_ptr<DecodedFrame>& frame)
{
    const uint64_t guid = frame->Info->Guid;
    const unsigned camera_index = frame->Info->FrameHeader.CameraIndex;

    uint32_t oldest_delta = 0;
    unsigned oldest_index = 0;

    // Note this assigns to high numbers first which helps shake out client-side bugs
    for (unsigned i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
    {
        auto& perspective = PerspectiveMap[i];
        uint32_t delta = FrameNumber - perspective.FrameNumber;
        if (delta >= oldest_delta) {
            oldest_delta = delta;
            oldest_index = i;
        }

        if (perspective.Guid == guid && perspective.CameraIndex == camera_index) {
            perspective.FrameNumber = FrameNumber;
            return i;
        }
    }

    auto& perspective = PerspectiveMap[oldest_index];
    perspective.Guid = guid;
    perspective.CameraIndex = camera_index;
    perspective.FrameNumber = FrameNumber;
    return oldest_index;
}

void CaptureClient::SetServerCaptureMode(int mode)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (LastMode == mode) {
        return;
    }
    LastMode = mode;

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }
        conn->SendSetMode((protos::Modes)mode);
    }
}

void CaptureClient::SetExposure(
    int32_t auto_enabled,
    uint32_t exposure_usec,
    uint32_t awb_usec)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }

        conn->SetExposure(
            auto_enabled,
            exposure_usec,
            awb_usec);
    }
}

void CaptureClient::SetClip(
    int32_t enabled,
    float radius_meters,
    float floor_meters,
    float ceiling_meters)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }

        conn->SetClip(
            enabled,
            radius_meters,
            floor_meters,
            ceiling_meters);
    }
}

void CaptureClient::SetExtrinsics(
    uint64_t guid,
    uint32_t camera_index,
    const protos::CameraExtrinsics& extrinsics)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }

        if (conn->ServerGuid == guid) {
            conn->SetExtrinsics(camera_index, extrinsics);
        }
    }
}

void CaptureClient::SetCompression(const protos::CompressionSettings& compression)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }

        conn->SetCompression(compression);
    }
}

void CaptureClient::PlaybackSettings(uint32_t dejitter_queue_msec)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client || !Client->PlaybackQueue) {
        return;
    }

    Client->PlaybackQueue->SetQueueDepth(dejitter_queue_msec);
}

void CaptureClient::SetLighting(
    uint64_t guid,
    uint32_t camera_index,
    float brightness,
    float saturation)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!Client) {
        return;
    }

    auto connections = Client->Connections.GetList();
    for (auto& conn : connections)
    {
        if (!conn->IsAuthenticated) {
            continue;
        }

        if (conn->ServerGuid == guid) {
            conn->SetLighting(camera_index, brightness, saturation);
        }
    }
}

void CaptureClient::Reset()
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (Client) {
        spdlog::info("API: Shutdown started");
        Client->Shutdown();
        Client.reset();
        spdlog::info("API: Shutdown complete");
        LastMode = -1;
    }
    Reader.reset();
    PlaybackQueue.reset();
    //PinnedBatch.reset(); - explicitly not invalidated
    LatestBatch.reset();
}

void CaptureClient::PlaybackTricks(bool pause, bool loop_repeat)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (Reader) {
        Reader->Pause(pause);
        Reader->SetLoopRepeat(loop_repeat);
    }
}

bool CaptureClient::PlaybackReadFile(const char* file_path)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (!PlaybackQueue)
    {
        PlaybackQueue = std::make_shared<DejitterQueue>();

        PlaybackQueue->Initialize([this](std::shared_ptr<DecodedBatch>& batch)
        {
            PlayFrame(batch);
        });
    }

    Reader.reset();
    Reader = std::make_unique<FileReader>();
    return Reader->Open(PlaybackQueue, file_path);
}

void CaptureClient::PlaybackAppend(const void* data, unsigned bytes)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    // FIXME
}

void CaptureClient::GetPlaybackState(XrcapPlayback& playback_state)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (Reader) {
        Reader->GetPlaybackState(playback_state);
    } else {
        memset(&playback_state, 0, sizeof(playback_state));
        playback_state.State = XrcapPlaybackState_Idle;
    }
}

void CaptureClient::PlaybackSeek(uint64_t video_usec)
{
    std::lock_guard<std::mutex> locker(ApiLock);

    // FIXME
}

bool CaptureClient::Record(const char* file_path)
{
    std::lock_guard<std::mutex> locker(WriterLock);
    std::lock_guard<std::mutex> locker2(RecordingStateLock);

    Writer.reset();

    // Reset state
    RecordingState.RecordingFileOpen = 0;
    RecordingState.Paused = 1;
    RecordingState.VideoDurationUsec = 0;
    RecordingState.VideoFrameCount = 0;
    RecordingState.FileSizeBytes = 0;

    if (file_path == nullptr || *file_path == '\0') {
        return true; // Call always succeeds if user is trying to close file
    }

    Writer = std::make_unique<FileWriter>();
    if (!Writer->Open(file_path)) {
        return false;
    }

    RecordingState.RecordingFileOpen = 1;

    return true;
}

void CaptureClient::RecordPause(bool pause)
{
    //std::lock_guard<std::mutex> locker(ApiLock);
    std::lock_guard<std::mutex> locker(RecordingStateLock);
    bool new_state = pause ? 1 : 0;
    if (RecordingState.Paused != new_state)
    {
        RecordingState.Paused = new_state;

        if (!pause) {
            if (!Client) {
                return;
            }

            spdlog::info("Requesting keyframe on unpausing recording");

            auto connections = Client->Connections.GetList();
            for (auto& conn : connections)
            {
                if (!conn->IsAuthenticated) {
                    continue;
                }

                conn->SendKeyframeRequest();
            }
        }
    }
}

void CaptureClient::GetRecordingState(XrcapRecording& recording_state)
{
    //std::lock_guard<std::mutex> locker(ApiLock);
    std::lock_guard<std::mutex> locker(RecordingStateLock);
    recording_state = RecordingState;
}

void CaptureClient::Shutdown()
{
    std::lock_guard<std::mutex> locker(ApiLock);

    if (Client) {
        spdlog::info("API: Shutdown started");
        Client->Shutdown();
        Client.reset();
        spdlog::info("API: Shutdown complete");
        LastMode = -1;
    }
    Reader.reset();

    {
        std::lock_guard<std::mutex> locker(WriterLock);
        Writer.reset();
    }
    PlaybackQueue.reset();
    PinnedBatch.reset();
    LatestBatch.reset();
}


} // namespace core
