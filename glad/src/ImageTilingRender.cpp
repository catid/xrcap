// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "ImageTilingRender.hpp"

#include "core_logging.hpp"

namespace core {


//------------------------------------------------------------------------------
// ImageTilingSolver

void ImageTilingSolver::SolveFit(int width, int height, int image_count, float aspect_ratio)
{
    if (LastWidth != width || LastHeight != height || LastCount != image_count ||
        std::fabs(LastAspect - aspect_ratio) > 0.00001f)
    {
        LastAspect = aspect_ratio;
        LastWidth = width;
        LastHeight = height;
        LastCount = image_count;
    } else {
        return;
    }

    const float max_width = width / (float)height;
    const float max_height = height / (float)width;

    float best = 0.f;

    // Fit to top edge:
    for (int columns = 1; columns <= image_count; ++columns)
    {
        const float tile_width = 1.f / columns;
        const float tile_height = 1.f / (aspect_ratio * columns);
        const int rows = static_cast<int>( (image_count + columns - 1) * tile_width );
        const float images_height = tile_height * rows;
        float score = images_height;
        if (images_height > max_height) {
            continue;
        }
        score *= max_width;
        if (best < score) {
            TileWidth = tile_width;
            TileHeight = tile_height * max_width; // scale by window aspect
            TileSpan = columns;
            best = score;
            RowFirst = true;
        }
    }

    // Fit to left edge:
    for (int rows = 1; rows <= image_count; ++rows)
    {
        const float tile_height = 1.f / rows;
        const float tile_width = aspect_ratio * tile_height;
        const int columns = static_cast<int>( (image_count + rows - 1) * tile_height );
        const float images_width = tile_width * columns;
        //const int incomplete_columns = rows * columns - image_count;
        float score = images_width;
        if (images_width > max_width) {
            continue;
        }
        score *= max_height;
        if (best < score) {
            TileWidth = tile_width * max_height; // scale by window aspect
            TileHeight = tile_height;
            TileSpan = rows;
            best = score;
            RowFirst = false;
        }
    }
}


//------------------------------------------------------------------------------
// OpenGL YUV Multi-plane Simple Frame Renderer

static const char* m_YUVSimpleVertexShader = R"(
    #version 330 core
    uniform mat4 MVPMatrix;
    layout (location = 0) in vec4 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    out vec4 TexPos;
    void main()
    {
        gl_Position = MVPMatrix * aPos;
        TexPos = gl_Position;
        TexCoord = aTexCoord;
    }
)";

static const char* m_YUVSimpleFragmentShader = R"(
    #version 330 core
    #ifdef GL_ES
        precision highp float;
    #endif
    uniform sampler2D TexY;
    uniform sampler2D TexU;
    uniform sampler2D TexV;
    in vec2 TexCoord;
    in vec4 TexPos;
    out vec4 FragColor;
    void main()
    {
        // Sample YUV color
        float r, g, b, y, u, v;
        y = texture(TexY, TexCoord).r;
        u = texture(TexU, TexCoord).r;
        v = texture(TexV, TexCoord).r;

        // Convert to RGB
        y = 1.1643 * (y - 0.0625);
        u = u - 0.5;
        v = v - 0.5;
        r = y + 1.5958 * v;
        g = y - 0.39173 * u - 0.81290 * v;
        b = y + 2.017 * u;

        FragColor = vec4(r, g, b, 1.0);
    }
)";

bool YUVPlaneSimpleFrameRenderer::Initialize()
{
    bool success = true;

    Shader vs, fs;
    success &= vs.Create(m_YUVSimpleVertexShader, GL_VERTEX_SHADER);
    success &= fs.Create(m_YUVSimpleFragmentShader, GL_FRAGMENT_SHADER);

    success &= MyProgram.Create();
    success &= MyProgram.Attach(vs);
    success &= MyProgram.Attach(fs);

    if (!success) {
        spdlog::warn("YUVPlaneSimpleFrameRenderer: Shader program creation failed");
    }

    if (!MyProgram.Link()) {
        spdlog::error("YUVPlaneSimpleFrameRenderer: Unable to link shader program for video texture");
        vs.Delete();
        fs.Delete();
        return false;
    }
    vs.Delete();
    fs.Delete();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_Coords);
    glGenBuffers(1, &EBO);

    UniformTexY = glGetUniformLocation(MyProgram.ProgramId, "TexY");
    UniformTexU = glGetUniformLocation(MyProgram.ProgramId, "TexU");
    UniformTexV = glGetUniformLocation(MyProgram.ProgramId, "TexV");
    UniformMVPMatrix = glGetUniformLocation(MyProgram.ProgramId, "MVPMatrix");

    glBindVertexArray(VAO);

    IsGLOkay();

    const float left = 0.f;
    const float top = 0.f;
    const float right = 1.f;
    const float bottom = 1.f;

    const int floats_count = 5 * 4;
    const float xyzuv[floats_count] = {
        left,  bottom, 0.f,  0.f, 1.f,
        left,  top,    0.f,  0.f, 0.f,
        right, top,    0.f,  1.f, 0.f,
        right, bottom, 0.f,  1.f, 1.f
    };

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        xyzuv,
        GL_STREAM_DRAW);

    const int indices_count = 3 * 2;
    const uint32_t indices[indices_count] = {
        1, 0, 3,
        1, 3, 2
    };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        indices,
        GL_STREAM_DRAW);

    glBindVertexArray(0);

    TriangleIndexCount = indices_count;

    return IsGLOkay();
}

void YUVPlaneSimpleFrameRenderer::UpdatePosition(
    float left,
    float top,
    float width,
    float height)
{
    if (Left == left && Top == top && Width == width && Height == height) {
        return;
    }
    Left = left;
    Top = top;
    Width = width;
    Height = height;

    auto ortho = Matrix4::orthographic(0.f, 1.f, 1.f, 0.f, -1.f, 1.f);
    auto scaler = Matrix4::scale(Vector3(width, height, 1.f));
    auto transer = Matrix4::translation(Vector3(left, top, 0.f));
    Transform = ortho * transer * scaler;
}

bool YUVPlaneSimpleFrameRenderer::UpdateYUV(
    const uint8_t* y_ptr,
    const uint8_t* u_ptr,
    const uint8_t* v_ptr,
    int y_width,
    int y_height,
    int y_stride,
    int uv_width,
    int uv_height,
    int uv_stride)
{
    //ModuleLogger.Info("UpdateYUV: YUV_ptr=", (void*)YUV_ptr, " width=", width, " height=", height);

    if (y_width != WidthPixels || y_height != HeightPixels)
    {
        if (WidthPixels != 0) {
            glDeleteTextures(1, &TexY);
            glDeleteTextures(1, &TexU);
            glDeleteTextures(1, &TexV);
        }

        WidthPixels = y_width;
        HeightPixels = y_height;

        glGenTextures(1, &TexY);
        glBindTexture(GL_TEXTURE_2D, TexY);

        //GLuint linear = GL_LINEAR;
        GLuint linear = GL_NEAREST;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear);

        glGenTextures(1, &TexU);
        glBindTexture(GL_TEXTURE_2D, TexU);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear);

        glGenTextures(1, &TexV);
        glBindTexture(GL_TEXTURE_2D, TexV);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear);
    }

    glPixelStorei(GL_PACK_SWAP_BYTES, 0); // No swap
    glPixelStorei(GL_PACK_ROW_LENGTH, y_stride);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glBindTexture(GL_TEXTURE_2D, TexY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, y_width, y_height,
                    0, GL_RED, GL_UNSIGNED_BYTE, y_ptr);

    glBindTexture(GL_TEXTURE_2D, TexU);
    glPixelStorei(GL_PACK_ROW_LENGTH, uv_stride);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uv_width, uv_height,
                    0, GL_RED, GL_UNSIGNED_BYTE, u_ptr);

    glBindTexture(GL_TEXTURE_2D, TexV);
    glPixelStorei(GL_PACK_ROW_LENGTH, uv_stride);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uv_width, uv_height,
                    0, GL_RED, GL_UNSIGNED_BYTE, v_ptr);

    return IsGLOkay();
}

bool YUVPlaneSimpleFrameRenderer::Render()
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    MyProgram.Use();

    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexY);
    glUniform1i(UniformTexY, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TexU);
    glUniform1i(UniformTexU, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, TexV);
    glUniform1i(UniformTexV, 2);

    float* m = Transform.GetPtr();
    glUniformMatrix4fv(UniformMVPMatrix, 1, GL_FALSE, m);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        GL_VERTEX_ATTRIB_OFFSET(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        GL_VERTEX_ATTRIB_OFFSET(3 * sizeof(float)));

    glDrawElements(
        GL_TRIANGLES,
        TriangleIndexCount,
        GL_UNSIGNED_INT,
        0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);

    return IsGLOkay();
}

void YUVPlaneSimpleFrameRenderer::Shutdown()
{
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &VAO);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &VBO_Coords);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &EBO);

    glDeleteTextures(1, &TexY);
    glDeleteTextures(1, &TexU);
    glDeleteTextures(1, &TexV);

    MyProgram.Delete();
}


//------------------------------------------------------------------------------
// OpenGL NV12 Two-plane Simple Frame Renderer

static const char* m_NV12SimpleVertexShader = R"(
    #version 330 core
    uniform mat4 MVPMatrix;
    layout (location = 0) in vec4 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    out vec4 TexPos;
    void main()
    {
        gl_Position = MVPMatrix * aPos;
        TexPos = gl_Position;
        TexCoord = aTexCoord;
    }
)";

static const char* m_NV12SimpleFragmentShader = R"(
    #version 330 core
    #ifdef GL_ES
        precision highp float;
    #endif
    uniform sampler2D TexY;
    uniform sampler2D TexUV;
    in vec2 TexCoord;
    in vec4 TexPos;
    out vec4 FragColor;
    void main()
    {
        // Sample NV12 color
        float r, g, b, y, u, v;
        y = texture(TexY, TexCoord).r;
        u = texture(TexUV, TexCoord).r;
        v = texture(TexUV, TexCoord).g;

        // Convert to RGB
        y = 1.1643 * (y - 0.0625);
        u = u - 0.5;
        v = v - 0.5;
        r = y + 1.5958 * v;
        g = y - 0.39173 * u - 0.81290 * v;
        b = y + 2.017 * u;

        FragColor = vec4(r, g, b, 1.0);
        //FragColor = vec4(TexPos.z, TexPos.z, TexPos.z, 1.0);
    }
)";

bool NV12PlaneSimpleFrameRenderer::Initialize()
{
    bool success = true;

    Shader vs, fs;
    success &= vs.Create(m_NV12SimpleVertexShader, GL_VERTEX_SHADER);
    success &= fs.Create(m_NV12SimpleFragmentShader, GL_FRAGMENT_SHADER);

    success &= MyProgram.Create();
    success &= MyProgram.Attach(vs);
    success &= MyProgram.Attach(fs);

    if (!success) {
        spdlog::warn("NV12PlaneSimpleFrameRenderer: Shader program creation failed");
    }

    if (!MyProgram.Link()) {
        spdlog::error("NV12PlaneSimpleFrameRenderer: Unable to link shader program for video texture");
        vs.Delete();
        fs.Delete();
        return false;
    }
    vs.Delete();
    fs.Delete();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_Coords);
    glGenBuffers(1, &EBO);

    UniformTexY = glGetUniformLocation(MyProgram.ProgramId, "TexY");
    UniformTexUV = glGetUniformLocation(MyProgram.ProgramId, "TexUV");
    UniformMVPMatrix = glGetUniformLocation(MyProgram.ProgramId, "MVPMatrix");

    glBindVertexArray(VAO);

    IsGLOkay();

    const float left = 0.f;
    const float top = 0.f;
    const float right = 1.f;
    const float bottom = 1.f;

    const int floats_count = 5 * 4;
    const float xyzuv[floats_count] = {
        left,  bottom, 0.f,  0.f, 1.f,
        left,  top,    0.f,  0.f, 0.f,
        right, top,    0.f,  1.f, 0.f,
        right, bottom, 0.f,  1.f, 1.f
    };

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        xyzuv,
        GL_STREAM_DRAW);

    const int indices_count = 3 * 2;
    const uint32_t indices[indices_count] = {
        1, 0, 3,
        1, 3, 2
    };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        indices,
        GL_STREAM_DRAW);

    glBindVertexArray(0);

    TriangleIndexCount = indices_count;

    return IsGLOkay();
}

void NV12PlaneSimpleFrameRenderer::UpdatePosition(
    float left,
    float top,
    float width,
    float height)
{
    if (Left == left && Top == top && Width == width && Height == height) {
        return;
    }
    Left = left;
    Top = top;
    Width = width;
    Height = height;

    auto ortho = Matrix4::orthographic(0.f, 1.f, 1.f, 0.f, -1.f, 1.f);
    auto scaler = Matrix4::scale(Vector3(width, height, 1.f));
    auto transer = Matrix4::translation(Vector3(left, top, 0.f));
    Transform = ortho * transer * scaler;
}

bool NV12PlaneSimpleFrameRenderer::UpdateNV12(
    const uint8_t* y_ptr,
    const uint8_t* uv_ptr,
    int y_width,
    int y_height,
    int y_stride,
    int uv_width,
    int uv_height,
    int uv_stride)
{
    CORE_UNUSED(uv_stride);
    //ModuleLogger.Info("UpdateYUV: YUV_ptr=", (void*)YUV_ptr, " width=", width, " height=", height);

    if (y_width != WidthPixels || y_height != HeightPixels)
    {
        if (WidthPixels != 0) {
            glDeleteTextures(1, &TexY);
            glDeleteTextures(1, &TexUV);
        }

        WidthPixels = y_width;
        HeightPixels = y_height;

        glGenTextures(1, &TexY);
        glBindTexture(GL_TEXTURE_2D, TexY);

        //GLuint linear = GL_LINEAR;
        GLuint linear = GL_NEAREST;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear);

        glGenTextures(1, &TexUV);
        glBindTexture(GL_TEXTURE_2D, TexUV);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear);
    }

    glPixelStorei(GL_PACK_SWAP_BYTES, 0); // No swap
    glPixelStorei(GL_PACK_ROW_LENGTH, y_stride);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glBindTexture(GL_TEXTURE_2D, TexY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, y_width, y_height,
                    0, GL_RED, GL_UNSIGNED_BYTE, y_ptr);

    glBindTexture(GL_TEXTURE_2D, TexUV);
    //glPixelStorei(GL_PACK_ROW_LENGTH, uv_stride);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, uv_width, uv_height,
                    0, GL_RG, GL_UNSIGNED_BYTE, uv_ptr);

    return IsGLOkay();
}

bool NV12PlaneSimpleFrameRenderer::Render()
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    MyProgram.Use();

    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexY);
    glUniform1i(UniformTexY, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TexUV);
    glUniform1i(UniformTexUV, 1);

    float* m = Transform.GetPtr();
    glUniformMatrix4fv(UniformMVPMatrix, 1, GL_FALSE, m);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        GL_VERTEX_ATTRIB_OFFSET(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        GL_VERTEX_ATTRIB_OFFSET(3 * sizeof(float)));

    glDrawElements(
        GL_TRIANGLES,
        TriangleIndexCount,
        GL_UNSIGNED_INT,
        0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);

    return IsGLOkay();
}

void NV12PlaneSimpleFrameRenderer::Shutdown()
{
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &VAO);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &VBO_Coords);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &EBO);

    glDeleteTextures(1, &TexY);
    glDeleteTextures(1, &TexUV);

    MyProgram.Delete();
}


//------------------------------------------------------------------------------
// ImageTilingRenderer

bool ImageTilingRenderer::Initialize()
{
    for (int i = 0; i < kMaxTiledImages; ++i)
    {
        if (!FrameRenderersYUV[i].Initialize()) {
            spdlog::error("Mesh renderer YUV failed to initialize");
            return false;
        }
        if (!FrameRenderersNV12[i].Initialize()) {
            spdlog::error("Mesh renderer NV12 failed to initialize");
            return false;
        }
    }

    return true;
}

void ImageTilingRenderer::Shutdown()
{
    for (int i = 0; i < kMaxTiledImages; ++i) {
        FrameRenderersYUV[i].Shutdown();
        FrameRenderersNV12[i].Shutdown();
    }
}

void ImageTilingRenderer::SetImage(
    int index,
    TileImageData data)
{
    Ptrs[index] = data;
}

bool ImageTilingRenderer::Render(
    int width,
    int height,
    int image_count,
    int image_width,
    int image_height,
    bool is_nv12)
{
    if (image_count <= 0) {
        return true;
    }
    if (image_count > kMaxTiledImages) {
        image_count = kMaxTiledImages;
    }

    const float aspect_ratio = image_width / (float)image_height;
    Tiling.SolveFit(width, height, image_count, aspect_ratio);

    const float last_x = 1.f - Tiling.TileWidth;
    const float last_y = 1.f - Tiling.TileHeight;

    // Start at the end
    float offset_x = last_x, offset_y = last_y;

    // Work backwards
    for (int i = 0, tile = 0; i < image_count; ++i, ++tile)
    {
        if (tile > 0) {
            if (Tiling.RowFirst) {
                offset_x -= Tiling.TileWidth;
                if (tile % Tiling.TileSpan == 0) {
                    offset_x = last_x;
                    offset_y -= Tiling.TileHeight;
                }
            } else {
                offset_y -= Tiling.TileHeight;
                if (tile % Tiling.TileSpan == 0) {
                    offset_x -= Tiling.TileWidth;
                    offset_y = last_y;
                }
            }
        }

        auto& ptrs = Ptrs[i];
        if (!ptrs.Y) {
            continue;
        }

        if (is_nv12)
        {
            FrameRenderersNV12[i].UpdateNV12(
                ptrs.Y,
                ptrs.U,
                image_width,
                image_height,
                image_width, // stride
                image_width / 2,
                image_height / 2,
                image_width); // stride
            if (!IsGLOkay()) {
                spdlog::error("GL error: UpdateNV12 failed");
                return false;
            }

            FrameRenderersNV12[i].UpdatePosition(
                offset_x,
                offset_y,
                Tiling.TileWidth,
                Tiling.TileHeight);

            FrameRenderersNV12[i].Render();
            if (!IsGLOkay()) {
                spdlog::error("GL error: Render failed");
                return false;
            }
        }
        else
        {
            FrameRenderersYUV[i].UpdateYUV(
                ptrs.Y,
                ptrs.U,
                ptrs.V,
                image_width,
                image_height,
                image_width, // stride
                image_width / 2,
                image_height / 2,
                image_width / 2); // stride
            if (!IsGLOkay()) {
                spdlog::error("GL error: UpdateYUV failed");
                return false;
            }

            FrameRenderersYUV[i].UpdatePosition(
                offset_x,
                offset_y,
                Tiling.TileWidth,
                Tiling.TileHeight);

            FrameRenderersYUV[i].Render();
            if (!IsGLOkay()) {
                spdlog::error("GL error: Render failed");
                return false;
            }
        }
    }

    memset(&Ptrs, 0, sizeof(Ptrs));
    return true;
}


} // namespace core
