// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "GLCore.hpp"
#include <vectormath.hpp>

namespace core {


//------------------------------------------------------------------------------
// ImageTilingSolver

class ImageTilingSolver
{
public:
    void SolveFit(int width, int height, int image_count, float aspect_ratio);

    // Output
    float TileWidth = 0.f;
    float TileHeight = 0.f;
    bool RowFirst = false; // true: Iterate over rows, false: Iterate over cols.
    int TileSpan = 1; // Number of rows/columns before next column/row

protected:
    int LastWidth = 0, LastHeight = 0, LastCount = 0;
    float LastAspect = 0.f;
};


//------------------------------------------------------------------------------
// OpenGL YUV Multi-plane Simple Frame Renderer

class YUVPlaneSimpleFrameRenderer
{
public:
    bool Initialize();
    void Shutdown();

    void UpdatePosition(
        float left,
        float top,
        float width,
        float height);

    bool UpdateYUV(
        const uint8_t* y_ptr,
        const uint8_t* u_ptr,
        const uint8_t* v_ptr,
        int y_width,
        int y_height,
        int y_stride,
        int uv_width,
        int uv_height,
        int uv_stride);

    // Render the texture to the screen
    bool Render();

protected:
    int WidthPixels = 0, HeightPixels = 0;

    GLuint TexY = 0, TexU = 0, TexV = 0;
    GLuint UniformTexY = 0, UniformTexU = 0, UniformTexV = 0;
    GLuint UniformMVPMatrix = 0;
    Program MyProgram;

    GLuint VAO = 0;
    GLuint VBO_Coords = 0;
    GLuint EBO = 0;

    int TriangleIndexCount = 0;

    Matrix4 Transform{};

    float Left, Top, Width, Height;
};


//------------------------------------------------------------------------------
// OpenGL NV12 Two-Plane Simple Frame Renderer

class NV12PlaneSimpleFrameRenderer
{
public:
    bool Initialize();
    void Shutdown();

    void UpdatePosition(
        float left,
        float top,
        float width,
        float height);

    bool UpdateNV12(
        const uint8_t* y_ptr,
        const uint8_t* uv_ptr,
        int y_width,
        int y_height,
        int y_stride,
        int uv_width,
        int uv_height,
        int uv_stride); // 2*uv_width or more

    // Render the texture to the screen
    bool Render();

protected:
    int WidthPixels = 0, HeightPixels = 0;

    GLuint TexY = 0, TexUV = 0;
    GLuint UniformTexY = 0, UniformTexUV = 0;
    GLuint UniformMVPMatrix = 0;
    Program MyProgram;

    GLuint VAO = 0;
    GLuint VBO_Coords = 0;
    GLuint EBO = 0;

    int TriangleIndexCount = 0;

    Matrix4 Transform{};

    float Left, Top, Width, Height;
};


//------------------------------------------------------------------------------
// ImageTilingRenderer

static const int kMaxTiledImages = 4;

struct TileImageData {
    const uint8_t* Y = nullptr;
    const uint8_t* U = nullptr;
    const uint8_t* V = nullptr;
};

class ImageTilingRenderer
{
public:
    bool Initialize();
    void Shutdown();

    void SetImage(
        int index,
        TileImageData data);
    bool Render(
        int width,
        int height,
        int image_count,
        int image_width,
        int image_height,
        bool is_nv12);

protected:
    YUVPlaneSimpleFrameRenderer FrameRenderersYUV[kMaxTiledImages];
    NV12PlaneSimpleFrameRenderer FrameRenderersNV12[kMaxTiledImages];
    TileImageData Ptrs[kMaxTiledImages];

    ImageTilingSolver Tiling;
    int ColorWidth, ColorHeight;
};


} // namespace core
