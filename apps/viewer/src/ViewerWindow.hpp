// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include <core_logging.hpp> // core
#include <capture_client.h> // capture_client
#include <core_nuklear.hpp> // nuklear
#include <ImageTilingRender.hpp> // glad
#include <VideoMeshRender.hpp> // glad
#include <CameraExtrinsics.hpp> // depth_mesh
#include <ColorNormalization.hpp> // depth_mesh
#include <TrackballCamera.hpp>

#include "ViewerSettings.hpp"

#include <atomic>
#include <thread>
#include <memory>

namespace core {


//------------------------------------------------------------------------------
// Constants

enum class CalibrationState
{
    Idle,
    FindingMarker,
    Processing,
};


//------------------------------------------------------------------------------
// ViewerWindow

class ViewerWindow
{
public:
    void Initialize(const std::string& file_path);
    void Shutdown();

    bool IsTerminated() const
    {
        return Terminated;
    }

protected:
    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    GLFWwindow* Window = nullptr;

    std::atomic<bool> IsIconified = ATOMIC_VAR_INIT(false);

    nk_context* NuklearContext = nullptr;
    nk_colorf BackgroundColor{};

    NV12VideoMeshRender MeshRenderer[XRCAP_PERSPECTIVE_COUNT];

    ImageTilingRenderer ImageTileRender;

    std::mutex FrameLock;
    bool FrameInUse = false;
    XrcapFrame LastFrame{};
    XrcapStatus LastStatus{};

    ViewerSettings Settings;

    bool RenderPaused = false;

    int ShowMeshCheckValue = 0;

    bool EnableRawStorage = false;

    std::atomic<bool> ExtrinsicsCalibrationRequested = ATOMIC_VAR_INIT(false);
    std::atomic<bool> FullCalibrationRequested = ATOMIC_VAR_INIT(false);
    std::atomic<CalibrationState> CalibState = ATOMIC_VAR_INIT(CalibrationState::Idle);
    std::shared_ptr<std::thread> CalibThread;

    // Size of clipping cylinder in meters
    float ClipRadiusMeters = 1.5f;
    float ClipFloorMeters = -0.5f;
    float ClipCeilingMeters = 2.2f;
    int ClipEnabled = 0;

    int AutoExposureValue = 1;

    bool LightingLocked = false;
    std::atomic<bool> LightingCalibrationRequested = ATOMIC_VAR_INIT(false);

    std::shared_ptr<std::thread> LightCalibThread;
    std::mutex LightLock;
    std::vector<std::shared_ptr<KdtreePointCloud>> LightClouds;

    TrackballCamera Camera;

    int ColorBitrate = 4000000;
    int ColorQuality = 25;
    int ColorVideo = XrcapVideo_H264;
    int DepthVideo = XrcapVideo_Lossless;
    int DenoisePercent = 100;
    int CullImages = 0;
    int FacePaintingFix = 0;

    int PhotoboothEnabled = 0;

    int PlaybackQueueDepth = 500; // msec

    bool IsLivePlayback = false;
    bool IsFileOpen = false;
    int FileLoopEnabled = 1;

    uint64_t PhotoboothStartMsec = 0;

    int DracoCompressionEnabled = 0;
    int GltfJpegQuality = 90;


    void ResetLighting();

    void Loop();
    void CalibLoop();
    void LightCalibLoop();

    void StartRender();
    void Render();
    void RenderMeshes();
    void SetupUI();
    void StopRender();

    void OnMouseMove(double x, double y);
    void OnMouseDown(int button, double x, double y);
    void OnMouseUp(int button);
    void OnMouseScroll(double x, double y);

    void OnKey(int key, bool press);

    void LoadMeshAndTest();

    void OpenFile();
    void CloseFile();

    void OpenRecordingFile();
    void CloseRecordingFile();
    void StartRecording();
    void PauseRecording();

    void SaveGltf();
};


} // namespace core
