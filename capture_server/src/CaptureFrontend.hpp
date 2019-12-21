// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    UI for the capture server
*/

#pragma once

#include <core_nuklear.hpp> // nuklear
#include <CaptureManager.hpp> // capture
#include <VideoMeshRender.hpp> // glad
#include <ImageTilingRender.hpp> // glad
#include <TrackballCamera.hpp> // glad

#include "CaptureServer.hpp"
#include "ServerSettings.hpp"

#include <atomic>
#include <thread>
#include <memory>

namespace core {


//------------------------------------------------------------------------------
// CaptureFrontend

class CaptureFrontend
{
public:
    void Initialize();
    void Shutdown();

    bool IsTerminated() const
    {
        return Terminated;
    }

protected:
    std::atomic<bool> Terminated = ATOMIC_VAR_INIT(false);
    std::shared_ptr<std::thread> Thread;

    RuntimeConfiguration RuntimeConfig;
    CaptureManager Capture;

    std::mutex ServerLock;
    std::shared_ptr<CaptureServer> Server;
    int ServerPort = 0;

    GLFWwindow* Window = nullptr;

    std::atomic<bool> IsIconified = ATOMIC_VAR_INIT(false);

    nk_context* NuklearContext = nullptr;
    nk_colorf BackgroundColor{};

    std::mutex BatchLock;
    std::shared_ptr<ImageBatch> Batch;

    std::string UiPassword;
    ServerSettings NextSettings{};
    ServerSettings Settings{};

    uint64_t IntervalStartUsec = 0;
    std::atomic<unsigned> IntervalFrameCounter = 0;
    float ReceivedFramerate = 0.f;

    bool RenderPaused = false;
    std::shared_ptr<ImageBatch> PausedBatch;

    int ShowPreviewCheckValue = 1;
    int ShowMeshCheckValue = 0;
    int MultiServerCheckValue = 0;

    ImageTilingRenderer ImageTileRender;

    static const int kMaxMeshes = 4;
    NV12VideoMeshRender MeshRender[kMaxMeshes];

    TrackballCamera Camera;


    void Loop();

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

    void OnImageBatch(std::shared_ptr<ImageBatch>& batch);

    void UpdatePasswordHashFromUi();
    void ApplyNetworkSettings();
};


} // namespace core
