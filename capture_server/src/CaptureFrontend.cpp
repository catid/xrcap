// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CaptureFrontend.hpp"
#include <core_logging.hpp>
#include <core_string.hpp>

#include <crypto_spake.h>
#include <sodium.h>
#include <xxhash.h>

#include "AppIcon.inc"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace core {


//------------------------------------------------------------------------------
// Constants

static const struct nk_color nk_red = {255,0,0,255};
static const struct nk_color nk_green = {0,255,0,255};
static const struct nk_color nk_blue = {0,0,255,255};
static const struct nk_color nk_white = {255,255,255,255};
static const struct nk_color nk_black = {0,0,0,255};
static const struct nk_color nk_yellow = {255,255,0,255};


//------------------------------------------------------------------------------
// CaptureFrontend

void CaptureFrontend::Initialize()
{
    if (sodium_init() < 0) {
        spdlog::error("sodium_init failed");
    }

    auto on_batch = [&](std::shared_ptr<ImageBatch>& batch)
    {
        ++IntervalFrameCounter;

        {
            std::lock_guard<std::mutex> locker(BatchLock);
            Batch = batch;
        }

        std::shared_ptr<CaptureServer> server;
        {
            std::lock_guard<std::mutex> locker(ServerLock);
            server = Server;
        }
        if (server) {
            server->BroadcastVideo(batch);
        }
    };

    Capture.Initialize(&RuntimeConfig, on_batch);
    Capture.SetMode(CaptureMode::Disabled);

    if (!LoadFromFile(GetSettingsFilePath("xrcap", CAPTURE_SERVER_DEFAULT_SETTINGS), Settings)) {
        spdlog::warn("Failed to load capture configuration");
    }
    NextSettings = Settings;

    ApplyNetworkSettings();

    Terminated = false;
    Thread = std::make_shared<std::thread>(&CaptureFrontend::Loop, this);
}

void CaptureFrontend::UpdatePasswordHashFromUi()
{
    if (!UiPassword.empty()) {
        spdlog::info("Updating password hash");
        // FIXME: Do not log the password to disk
        //spdlog::info("Password from application: `{}` len={}", UiPassword, UiPassword.size());
        const uint64_t t0 = GetTimeUsec();
        Settings.ServerPasswordHash.clear();
        uint8_t stored_data[crypto_spake_STOREDBYTES];

        const int ret = crypto_spake_server_store(
            stored_data,
            UiPassword.c_str(),
            UiPassword.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE);
        if (ret != 0) {
            spdlog::error("crypto_spake_server_store failed");
        } else {
            spdlog::info("H(stored_data):{}",
                HexString(XXH64(stored_data, sizeof(stored_data), 0)));

            const int count = GetBase64LengthFromByteCount(crypto_spake_STOREDBYTES);
            std::vector<char> str(count + 1);
            const int b64len = WriteBase64Str(
                stored_data,
                sizeof(stored_data),
                str.data(),
                count + 1);
            if (b64len > 0) {
                Settings.ServerPasswordHash = str.data();
                const uint64_t t1 = GetTimeUsec();
                spdlog::info("Updating password hash updated in {} msec", (t1 - t0) / 1000.f);
            } else {
                spdlog::error("WriteBase64Str for password hash failed");
            }
        }

        UiPassword.clear();
    }
}

void CaptureFrontend::ApplyNetworkSettings()
{
    std::lock_guard<std::mutex> locker(ServerLock);
    if (Server) {
        Server->Shutdown();
        Server.reset();
    }

    // For multiple servers we need to query the TDMA slots for depth exposure from rendezvous server.
    Capture.EnableTmdaMode(Settings.EnableMultiServers);

    Server = std::make_shared<CaptureServer>();
    const bool init_result = Server->Initialize(
        &Capture,
        Settings.ServerName,
        Settings.RendezvousServerHostname,
        Settings.RendezvousServerPort,
        Settings.ServerPasswordHash,
        Settings.ServerUdpPort,
        Settings.EnableMultiServers);
    if (!init_result) {
        spdlog::error("Failed to initialize server!");
        Server = nullptr;
    }
}

void CaptureFrontend::Shutdown()
{
    Terminated = true;
    JoinThread(Thread);

    Batch.reset();
    Capture.Shutdown();

    std::lock_guard<std::mutex> locker(ServerLock);
    if (Server) {
        Server->Shutdown();
        Server = nullptr;
    }
}

void CaptureFrontend::Loop()
{
    SetCurrentThreadName("CaptureFrontend");

    ScopedFunction term_scope([&]() { Terminated = true; });

    const int init_result = glfwInit();
    if (init_result != GLFW_TRUE) {
        spdlog::error("glfwInit failed");
        return;
    }
    ScopedFunction init_scope([&]() { glfwTerminate(); });

    spdlog::info("GLFW version: {}", glfwGetVersionString());

    glfwSetErrorCallback([](int err_code, const char* err_string) {
        spdlog::error("GLFW error #{}: {}", err_code, err_string);
    });

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // This apparently helps on OSX
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); // Does it have a border/titlebar?
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    Window = glfwCreateWindow(
        1440,
        960,
        "RGBD Capture Server",
        nullptr,
        nullptr);
    if (!Window) {
        spdlog::error("glfwCreateWindow failed");
        return;
    }
    ScopedFunction window_scope([&]() {
        glfwDestroyWindow(Window);
        Window = nullptr;
    });

    GLFWimage icon;
    icon.pixels = stbi_load_from_memory(
        icons8_futurama_nibbler_64_png,
        icons8_futurama_nibbler_64_png_len,
        &icon.width,
        &icon.height,
        nullptr,
        4); // RGBA
    glfwSetWindowIcon(Window, 1, &icon);
    free(icon.pixels);

    glfwMakeContextCurrent(Window);
    ScopedFunction context_scope([&]() {
        glfwMakeContextCurrent(nullptr);
    });

    // Load GL: Must be done after glfwMakeContextCurrent()
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize OpenGL context");
        return;
    }
    spdlog::info("GLAD loaded OpenGL version {}.{}", GLVersion.major, GLVersion.minor);

    glfwSetWindowUserPointer(Window, this);

    glfwSetCursorPosCallback(Window, [](GLFWwindow* window, double xpos, double ypos) {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        thiz->OnMouseMove(xpos, ypos);
    });
    glfwSetMouseButtonCallback(Window, [](GLFWwindow* window, int button, int action, int mods) {
        CORE_UNUSED(mods);
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        if (action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            thiz->OnMouseDown(button, x, y);
        }
        else if (action == GLFW_RELEASE) {
            thiz->OnMouseUp(button);
        }

        // Forward to Nuklear
        nk_glfw3_mouse_button_callback(window, button, action, mods);
    });
    glfwSetScrollCallback(Window, [](GLFWwindow* window, double xpos, double ypos) {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        thiz->OnMouseScroll(xpos, ypos);

        // Forward to Nuklear
        nk_gflw3_scroll_callback(window, xpos, ypos);
    });

    glfwSetKeyCallback(Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        CORE_UNUSED(scancode);
        CORE_UNUSED(mods);

        // If a key was pressed:
        if (action == GLFW_PRESS) {
            thiz->OnKey(key, true);
        }
        else if (action == GLFW_RELEASE) {
            thiz->OnKey(key, false);
        }
    });

    glfwSetCharCallback(Window, [](GLFWwindow* window, unsigned codepoint) {
        // Forward to Nuklear
        nk_glfw3_char_callback(window, codepoint);
    });

    glfwSetWindowSizeCallback(Window, [](GLFWwindow* window, int width, int height)
    {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        CORE_UNUSED(thiz);

        spdlog::trace("Window client area resized: {}x{}", width, height);
    });

    glfwSetFramebufferSizeCallback(Window, [](GLFWwindow* window, int width, int height)
    {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        CORE_UNUSED(thiz);

        spdlog::trace("Framebuffer resized: {}x{}", width, height);

        glViewport(0, 0, width, height);
    });

    glfwSetWindowIconifyCallback(Window, [](GLFWwindow* window, int iconified)
    {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );
        thiz->IsIconified = (iconified != 0);

        spdlog::info("Iconified: {}", thiz->IsIconified);
    });

    glfwSetWindowRefreshCallback(Window, [](GLFWwindow* window)
    {
        CaptureFrontend* thiz = reinterpret_cast<CaptureFrontend*>( glfwGetWindowUserPointer(window) );

        thiz->Render();
    });

    // Wait for V-sync
    glfwSwapInterval(1);

    int width = 0, height = 0;
    glfwGetFramebufferSize(Window, &width, &height);
    glViewport(0, 0, width, height);

    StartRender();

    // While window is still open:
    while (!glfwWindowShouldClose(Window) && !Terminated)
    {
        if (!IsIconified)
        {
            Render();
            RuntimeConfig.ImagesNeeded = (ShowPreviewCheckValue != 0);
        } else {
            RuntimeConfig.ImagesNeeded = false;
            // Wait for window to be restored
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        const uint64_t now_usec = GetTimeUsec();
        const uint64_t interval_usec = now_usec - IntervalStartUsec;
        if (now_usec - IntervalStartUsec > 1000000) {
            const unsigned counter = IntervalFrameCounter.exchange(0);
            ReceivedFramerate = counter * (1000000.f / static_cast<float>( interval_usec ));
            IntervalStartUsec = now_usec;
        }

        // This function blocks sometimes during resize, so we should watch
        // for the window refresh callback and render there too.
        glfwPollEvents();
    }

    Terminated = true;

    StopRender();
}

void CaptureFrontend::StartRender()
{
    if (!ImageTileRender.Initialize()) {
        spdlog::error("Image tile render initialization failed");
    }
    for (int i = 0; i < kMaxMeshes; ++i) {
        if (!MeshRender[i].Initialize()) {
            spdlog::error("Mesh render {} initialization failed", i);
        }
    }

    Capture.SetMode(CaptureMode::CaptureLowQual);

    BackgroundColor.r = 0.10f;
    BackgroundColor.g = 0.18f;
    BackgroundColor.b = 0.24f;
    BackgroundColor.a = 1.0f;

    NuklearContext = nk_glfw3_init(Window, NK_GLFW3_DEFAULT/*NK_GLFW3_INSTALL_CALLBACKS*/);
    nk_context* ctx = NuklearContext;

    struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&atlas);
    struct nk_font *firacode = nk_font_atlas_add_from_file(
        atlas,
        "FiraCode-Retina.ttf",
        16,
        0);
    nk_glfw3_font_stash_end();
    nk_style_load_all_cursors(ctx, atlas->cursors);
    nk_style_set_font(ctx, &firacode->handle);

    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(20, 20, 20, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(102, 112, 114, 215);
    table[NK_COLOR_HEADER] = nk_rgba(117, 162, 204, 220);
    table[NK_COLOR_BORDER] = nk_rgba(140, 159, 173, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(137, 182, 255, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(142, 187, 229, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(0, 0, 0, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(182, 215, 215, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_SELECT] = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(137, 182, 224, 245);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_EDIT] = nk_rgba(210, 210, 210, 225);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(20, 20, 20, 255);
    table[NK_COLOR_COMBO] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_CHART] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(156, 193, 220, 255);
    nk_style_from_table(ctx, table);
}

void CaptureFrontend::StopRender()
{
    PausedBatch.reset();

    ImageTileRender.Shutdown();
    for (int i = 0; i < kMaxMeshes; ++i) {
        MeshRender[i].Shutdown();
    }

    Capture.SetMode(CaptureMode::Disabled);

    nk_glfw3_shutdown();
}

void CaptureFrontend::SetupUI()
{
#if 0
    float xscale = 1.f, yscale = 1.f;
    GLFWmonitor* monitor = glfwGetWindowMonitor(Window);
    if (monitor) {
        glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    }
#endif

    int width = 0, height = 0;
    glfwGetFramebufferSize(Window, &width, &height);

    nk_context* ctx = NuklearContext;
    const nk_colorf bg = BackgroundColor;

    nk_glfw3_new_frame();

    struct nk_rect bounds{};
    if (nk_begin_titled(ctx, "Network", "Network Status", nk_rect(10, 10, 450, 280),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        int capture_server_port = NextSettings.ServerUdpPort;
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Capture Server Port:", NK_TEXT_RIGHT);
        nk_property_int(ctx, "#capture_port", 0, &capture_server_port, 65535, 1, 1.f);
        NextSettings.ServerUdpPort = capture_server_port;

        char rendezvous_server_host[256] = { '\0' };
        SafeCopyCStr(rendezvous_server_host, sizeof(rendezvous_server_host), NextSettings.RendezvousServerHostname.c_str());
        int host_len = static_cast<int>( NextSettings.RendezvousServerHostname.size() );
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Rendezvous Server Hostname:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, rendezvous_server_host, &host_len, 255, nk_filter_ascii);
        rendezvous_server_host[host_len] = 0;
        NextSettings.RendezvousServerHostname = rendezvous_server_host;

        int rendezvous_server_port = NextSettings.RendezvousServerPort;
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Rendezvous Server Port:", NK_TEXT_RIGHT);
        nk_property_int(ctx, "#rendezvous_port", 0, &rendezvous_server_port, 65535, 1, 1.f);
        NextSettings.RendezvousServerPort = rendezvous_server_port;

        char name[256] = { '\0' };
        SafeCopyCStr(name, sizeof(name), NextSettings.ServerName.c_str());
        int name_len = static_cast<int>( NextSettings.ServerName.size() );
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "Capture Name:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, name, &name_len, 255, nk_filter_ascii);
        name[name_len] = 0;
        NextSettings.ServerName = name;

        char password[256] = { '\0' };
        SafeCopyCStr(password, sizeof(password), UiPassword.c_str());
        int password_len = static_cast<int>( UiPassword.size() );
        nk_layout_row_dynamic(ctx, 25, 2);
        nk_label(ctx, "New Password:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, password, &password_len, 255, nk_filter_ascii);
        password[password_len] = 0;
        UiPassword = password;

        nk_layout_row_dynamic(ctx, 20, 1);
        MultiServerCheckValue = NextSettings.EnableMultiServers ? 1 : 0;
        nk_checkbox_label(ctx, "Use Multiple Capture Servers", &MultiServerCheckValue);
        NextSettings.EnableMultiServers = MultiServerCheckValue != 0;

        nk_layout_row_dynamic(ctx, 30, 1);
        if (nk_button_label(ctx, "Apply Settings")) {
            Settings = NextSettings;
            UpdatePasswordHashFromUi();
            if (!SaveToFile(Settings, GetSettingsFilePath("xrcap", CAPTURE_SERVER_DEFAULT_SETTINGS))) {
                spdlog::warn("Failed to save to file");
            }
            ApplyNetworkSettings();
        }

        nk_layout_row_dynamic(ctx, 20, 2);
        nk_label(ctx, "Client count:", NK_TEXT_RIGHT);
        unsigned count = 0;
        {
            std::lock_guard<std::mutex> locker(ServerLock);
            if (Server) {
                count = Server->Connections.GetCount();
            }
        }
        std::string client_count_str = std::to_string(count);
        nk_label_colored(ctx, client_count_str.c_str(), NK_TEXT_LEFT, nk_white);

        bounds = nk_window_get_bounds(ctx);
    }
    nk_end(ctx);
    if (bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.w >= width ||
        bounds.y + bounds.h >= height) {
        if (bounds.x + bounds.w >= width) {
            bounds.x = width - bounds.w;
        }
        if (bounds.x < 0) {
            bounds.x = 0;
        }
        if (bounds.y + bounds.h >= height) {
            bounds.y = height - bounds.h;
        }
        if (bounds.y < 0) {
            bounds.y = 0;
        }
        nk_window_set_bounds(ctx, "Network", bounds);
    }

    if (nk_begin_titled(ctx, "Capture", "Capture Status", nk_rect(10, 490, 320, 240),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 20, 2);
        nk_label(ctx, "Requested mode: ", NK_TEXT_RIGHT);
        const int old_mode = static_cast<int>( RuntimeConfig.Mode.load() );
        const int new_mode = nk_combo_separator(
            ctx,
            "Disable|Calibration|CaptureLowQ|CaptureHighQ",
            '|',
            old_mode,
            static_cast<int>( CaptureMode::Count ),
            40,
            nk_vec2(400.f, 400.f));
        if (old_mode != new_mode) {
            Capture.SetMode(static_cast<CaptureMode>( new_mode ));
        }

        nk_layout_row_dynamic(ctx, 20, 2);
        nk_checkbox_label(ctx, "Show preview", &ShowPreviewCheckValue);
        nk_checkbox_label(ctx, "Show mesh", &ShowMeshCheckValue);

        nk_layout_row_dynamic(ctx, 20, 2);
        nk_label(ctx, "Capture status:", NK_TEXT_RIGHT);
        CaptureStatus status = Capture.GetStatus();
        std::string status_str = CaptureStatusToString(status);
        nk_label_colored(ctx, status_str.c_str(), NK_TEXT_LEFT,
            CaptureStatusFailed(status) ? nk_red : nk_green);

        nk_layout_row_dynamic(ctx, 20, 2);
        nk_label(ctx, "Measured framerate:", NK_TEXT_RIGHT);
        std::string framerate_str = std::to_string(ReceivedFramerate);
        nk_label_colored(ctx, framerate_str.c_str(), NK_TEXT_LEFT,
            ReceivedFramerate <= 0.f ? nk_red : nk_white);

        std::vector<CameraStatus> cameras = Capture.GetCameraStatus();
        const int camera_count = static_cast<int>( cameras.size() );
        for (int camera_index = 0; camera_index < camera_count; ++camera_index)
        {
            nk_layout_row_dynamic(ctx, 20, 2);
            std::string camera_str;
            camera_str += "Camera ";
            camera_str += std::to_string(camera_index);
            camera_str += ":";
            nk_label(ctx, camera_str.c_str(), NK_TEXT_RIGHT);
            CameraStatus camera_status = cameras[camera_index];
            camera_str = CameraStatusToString(camera_status);
            nk_label_colored(ctx, camera_str.c_str(), NK_TEXT_LEFT,
                CameraStatusFailed(camera_status) ? nk_red : nk_green);
        }

        bounds = nk_window_get_bounds(ctx);
    }
    nk_end(ctx);
    if (bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.w >= width ||
        bounds.y + bounds.h >= height) {
        if (bounds.x + bounds.w >= width) {
            bounds.x = width - bounds.w;
        }
        if (bounds.x < 0) {
            bounds.x = 0;
        }
        if (bounds.y + bounds.h >= height) {
            bounds.y = height - bounds.h;
        }
        if (bounds.y < 0) {
            bounds.y = 0;
        }
        nk_window_set_bounds(ctx, "Capture", bounds);
    }

    if (RenderPaused)
    {
        if (nk_begin_titled(ctx, "Playback", "Playback", nk_rect(width * 0.5f, static_cast<float>( height - 100 ), 240, 90),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "PAUSED (Space Bar)", NK_TEXT_CENTERED);
        }
        nk_end(ctx);
    }
}

void CaptureFrontend::OnMouseDown(int button, double x, double y)
{
    Camera.OnMouseDown(button, (float)x, (float)y);
}

void CaptureFrontend::OnMouseUp(int button)
{
    Camera.OnMouseUp(button);
}

void CaptureFrontend::OnMouseScroll(double x, double y)
{
    Camera.OnMouseScroll((float)x, (float)y);
}

void CaptureFrontend::OnMouseMove(double x, double y)
{
    Camera.OnMouseMove((float)x, (float)y);
}

void CaptureFrontend::OnKey(int key, bool press)
{
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        std::vector<protos::CameraExtrinsics> extrinsics = RuntimeConfig.GetExtrinsics();
        const int camera_index = key - GLFW_KEY_1;
        if (extrinsics.size() <= camera_index || extrinsics[camera_index].IsIdentity) {
            spdlog::warn("Cannot snap to camera because extrinsics have not been calibrated yet");
            return;
        }
        auto& transform = extrinsics[camera_index].Transform;
        Camera.SnapToPose(-transform[0 * 4 + 3], transform[1 * 4 + 3], -transform[2 * 4 + 3]);
    }
    else if (key == GLFW_KEY_F1) {
        if (press) {
            Camera.SnapToAngle(0.f, 0.f);
        }
    }
    else if (key == GLFW_KEY_F2) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT * 0.5f, 0.f);
        }
    }
    else if (key == GLFW_KEY_F3) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT, 0.f);
        }
    }
    else if (key == GLFW_KEY_F4) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT * 1.5f, 0.f);
        }
    }
    else if (key == GLFW_KEY_F5) {
        if (press) {
            Camera.SnapToAngle(0.f, M_PI_FLOAT * 0.25f);
        }
    }
    else if (key == GLFW_KEY_F6) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT * 0.5f, M_PI_FLOAT * 0.25f);
        }
    }
    else if (key == GLFW_KEY_F7) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT, M_PI_FLOAT * 0.25f);
        }
    }
    else if (key == GLFW_KEY_F8) {
        if (press) {
            Camera.SnapToAngle(M_PI_FLOAT * 1.5f, M_PI_FLOAT * 0.25f);
        }
    }
    else if (key == GLFW_KEY_F9) {
        if (press) {
            Camera.SnapToAngle(0.f, M_PI_FLOAT * 0.5f);
        }
    }
    else if (key == GLFW_KEY_M) {
        if (press) {
            ShowMeshCheckValue = !ShowMeshCheckValue;
            spdlog::debug("ShowMeshCheckValue = {}", ShowMeshCheckValue);
        }
    }
    else if (key == GLFW_KEY_SPACE) {
        if (press) {
            RenderPaused = !RenderPaused;
            spdlog::debug("RenderPaused = {}", RenderPaused);
        }
    }
    else if (key == GLFW_KEY_LEFT) {
        if (press) {
            //RotationVelocityY = -1;
        } else {
            //RotationVelocityY = 0;
        }
    }
    else if (key == GLFW_KEY_RIGHT) {
        if (press) {
            //RotationVelocityY = 1;
        } else {
            //RotationVelocityY = 0;
        }
    }
    else if (key == GLFW_KEY_UP) {
        if (press) {
            //RotationVelocityX = -1;
        } else {
            //RotationVelocityX = 0;
        }
    }
    else if (key == GLFW_KEY_DOWN) {
        if (press) {
            //RotationVelocityX = 1;
        } else {
            //RotationVelocityX = 0;
        }
    }
}

void CaptureFrontend::OnImageBatch(std::shared_ptr<ImageBatch>& batch)
{
    std::lock_guard<std::mutex> locker(BatchLock);
    Batch = batch;
}

void CaptureFrontend::Render()
{
    // Make changes to UI based on input
    SetupUI();

    // Clear background
    const nk_colorf bg = BackgroundColor;
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    if (ShowPreviewCheckValue) {
        RenderMeshes();
    }

    // Render GUI
    const int max_vertex_buffer = 512 * 1024;
    const int max_element_buffer = 128 * 1024;
    nk_glfw3_render(NK_ANTI_ALIASING_ON, max_vertex_buffer, max_element_buffer);

    // FIXME: Show capture status here

    glfwSwapBuffers(Window);
}

void CaptureFrontend::RenderMeshes()
{
    // Render tasks:

    std::shared_ptr<ImageBatch> batch;
    if (!RenderPaused) {
        PausedBatch.reset();
        std::lock_guard<std::mutex> locker(BatchLock);
        batch = Batch;
    }
    else if (!PausedBatch)
    {
        {
            std::lock_guard<std::mutex> locker(BatchLock);
            batch = Batch;
        }
        PausedBatch = batch;
    }
    else
    {
        batch = PausedBatch;
    }

    if (!batch || batch->Images.empty()) {
        return;
    }

    static const float kFloatEpsilon = 0.0000000001f;
    static const float kFloatPi = 3.141592654f;

    int width = 0, height = 0;
    glfwGetFramebufferSize(Window, &width, &height);

    if (ShowMeshCheckValue)
    {
        Matrix4 projection = Matrix4::perspective(
            kFloatPi * 80.f/180.f,
            width / static_cast<float>( height),
            0.2f,
            20.f);

        Point3 MeshCenter = Point3(0.f, 0.f, 0.f);

        const Matrix4 view = Camera.GetCameraViewTransform();

        std::vector<protos::CameraExtrinsics> extrinsics = RuntimeConfig.GetExtrinsics();

        const int image_count = static_cast<int>( batch->Images.size() );
        for (int i = 0; i < image_count && i < kMaxMeshes; ++i)
        {
            auto& image = batch->Images[i];
            bool success = MeshRender[i].UpdateMesh(
                image->MeshVertices.data(),
                static_cast<int>( image->MeshVertices.size() ),
                image->MeshTriangles.data(),
                static_cast<int>( image->MeshTriangles.size() ));
            if (!success) {
                spdlog::error("Failed to update mesh for camera {}", i);
                return;
            }

            success = MeshRender[i].UpdateNV12(
                image->Color[0],
                image->Color[1],
                image->ColorWidth,
                image->ColorHeight,
                image->ColorStride,
                image->ChromaWidth,
                image->ChromaHeight,
                image->ChromaStride);
            if (!success) {
                spdlog::error("Failed to update NV12 for camera {}", i);
                return;
            }

            float camera_pos[4] = { 0.f, 0.f, 0.f, 10.f };

            Matrix4 model = Matrix4::identity();

            if ((int)extrinsics.size() > i)
            {
                if (!extrinsics[i].IsIdentity) {
                    const auto& transform = extrinsics[i].Transform;
                    camera_pos[0] = -transform[0 * 4 + 3];
                    camera_pos[1] = -transform[1 * 4 + 3];
                    camera_pos[2] = -transform[2 * 4 + 3];
                    for (int row = 0; row < 4; ++row) {
                        for (int col = 0; col < 4; ++col) {
                            model.setElem(col, row, transform[row * 4 + col]);
                        }
                    }
                }
            }

            Matrix4 mvp = projection * view * model;

            success = MeshRender[i].Render(mvp, camera_pos);
            if (!success) {
                spdlog::error("Failed to render mesh for camera {}", i);
                return;
            }
        }
    }
    else
    {
        RgbdImage* first_image = nullptr;
        int image_count = 0;

        for (auto& image : batch->Images)
        {
            if (!first_image) {
                first_image = image.get();
            }

            if (first_image->ColorWidth != image->ColorWidth) {
                continue; // Skip frames that have different sizes
            }

            TileImageData data;
            data.Y = image->Color[0];
            data.U = image->Color[1];
            data.V = image->Color[2];
            ImageTileRender.SetImage(image_count, data);
            ++image_count;
        }

        ImageTileRender.Render(
            width,
            height,
            image_count,
            first_image->ColorWidth,
            first_image->ColorHeight,
            first_image->IsNV12);
    }
}


} // namespace core
