// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "ViewerWindow.hpp"

#include <core_logging.hpp>
#include <core_string.hpp>
#include <core_win32.hpp>

#include <nfd.h> // nativefiledialog library

#include "AppIcon.inc"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fstream>
#include <ios>

namespace core {


//------------------------------------------------------------------------------
// ViewerWindow

void ViewerWindow::Initialize(const std::string& file_path)
{
    Terminated = false;
    Thread = std::make_shared<std::thread>(&ViewerWindow::Loop, this);

    if (!LoadFromFile(GetSettingsFilePath("xrcap", CAPTURE_VIEWER_DEFAULT_SETTINGS), Settings)) {
        spdlog::warn("Failed to load settings from previous session");
    }

    if (!file_path.empty()) {
        if (xrcap_playback_read_file(file_path.c_str())) {
            IsLivePlayback = false;
            IsFileOpen = true;
        }
    }
}

void ViewerWindow::Shutdown()
{
    Terminated = true;
    JoinThread(Thread);
}

void ViewerWindow::Loop()
{
    SetCurrentThreadName("Viewer");

    CalibThread = std::make_shared<std::thread>(&ViewerWindow::CalibLoop, this);
    LightCalibThread = std::make_shared<std::thread>(&ViewerWindow::LightCalibLoop, this);

    ScopedFunction term_scope([&]() {
        Terminated = true;
        JoinThread(CalibThread);
        JoinThread(LightCalibThread);
        xrcap_shutdown();
    });

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
        1280,
        768,
        "Mesh Viewer",
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

    glfwSetCursorPosCallback(Window, [](GLFWwindow* window, double xpos, double ypos) {
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
        thiz->OnMouseMove(xpos, ypos);
    });
    glfwSetMouseButtonCallback(Window, [](GLFWwindow* window, int button, int action, int mods) {
        CORE_UNUSED(mods);
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
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
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
        thiz->OnMouseScroll(xpos, ypos);

        // Forward to Nuklear
        nk_gflw3_scroll_callback(window, xpos, ypos);
    });

    glfwSetKeyCallback(Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
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
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
        CORE_UNUSED(thiz);

        spdlog::trace("Window client area resized: {}x{}", width, height);
    });

    glfwSetFramebufferSizeCallback(Window, [](GLFWwindow* window, int width, int height)
    {
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
        CORE_UNUSED(thiz);

        spdlog::trace("Framebuffer resized: {}x{}", width, height);

        glViewport(0, 0, width, height);
    });

    glfwSetWindowIconifyCallback(Window, [](GLFWwindow* window, int iconified)
    {
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );
        thiz->IsIconified = (iconified != 0);

        spdlog::info("Iconified: {}", thiz->IsIconified);
    });

    glfwSetWindowRefreshCallback(Window, [](GLFWwindow* window)
    {
        ViewerWindow* thiz = reinterpret_cast<ViewerWindow*>( glfwGetWindowUserPointer(window) );

        thiz->Render();
    });

    // Wait for V-sync
    glfwSwapInterval(1);

    int width = 0, height = 0;
    glfwGetWindowSize(Window, &width, &height);
    glViewport(0, 0, width, height);

    StartRender();

    // While window is still open:
    while (!glfwWindowShouldClose(Window) && !Terminated)
    {
        if (PhotoboothStartMsec != 0 && GetTimeMsec() - PhotoboothStartMsec > 10000) {
            xrcap_record_pause(1);
            PhotoboothStartMsec = 0;
        }

        if (!IsIconified)
        {
            Render();
        } else {
            // Wait for window to be restored
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // This function blocks sometimes during resize, so we should watch
        // for the window refresh callback and render there too.
        glfwPollEvents();
    }

    StopRender();
}

void ViewerWindow::StartRender()
{
    ImageTileRender.Initialize();
    for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
    {
        if (!MeshRenderer[i].Initialize()) {
            spdlog::error("Mesh renderer failed to initialize");
        }
    }

    BackgroundColor.r = 0.10f;
    BackgroundColor.g = 0.18f;
    BackgroundColor.b = 0.24f;
    BackgroundColor.a = 1.0f;

    NuklearContext = nk_glfw3_init(Window, NK_GLFW3_DEFAULT/*NK_GLFW3_INSTALL_CALLBACKS*/);
    nk_context* ctx = NuklearContext;

    std::string font_path = GetFullFilePathFromRelative("FiraCode-Retina.ttf");

    struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&atlas);
    struct nk_font *firacode = nk_font_atlas_add_from_file(
        atlas,
        font_path.c_str(),
        20,
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
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(0, 0, 0, 255);
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
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(156, 193, 220, 255);
    nk_style_from_table(ctx, table);
}

void ViewerWindow::StopRender()
{
    ImageTileRender.Shutdown();
    for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
    {
        MeshRenderer[i].Shutdown();
    }

    nk_glfw3_shutdown();
}

void ViewerWindow::SetupUI()
{
    int width = 0, height = 0;
    glfwGetWindowSize(Window, &width, &height);

    nk_context* ctx = NuklearContext;
    const nk_colorf bg = BackgroundColor;

    nk_glfw3_new_frame();

    struct nk_rect bounds{};
    if (nk_begin_titled(ctx, "Login", "Login", nk_rect(10, 10, 330, 300),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 30, 2);

        char rendezvous_server_host[256] = { '\0' };
        SafeCopyCStr(rendezvous_server_host, sizeof(rendezvous_server_host), Settings.ServerHostname.c_str());
        int host_len = static_cast<int>( Settings.ServerHostname.size() );
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Hostname:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, rendezvous_server_host, &host_len, 255, nk_filter_ascii);
        rendezvous_server_host[host_len] = 0;
        Settings.ServerHostname = rendezvous_server_host;

        int rendezvous_server_port = Settings.ServerPort;
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Port:", NK_TEXT_RIGHT);
        nk_property_int(ctx, "#port", 0, &rendezvous_server_port, 65535, 1, 1.f);
        Settings.ServerPort = rendezvous_server_port;

        char name[256] = { '\0' };
        SafeCopyCStr(name, sizeof(name), Settings.ServerName.c_str());
        int name_len = static_cast<int>( Settings.ServerName.size() );
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Name:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, name, &name_len, 255, nk_filter_ascii);
        name[name_len] = 0;
        Settings.ServerName = name;

        char password[256] = { '\0' };
        SafeCopyCStr(password, sizeof(password), Settings.ServerPassword.c_str());
        int password_len = static_cast<int>( Settings.ServerPassword.size() );
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Password:", NK_TEXT_RIGHT);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, password, &password_len, 255, nk_filter_ascii);
        password[password_len] = 0;
        Settings.ServerPassword = password;

        nk_layout_row_dynamic(ctx, 60, 1);
        if (nk_button_label(ctx, "Connect")) {
            spdlog::info("Connection requested by UI");

            CloseFile();
            CloseRecordingFile();

            xrcap_connect(
                Settings.ServerHostname.c_str(),
                Settings.ServerPort,
                Settings.ServerName.c_str(),
                Settings.ServerPassword.c_str());

            if (!SaveToFile(Settings, GetSettingsFilePath("xrcap", CAPTURE_VIEWER_DEFAULT_SETTINGS))) {
                spdlog::warn("Failed to save settings");
            }
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
        nk_window_set_bounds(ctx, "Login", bounds);
    }

    if (nk_begin_titled(ctx, "Compression", "Compression", nk_rect(10, 320, 330, 410),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_property_int(ctx, "#bitrate", 1000000, &ColorBitrate, 20000000, 1000000, 1000000.f);

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_property_int(ctx, "#quality", 1, &ColorQuality, 51, 1, 1.f);

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Color Video: ", NK_TEXT_RIGHT);
        ColorVideo = 1 + nk_combo_separator(
            ctx,
            "H264|H265",
            '|',
            ColorVideo - 1,
            static_cast<int>( XrcapVideo_Count - 1 ),
            30,
            nk_vec2(400.f, 400.f));

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Depth Video: ", NK_TEXT_RIGHT);
        DepthVideo = nk_combo_separator(
            ctx,
            "Lossless|H264|H265",
            '|',
            DepthVideo,
            static_cast<int>( XrcapVideo_Count ),
            30,
            nk_vec2(400.f, 400.f));

        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Denoise Percent:", NK_TEXT_RIGHT);
        nk_property_int(ctx, "#%", 0, &DenoisePercent, 100, 10, 10.f);

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_checkbox_label(ctx, "Cull Images", &CullImages);

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_checkbox_label(ctx, "Face Painting Fix", &FacePaintingFix);

        nk_layout_row_dynamic(ctx, 40, 1);
        if (nk_button_label(ctx, "Apply Video Settings")) {
            XrcapCompression compression;
            compression.ColorBitrate = ColorBitrate;
            compression.ColorQuality = static_cast<uint8_t>(ColorQuality);
            compression.ColorVideo = static_cast<uint8_t>(ColorVideo);
            compression.DepthVideo = static_cast<uint8_t>(DepthVideo);
            compression.DenoisePercent = static_cast<uint8_t>(DenoisePercent);
            compression.FacePaintingFix = FacePaintingFix != 0 ? 1 : 0;
            xrcap_set_compression(&compression);
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
        nk_window_set_bounds(ctx, "Compression", bounds);
    }

    if (nk_begin_titled(ctx, "State", "System Status", nk_rect(350, 10, 460, 500),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "State: ", NK_TEXT_RIGHT);
        nk_label(ctx, xrcap_stream_state_str(LastStatus.State), NK_TEXT_RIGHT);

        nk_label(ctx, "Requested mode: ", NK_TEXT_RIGHT);
        const int old_mode = static_cast<int>( LastStatus.Mode );
        const int new_mode = nk_combo_separator(
            ctx,
            "Disable|Calibration|CaptureLowQ|CaptureHighQ",
            '|',
            old_mode,
            static_cast<int>( XrcapStreamMode_Count ),
            40,
            nk_vec2(400.f, 400.f));
        if (old_mode != new_mode) {
            xrcap_set_server_capture_mode(static_cast<XrcapStreamMode>( new_mode ));

            // Reset lighting on capture mode changes
            ResetLighting();
        }

        nk_label(ctx, "Mode: ", NK_TEXT_RIGHT);
        nk_label(ctx, xrcap_stream_mode_str(LastStatus.Mode), NK_TEXT_RIGHT);

        nk_label(ctx, "Health: ", NK_TEXT_RIGHT);
        nk_label(ctx, xrcap_capture_status_str(LastStatus.CaptureStatus), NK_TEXT_RIGHT);

        for (int i = 0; i < LastStatus.CameraCount; ++i) {
            std::string camera_label = "Camera ";
            camera_label += std::to_string(i);
            camera_label += ": ";
            nk_label(ctx, camera_label.c_str(), NK_TEXT_RIGHT);
            nk_label(ctx, xrcap_camera_code_str(LastStatus.CameraCodes[i]), NK_TEXT_RIGHT);
        }

        nk_label(ctx, "Mbps: ", NK_TEXT_RIGHT);
        std::string speed_str = std::to_string(LastStatus.BitsPerSecond / 1000000.f);
        nk_label(ctx, speed_str.c_str(), NK_TEXT_RIGHT);

        nk_label(ctx, "Ploss%: ", NK_TEXT_RIGHT);
        std::string ploss_str = std::to_string(LastStatus.PacketlossRate * 100.f);
        nk_label(ctx, ploss_str.c_str(), NK_TEXT_RIGHT);

        nk_label(ctx, "OWD_ms: ", NK_TEXT_RIGHT);
        std::string owd_str = std::to_string(LastStatus.TripUsec / 1000.f);
        nk_label(ctx, owd_str.c_str(), NK_TEXT_RIGHT);

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
        nk_window_set_bounds(ctx, "State", bounds);
    }

    if (nk_begin_titled(ctx, "Configuration", "Configuration", nk_rect(1040, 10, 460, 510),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label(ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 50, 1);
        if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx),1200))) {
            nk_layout_row_dynamic(ctx, 80, 1);
            const nk_colorf nc = nk_color_picker(ctx, bg, NK_RGBA);
            nk_layout_row_dynamic(ctx, 50, 1);
            BackgroundColor.r = nk_propertyf(ctx, "#R:", 0, nc.r, 1.0f, 0.01f,0.005f);
            BackgroundColor.g = nk_propertyf(ctx, "#G:", 0, nc.g, 1.0f, 0.01f,0.005f);
            BackgroundColor.b = nk_propertyf(ctx, "#B:", 0, nc.b, 1.0f, 0.01f,0.005f);
            BackgroundColor.a = nk_propertyf(ctx, "#A:", 0, nc.a, 1.0f, 0.01f,0.005f);
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_checkbox_label(ctx, "Show mesh", &ShowMeshCheckValue);

        int queue_depth = PlaybackQueueDepth;
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_property_int(ctx, "#PlayQueueMsec", 100, &queue_depth, 1000, 100, 100.f);
        if (PlaybackQueueDepth != queue_depth) {
            xrcap_playback_settings(PlaybackQueueDepth);
        }
        PlaybackQueueDepth = queue_depth;

        nk_layout_row_dynamic(ctx, 40, 1);
        if (nk_button_label(ctx, "Reset View")) {
            Camera.Reset();
        }

        bool clip_update = false;

        int clip_enabled = ClipEnabled;
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_checkbox_label(ctx, "Clip Enabled", &clip_enabled);
        if (ClipEnabled != clip_enabled) {
            clip_update = true;
        }
        ClipEnabled = clip_enabled;

        float clip_radius = ClipRadiusMeters;
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Clip Radius:", NK_TEXT_RIGHT);
        nk_property_float(ctx, "#meters", 0.1f, &clip_radius, 10.f, 0.2f, 0.2f);
        if (ClipRadiusMeters != clip_radius) {
            clip_update = true;
        }
        ClipRadiusMeters = clip_radius;

        float clip_floor_meters = ClipFloorMeters;
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Clip Floor:", NK_TEXT_RIGHT);
        nk_property_float(ctx, "#meters", -10.f, &clip_floor_meters, 10.f, 0.2f, 0.2f);
        if (ClipFloorMeters != clip_floor_meters) {
            clip_update = true;
        }
        ClipFloorMeters = clip_floor_meters;

        float clip_ceiling_meters = ClipCeilingMeters;
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_label(ctx, "Clip Ceiling:", NK_TEXT_RIGHT);
        nk_property_float(ctx, "#meters", -10.f, &clip_ceiling_meters, 10.f, 0.2f, 0.2f);
        if (ClipCeilingMeters != clip_ceiling_meters) {
            clip_update = true;
        }
        ClipCeilingMeters = clip_ceiling_meters;

        if (clip_update) {
            xrcap_set_clip(
                ClipEnabled ? 1 : 0,
                ClipRadiusMeters,
                ClipFloorMeters,
                ClipCeilingMeters);
        }

#if 0
        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            spdlog::info("Camera {}: exposure={} awb={} iso={}", i, perspective.ExposureUsec, perspective.AutoWhiteBalanceUsec, perspective.ISOSpeed);
        }
#endif

        nk_layout_row_dynamic(ctx, 40, 1);
        if (nk_button_label(ctx, "Reset Lighting")) {
            ResetLighting();
        }

        if (LightingLocked)
        {
            nk_layout_row_dynamic(ctx, 40, 1);
            if (nk_button_label(ctx, "Unlock Lighting")) {
                ResetLighting();
            }

            bool invalid_extrinsics= false;
            for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
            {
                auto& perspective = LastFrame.Perspectives[i];
                if (!perspective.Valid) {
                    continue;
                }
                if (!perspective.Extrinsics || perspective.Extrinsics->IsIdentity) {
                    invalid_extrinsics = true;
                    break;
                }
            }

            if (invalid_extrinsics)
            {
                nk_layout_row_dynamic(ctx, 40, 1);
                nk_label(ctx, "Waiting for April Tag extrinsics calibration...", NK_TEXT_LEFT);
            }
            else
            {
                nk_layout_row_dynamic(ctx, 40, 1);
                if (nk_button_label(ctx, "Calibrate Lighting")) {
                    LightingCalibrationRequested = true;
                }
            }
        }
        else
        {
            nk_layout_row_dynamic(ctx, 40, 1);
            if (nk_button_label(ctx, "Lock Lighting"))
            {
                LightingLocked = true;

                std::vector<uint32_t> exposures;
                std::vector<uint32_t> awbs;
                std::vector<uint32_t> isospeeds;

                for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
                {
                    auto& perspective = LastFrame.Perspectives[i];
                    if (!perspective.Valid) {
                        continue;
                    }

                    exposures.push_back(perspective.ExposureUsec);
                    awbs.push_back(perspective.AutoWhiteBalanceUsec);
                    isospeeds.push_back(perspective.ISOSpeed);
                }

                const size_t count = exposures.size();
                if (count > 0)
                {
                    // Median exposure time
                    std::sort(exposures.begin(), exposures.end());
                    const uint32_t exposure_usec = exposures[count / 2];

                    const uint32_t awb_usec = NormalizeAWB(awbs);

                    // Median ISO speed.
                    // Note the cameras have no ISO speed control, this is just informative
                    std::sort(isospeeds.begin(), isospeeds.end());
                    const uint32_t iso_speed = isospeeds[count / 2];

                    spdlog::info("Setting manual exposure={} awb={} iso={}", exposure_usec, awb_usec, iso_speed);
                    xrcap_set_exposure(0, exposure_usec, awb_usec);
                }
            }
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
        nk_window_set_bounds(ctx, "Configuration", bounds);
    }

    XrcapRecording record_state;
    xrcap_record_state(&record_state);

    std::string recording_title;
    if (record_state.RecordingFileOpen) {
        const uint64_t usec = record_state.VideoDurationUsec;
        recording_title = fmt::format("Recording t={} S", usec / 1000000.f);
    } else {
        recording_title = "Recording (no file)";
    }

    if (nk_begin_titled(ctx, "Recording", recording_title.c_str(), nk_rect(1040, 530, 320, 130),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 30, record_state.RecordingFileOpen ? 3 : 2);
        if (nk_button_label(ctx, "Open")) {
            OpenRecordingFile();
        }
        if (record_state.RecordingFileOpen) {
            if (nk_button_label(ctx, "Close")) {
                CloseRecordingFile();
            }
            if (record_state.Paused) {
                if (nk_button_label(ctx, "Record")) {
                    StartRecording();
                }
            }
            else {
                if (nk_button_label(ctx, "Pause Rec.")) {
                    PauseRecording();
                }
            }
        } else {
            nk_label(ctx, "Not Recording", NK_TEXT_LEFT);
        }

        nk_layout_row_dynamic(ctx, 30, 1);
        nk_checkbox_label(ctx, "Photobooth: Right-click trigger", &PhotoboothEnabled);

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
        nk_window_set_bounds(ctx, "Recording", bounds);
    }

    if (LastStatus.Mode == XrcapStreamMode_Calibration)
    {
        if (nk_begin_titled(ctx, "Calibration", "Multi-Camera Calibration", nk_rect(370, 530, 460, 200),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            const CalibrationState state = CalibState;
            switch (state)
            {
            default:
            case CalibrationState::Idle:
                nk_layout_row_dynamic(ctx, 40, 1);
                nk_label(ctx, "Idle.  Press 'm' to view mesh and check calibration.", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 40, 1);
                if (nk_button_label(ctx, "April Tag: Full Calibration")) {
                    FullCalibrationRequested = true;
                    ExtrinsicsCalibrationRequested = true;
                }
                nk_layout_row_dynamic(ctx, 40, 1);
                if (nk_button_label(ctx, "ICP: Improve Calibration")) {
                    FullCalibrationRequested = false;
                    ExtrinsicsCalibrationRequested = true;
                }
                break;
            case CalibrationState::FindingMarker:
                nk_layout_row_dynamic(ctx, 40, 1);
                nk_label(ctx, "Waiting for all cameras to find the marker...", NK_TEXT_LEFT);
                break;
            case CalibrationState::Processing:
                nk_layout_row_dynamic(ctx, 40, 1);
                nk_label(ctx, "Processing...", NK_TEXT_LEFT);
                break;
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
            nk_window_set_bounds(ctx, "Calibration", bounds);
        }
    }

    if (IsLivePlayback)
    {
        if (nk_begin_titled(ctx, "Live", "Live", nk_rect(width * 0.5f, static_cast<float>( height - 190 ), 380, 90),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Open")) {
                OpenFile();
            }
            if (RenderPaused) {
                nk_label(ctx, "PAUSED (Space)", NK_TEXT_LEFT);
            }
            else {
                nk_label(ctx, "LIVE", NK_TEXT_LEFT);
            }
        }
        nk_end(ctx);
    }
    else
    {
        XrcapPlayback playback;
        xrcap_get_playback_state(&playback);

        std::string title;
        if (IsFileOpen) {
            const uint64_t usec = playback.VideoTimeUsec;
            title = fmt::format("Playback t={} S", usec / 1000000.f);
        } else {
            title = "Playback (no file)";
        }

        if (nk_begin_titled(ctx, "Playback", title.c_str(), nk_rect(width * 0.5f, static_cast<float>( height - 190 ), 440, 90),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(ctx, 30, IsFileOpen ? 4 : 2);
            if (nk_button_label(ctx, "Open")) {
                OpenFile();
            }
            if (IsFileOpen) {
                if (nk_button_label(ctx, "Close")) {
                    CloseFile();
                }
                nk_checkbox_label(ctx, "Loop", &FileLoopEnabled);
                if (RenderPaused) {
                    nk_label(ctx, "PAUSED (Space)", NK_TEXT_LEFT);
                }
                else {
                    nk_label(ctx, "PLAY", NK_TEXT_LEFT);
                }
                xrcap_playback_tricks(RenderPaused ? 1 : 0, FileLoopEnabled ? 1 : 0);
            } else {
                nk_label(ctx, "No File Loaded", NK_TEXT_LEFT);
            }
        }
        nk_end(ctx);
    }
}

void ViewerWindow::OpenFile()
{
    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_OpenDialog("xrcap", "", &path);
    ScopedFunction path_scope([&]() {
        NFD_FreeOutPath(&path);
    });
    if (result == NFD_OKAY && path) {
        spdlog::info("OpenFile: User selected path: `{}`", path);

        xrcap_reset();
        if (xrcap_playback_read_file(path)) {
            IsLivePlayback = false;
            IsFileOpen = true;
        }
    } else {
        spdlog::warn("OpenFile: User cancelled file selection");
    }
}

void ViewerWindow::CloseFile()
{
    spdlog::info("Closing file");
    IsFileOpen = false;

    xrcap_reset();
}

void ViewerWindow::OpenRecordingFile()
{
    CloseRecordingFile();

    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_SaveDialog("xrcap", "Recording.xrcap", &path);
    ScopedFunction path_scope([&]() {
        NFD_FreeOutPath(&path);
    });
    if (result == NFD_OKAY && path) {
        spdlog::info("OpenRecordingFile: User selected path: `{}`", path);

        if (!xrcap_record(path)) {
            spdlog::error("Failed to open recording file");
        } else {
            spdlog::info("Successfully opened recording file");
        }
    } else {
        spdlog::warn("OpenRecordingFile: User cancelled file selection");
    }
}

void ViewerWindow::CloseRecordingFile()
{
    spdlog::info("Closed recording file");
    xrcap_record(nullptr);
}

void ViewerWindow::StartRecording()
{
    spdlog::info("Start recording");
    xrcap_record_pause(0);
}

void ViewerWindow::PauseRecording()
{
    spdlog::info("Pause recording");
    xrcap_record_pause(1);
}

void ViewerWindow::ResetLighting()
{
    for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
    {
        auto& perspective = LastFrame.Perspectives[i];
        if (!perspective.Valid) {
            continue;
        }
        xrcap_set_lighting(perspective.Guid, perspective.CameraIndex, 0.f, 1.0f);
    }

    // Also unlock lighting
    LightingLocked = false;
    xrcap_set_exposure(1, 0, 0);
    LightingCalibrationRequested = false;
}

struct LoadedFrame
{
    std::vector<float> floats;
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> uv_plane;
    VerticesInfo Info;
};

void ViewerWindow::LoadMeshAndTest()
{
    const std::string filename = "raw_mesh.bin";
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open {}", filename);
        return;
    }

    std::vector<std::shared_ptr<LoadedFrame>> frames;

    while (!!file)
    {
        std::shared_ptr<LoadedFrame> frame = std::make_shared<LoadedFrame>();
        auto& info = frame->Info;

        uint32_t magic = 0;
        uint32_t stride = 0;

        file.read((char*)&magic, sizeof(magic));
        if (!file || magic != 0x00112233) {
            break;
        }

        file.read((char*)&info.Width, sizeof(info.Width));
        file.read((char*)&info.Height, sizeof(info.Height));
        file.read((char*)&info.ChromaWidth, sizeof(info.ChromaWidth));
        file.read((char*)&info.ChromaHeight, sizeof(info.ChromaHeight));
        file.read((char*)&info.FloatsCount, sizeof(info.FloatsCount));
        file.read((char*)&stride, sizeof(stride));
        file.read((char*)&info.Accelerometer[0], sizeof(info.Accelerometer));

        if (!file || info.FloatsCount <= 0 || info.Width <= 0 || info.Height <= 0 || info.ChromaWidth <= 0 || info.ChromaHeight <= 0 || stride <= 0) {
            break;
        }

        frame->floats.resize(info.FloatsCount);
        file.read((char*)frame->floats.data(), info.FloatsCount * sizeof(float));

        const unsigned y_bytes = info.Width * info.Height;
        const unsigned uv_bytes = info.ChromaWidth * info.ChromaHeight * 2;

        frame->y_plane.resize(y_bytes);
        frame->uv_plane.resize(uv_bytes);
        file.read((char*)frame->y_plane.data(), y_bytes);
        file.read((char*)frame->uv_plane.data(), uv_bytes);

        info.XyzuvVertices = frame->floats.data();
        info.Y = frame->y_plane.data();
        info.UV = frame->uv_plane.data();
        info.Calibration = frame->Info.Calibration;
        frames.push_back(frame);
    }

    std::vector<VerticesInfo> vertices;

    for (auto& frame : frames) {
        vertices.push_back(frame->Info);
    }

    std::vector<AlignmentTransform> extrinsics;
    if (!CalculateExtrinsics(vertices, extrinsics)) {
        spdlog::error("Depth registration failed");
    } else {
        spdlog::info("Depth registration succeeded!");
    }
}

void ViewerWindow::OnMouseDown(int button, double x, double y)
{
    Camera.OnMouseDown(button, (float)x, (float)y);
}

void ViewerWindow::OnMouseUp(int button)
{
    Camera.OnMouseUp(button);

    if (PhotoboothEnabled && button == 1 && PhotoboothStartMsec == 0) {
        xrcap_record_pause(0);
        PhotoboothStartMsec = GetTimeMsec();
    }
}

void ViewerWindow::OnMouseScroll(double x, double y)
{
    Camera.OnMouseScroll((float)x, (float)y);
}

void ViewerWindow::OnMouseMove(double x, double y)
{
    Camera.OnMouseMove((float)x, (float)y);
}

void ViewerWindow::OnKey(int key, bool press)
{
    // Ignore key presses if the UI has focus
    if (nk_window_is_any_hovered(NuklearContext)) {
        return;
    }

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        const int target_index = key - GLFW_KEY_1;

        int camera_index = 0;
        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            if (camera_index == target_index) {
                if (!perspective.Extrinsics->IsIdentity) {
                    auto& transform = perspective.Extrinsics->Transform;
                    Camera.SnapToPose(-transform[0 * 4 + 3], transform[1 * 4 + 3], -transform[2 * 4 + 3]);
                }
                break;
            }

            ++camera_index;
        }
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
    else if (key == GLFW_KEY_SPACE)
    {
        if (press) {
            RenderPaused = !RenderPaused;
            spdlog::debug("RenderPaused = {}", RenderPaused);
        }
    }
    else if (key == GLFW_KEY_M)
    {
        if (press) {
            ShowMeshCheckValue = !ShowMeshCheckValue;
            spdlog::debug("ShowMeshCheckValue = {}", ShowMeshCheckValue);
        }
    }
    else if (key == GLFW_KEY_P)
    {
        if (press) {
            //EnableRawStorage = true;
            //spdlog::debug("EnableRawStorage = {}", EnableRawStorage);
        }
    }
    else if (key == GLFW_KEY_T)
    {
        if (press) {
            //LoadMeshAndTest();
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

void ViewerWindow::Render()
{
    if (!RenderPaused) {
        std::lock_guard<std::mutex> locker(FrameLock);
        if (!FrameInUse) {
            xrcap_get(&LastFrame, &LastStatus);
        }
    }

    // Make changes to UI based on input
    SetupUI();

    // Clear background
    const nk_colorf bg = BackgroundColor;
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    RenderMeshes();

    // Render GUI
    const int max_vertex_buffer = 512 * 1024;
    const int max_element_buffer = 128 * 1024;
    nk_glfw3_render(NK_ANTI_ALIASING_ON, max_vertex_buffer, max_element_buffer);

    glfwSwapBuffers(Window);

    if (!ExtrinsicsCalibrationRequested && LightingCalibrationRequested)
    {
        LightingCalibrationRequested = false;

        const uint64_t t0 = GetTimeUsec();

        std::vector<LightCloudInputs> inputs;

        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            LightCloudInputs input;
            VerticesInfo& info = input.Info;
            info.XyzuvVertices = perspective.XyzuvVertices;
            for (int j = 0; j < 3; ++j) {
                info.Accelerometer[j] = perspective.Accelerometer[j];
            }
            info.FloatsCount = perspective.FloatsCount;
            info.Height = perspective.Height;
            info.Width = perspective.Width;
            info.ChromaWidth = perspective.ChromaWidth;
            info.ChromaHeight = perspective.ChromaHeight;
            info.Y = perspective.Y;
            info.UV = perspective.UV;
            info.Calibration = (CameraCalibration*)perspective.Calibration;

            input.Metadata.CameraIndex = perspective.CameraIndex;
            input.Metadata.Guid = perspective.Guid;
            input.Metadata.Brightness = perspective.Brightness;
            input.Metadata.Saturation = perspective.Saturation;

            if (perspective.Extrinsics->IsIdentity) {
                input.Extrinsics.Identity = true;
            } else {
                input.Extrinsics.Identity = false;
                memcpy(input.Extrinsics.Transform, perspective.Extrinsics->Transform, sizeof(input.Extrinsics.Transform));
            }

            inputs.push_back(input);
        }

        std::vector<std::shared_ptr<KdtreePointCloud>> clouds;
        ForegroundCreateClouds(inputs, clouds);

        // Send it to the background thread
        {
            std::lock_guard<std::mutex> locker(LightLock);
            LightClouds = clouds;
        }

        const uint64_t t1 = GetTimeUsec();
        spdlog::info("Foreground lighting work = {} ms", (t1 - t0) / 1000.f);
    }

    if (EnableRawStorage)
    {
        EnableRawStorage = false;

        const std::string filename = "raw_mesh.bin";
        std::ofstream file(filename.c_str(), std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open {}", filename);
        }
        else {
            for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
            {
                auto& perspective = LastFrame.Perspectives[i];
                if (!perspective.Valid) {
                    continue;
                }

                uint32_t magic = 0x00112233;
                uint32_t stride = XRCAP_FLOAT_STRIDE;
                uint32_t count = perspective.FloatsCount;
                uint32_t width = perspective.Width;
                uint32_t height = perspective.Height;
                uint32_t cwidth = perspective.ChromaWidth;
                uint32_t cheight = perspective.ChromaHeight;

                file.write((char*)&magic, sizeof(magic));
                file.write((char*)&width, sizeof(width));
                file.write((char*)&height, sizeof(height));
                file.write((char*)&cwidth, sizeof(cwidth));
                file.write((char*)&cheight, sizeof(cheight));
                file.write((char*)&count, sizeof(count));
                file.write((char*)&stride, sizeof(stride));
                file.write((char*)perspective.Accelerometer, sizeof(perspective.Accelerometer));
                file.write((char*)perspective.XyzuvVertices, count * sizeof(float));
                file.write((char*)perspective.Y, width * height);
                file.write((char*)perspective.UV, cwidth * cheight * 2);

                spdlog::debug("Stored perspective {} to {}", i, filename);
            }
        }
    }
}

void ViewerWindow::RenderMeshes()
{
    // Render tasks:

    if (!LastFrame.Valid) {
        return;
    }

    static const float kFloatEpsilon = 0.0000000001f;
    static const float kFloatPi = 3.141592654f;

    int width = 0, height = 0;
    glfwGetWindowSize(Window, &width, &height);

    if (ShowMeshCheckValue)
    {
        Matrix4 projection = Matrix4::perspective(
            kFloatPi * 80.f/180.f,
            width / static_cast<float>( height),
            0.2f,
            20.f);

        const Matrix4 view = Camera.GetCameraViewTransform();

        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            Matrix4 model = Matrix4::identity();
            XrcapExtrinsics* extrinsics = perspective.Extrinsics;
            if (!extrinsics->IsIdentity) {
                for (int row = 0; row < 4; ++row) {
                    for (int col = 0; col < 4; ++col) {
                        const float f = extrinsics->Transform[row * 4 + col];
                        model.setElem(col, row, f);
                    }
                }
            }

            Matrix4 mvp = projection * view * model;

            bool success = MeshRenderer[i].UpdateMesh(
                perspective.XyzuvVertices,
                perspective.FloatsCount,
                perspective.Indices,
                perspective.IndicesCount);
            if (!success) {
                spdlog::error("Failed to update mesh for camera {}", i);
                return;
            }

            success = MeshRenderer[i].UpdateNV12(
                perspective.Y,
                perspective.UV,
                perspective.Width,
                perspective.Height,
                perspective.Width,
                perspective.ChromaWidth,
                perspective.ChromaHeight,
                perspective.ChromaWidth*2);
            if (!success) {
                spdlog::error("Failed to update NV12 for camera {}", i);
                return;
            }

            float camera_pos[4] = { 0.f, 0.f, 0.f, 10.f };
            if (!perspective.Extrinsics->IsIdentity)
            {
                camera_pos[0] = -perspective.Extrinsics->Transform[0 * 4 + 3];
                camera_pos[1] = -perspective.Extrinsics->Transform[1 * 4 + 3];
                camera_pos[2] = -perspective.Extrinsics->Transform[2 * 4 + 3];
            }

            success = MeshRenderer[i].Render(mvp, camera_pos);
            if (!success) {
                spdlog::error("Failed to render mesh for camera {}", i);
                return;
            }
        }
    }
    else
    {
        int image_count = 0;

        XrcapPerspective* first_image = nullptr;
        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            if (!first_image) {
                first_image = &perspective;
            }

            if (first_image->Width != perspective.Width) {
                continue; // Skip frames that have different sizes
            }

            TileImageData data;
            data.Y = perspective.Y;
            data.U = perspective.UV;
            data.V = nullptr;
            ImageTileRender.SetImage(image_count, data);
            ++image_count;
        }

        if (image_count > 0) {
            ImageTileRender.Render(
                width,
                height,
                image_count,
                first_image->Width,
                first_image->Height,
                true); // is NV12
        }
    }
}

void ViewerWindow::CalibLoop()
{
    while (!Terminated)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (!ExtrinsicsCalibrationRequested) {
            continue;
        }

        CalibState = CalibrationState::Processing;

        std::vector<VerticesInfo> vertices;
        std::vector<PerspectiveMetadata> metadata;

        // Mark frame in use
        {
            std::lock_guard<std::mutex> locker(FrameLock);
            FrameInUse = true;
        }
        ScopedFunction use_scope([this]() {
            std::lock_guard<std::mutex> locker(FrameLock);
            FrameInUse = false;
        });

        std::vector<AlignmentTransform> extrinsics;
        unsigned existing_extrinsics_count = 0;

        for (int i = 0; i < XRCAP_PERSPECTIVE_COUNT; ++i)
        {
            auto& perspective = LastFrame.Perspectives[i];
            if (!perspective.Valid) {
                continue;
            }

            VerticesInfo info;
            info.XyzuvVertices = perspective.XyzuvVertices;
            for (int j = 0; j < 3; ++j) {
                info.Accelerometer[j] = perspective.Accelerometer[j];
            }
            info.FloatsCount = perspective.FloatsCount;
            info.Height = perspective.Height;
            info.Width = perspective.Width;
            info.ChromaWidth = perspective.ChromaWidth;
            info.ChromaHeight = perspective.ChromaHeight;
            info.Y = perspective.Y;
            info.UV = perspective.UV;
            info.Calibration = (CameraCalibration*)perspective.Calibration;
            vertices.push_back(info);

            PerspectiveMetadata data;
            data.Guid = perspective.Guid;
            data.CameraIndex = perspective.CameraIndex;
            metadata.push_back(data);

            AlignmentTransform transform;
            if (perspective.Extrinsics && !perspective.Extrinsics->IsIdentity) {
                transform.Identity = false;
                memcpy(transform.Transform, perspective.Extrinsics->Transform, sizeof(transform.Transform));
                existing_extrinsics_count++;
            } else {
                transform.Identity = true;
            }
            extrinsics.push_back(transform);
        }

        if (!FullCalibrationRequested && existing_extrinsics_count == vertices.size()) {
            if (!RefineExtrinsics(vertices, extrinsics)) {
                spdlog::error("ICP registration failed");
                ExtrinsicsCalibrationRequested = false;
                CalibState = CalibrationState::Idle;
                continue;
            }
        } else {
            if (!CalculateExtrinsics(vertices, extrinsics)) {
                spdlog::error("Full registration failed");
                CalibState = CalibrationState::FindingMarker;
                continue;
            }
        }

        spdlog::info("Registration succeeded!");

        for (int i = 0; i < vertices.size(); ++i)
        {
            XrcapExtrinsics converted_extrinsics{};
            converted_extrinsics.IsIdentity = extrinsics[i].Identity;
            memcpy(converted_extrinsics.Transform, extrinsics[i].Transform, sizeof(converted_extrinsics.Transform));

            xrcap_set_extrinsics(metadata[i].Guid, metadata[i].CameraIndex, &converted_extrinsics);
        }

        ExtrinsicsCalibrationRequested = false;
        CalibState = CalibrationState::Idle;
    }
}

void ViewerWindow::LightCalibLoop()
{
    while (!Terminated)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::vector<std::shared_ptr<KdtreePointCloud>> clouds;

        {
            std::lock_guard<std::mutex> locker(LightLock);
            clouds = LightClouds;
            LightClouds.clear();
        }

        if (clouds.size() < 2) {
            continue;
        }

        std::vector<float> brightness, saturation;
        ExtractCloudLighting(clouds);
        if (ColorNormalization(clouds, brightness, saturation)) {
            for (size_t i = 0; i < clouds.size() && i < brightness.size(); ++i) {
                auto& metadata = clouds[i]->Input.Metadata;

                xrcap_set_lighting(metadata.Guid, metadata.CameraIndex, brightness[i], saturation[i]);
            }
        }
    }
}


} // namespace core
