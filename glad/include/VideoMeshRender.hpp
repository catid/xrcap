// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#pragma once

#include "GLCore.hpp"

#include <vectormath.hpp>

namespace core {


//------------------------------------------------------------------------------
// OpenGL YUV Multi-plane Video Frame Renderer

class YUVVideoMeshRender
{
public:
    bool Initialize();
    void Shutdown();

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

    bool UpdateMesh(
        const float* xyzuv_ptr,
        int floats_count, // Number of float's
        const uint32_t* indices_ptr,
        int indices_count); // Number of uint32_t's

    // Render the texture to the screen
    bool Render(Matrix4& mvp);

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
};


//------------------------------------------------------------------------------
// OpenGL NV12 Multi-plane Video Frame Renderer

class NV12VideoMeshRender
{
public:
    bool Initialize();
    void Shutdown();

    bool UpdateNV12(
        const uint8_t* y_ptr,
        const uint8_t* uv_ptr,
        int y_width,
        int y_height,
        int y_stride,
        int uv_width,
        int uv_height,
        int uv_stride);

    bool UpdateMesh(
        const float* xyzuv_ptr,
        int floats_count, // Number of float's
        const uint32_t* indices_ptr,
        int indices_count); // Number of uint32_t's

    // Render the texture to the screen
    bool Render(Matrix4& mvp, const float* camera_pos);

protected:
    int WidthPixels = 0, HeightPixels = 0;

    GLuint TexY = 0, TexUV = 0;
    GLuint UniformTexY = 0, UniformTexUV = 0;
    GLuint UniformMVPMatrix = 0;
    GLuint UniformCameraPos = 0;
    Program MyProgram;

    GLuint VAO = 0;
    GLuint VBO_Coords = 0;
    GLuint EBO = 0;

    int TriangleIndexCount = 0;
};


} // namespace core
