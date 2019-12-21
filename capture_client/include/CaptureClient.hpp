// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "capture_client.h"
#include "NetClient.hpp"
#include "DejitterQueue.hpp"
#include "FileReader.hpp"
#include "FileWriter.hpp"

#include <mutex>

namespace core {


//------------------------------------------------------------------------------
// Tools

struct MappedPerspective
{
    uint64_t Guid = 0;
    unsigned CameraIndex = 0;
    uint32_t FrameNumber = 0;
};


//------------------------------------------------------------------------------
// CaptureClient

class CaptureClient
{
public:
    // API functions
    void Connect(
        const char* server_address,
        int32_t server_port,
        const char* server_name,
        const char* password);
    void Get(XrcapFrame* frame, XrcapStatus* status);
    void SetServerCaptureMode(int mode);
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
        uint64_t guid,
        uint32_t camera_index,
        const protos::CameraExtrinsics& extrinsics);
    void SetCompression(const protos::CompressionSettings& compression);
    void PlaybackSettings(uint32_t dejitter_queue_msec);
    void SetLighting(
        uint64_t guid,
        uint32_t camera_index,
        float brightness,
        float saturation);
    void Reset();
    void PlaybackTricks(bool pause, bool loop_repeat);
    bool PlaybackReadFile(const char* file_path);
    void PlaybackAppend(const void* data, unsigned bytes);
    void GetPlaybackState(XrcapPlayback& playback_state);
    void PlaybackSeek(uint64_t video_usec);
    bool Record(const char* file_path);
    void RecordPause(bool pause);
    void GetRecordingState(XrcapRecording& recording_state);
    void Shutdown();

protected:
    std::mutex ApiLock;
    std::shared_ptr<NetClient> Client;

    // Used to allow Connect() to run repeatedly
    std::string ServerAddress;
    int32_t ServerPort = 0;
    std::string ServerName;
    std::string Password;

    // Used to allow SetServerCaptureMode() to run repeatedly
    int LastMode = -1;

    std::shared_ptr<DejitterQueue> PlaybackQueue;

    // Frame data pinned for application
    std::mutex FrameLock;
    std::shared_ptr<DecodedBatch> PinnedBatch;
    std::shared_ptr<DecodedBatch> LatestBatch;

    // Frame number that increments once for each frame for display
    uint32_t FrameNumber = 0;

    UnixTimeConverter TimeConverter;

    MappedPerspective PerspectiveMap[XRCAP_PERSPECTIVE_COUNT];

    std::unique_ptr<FileReader> Reader;

    std::mutex WriterLock;
    std::unique_ptr<FileWriter> Writer;

    // Lock for recording state data
    std::mutex RecordingStateLock;
    XrcapRecording RecordingState{};

    void GetFrame(XrcapFrame* frame);
    void PlayFrame(std::shared_ptr<DecodedBatch>& batch);
    unsigned GetPerspectiveIndex(std::shared_ptr<DecodedFrame>& frame);
};


} // namespace core
