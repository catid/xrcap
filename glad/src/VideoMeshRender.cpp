// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "VideoMeshRender.hpp"

#include <core_logging.hpp> // core

namespace core {


//------------------------------------------------------------------------------
// OpenGL YUV Multi-plane Video Frame Renderer

static const char* m_YUVVideoVertexShader = R"(
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

static const char* m_YUVVideoFragmentShader = R"(
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
        //FragColor = vec4(TexPos.z, TexPos.z, TexPos.z, 1.0);
    }
)";

bool YUVVideoMeshRender::Initialize()
{
    bool success = true;

    Shader vs, fs;
    success &= vs.Create(m_YUVVideoVertexShader, GL_VERTEX_SHADER);
    success &= fs.Create(m_YUVVideoFragmentShader, GL_FRAGMENT_SHADER);

    success &= MyProgram.Create();
    success &= MyProgram.Attach(vs);
    success &= MyProgram.Attach(fs);

    if (!success) {
        spdlog::warn("YUVVideoMeshRender: Shader program creation failed");
    }

    if (!MyProgram.Link()) {
        spdlog::error("YUVVideoMeshRender: Unable to link shader program for video texture");
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

    return IsGLOkay();
}

bool YUVVideoMeshRender::UpdateYUV(
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

bool YUVVideoMeshRender::UpdateMesh(
    const float* xyzuv_ptr,
    int floats_count,
    const uint32_t* indices_ptr,
    int indices_count)
{
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        xyzuv_ptr,
        GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        indices_ptr,
        GL_STREAM_DRAW);

    glBindVertexArray(0);

    TriangleIndexCount = indices_count;

    return IsGLOkay();
}

bool YUVVideoMeshRender::Render(Matrix4& mvp)
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

    float* m = mvp.GetPtr();
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

void YUVVideoMeshRender::Shutdown()
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
// OpenGL NV12 Multi-plane Video Frame Renderer

static const char* m_NV12VideoVertexShader = R"(
    #version 330 core
    uniform mat4 MVPMatrix;
    uniform vec4 CameraPos;
    layout (location = 0) in vec4 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    out vec4 TexPos;
    void main()
    {
        vec4 p = MVPMatrix * aPos;

        //float offset = length(CameraPos.xyz - p.xyz) / CameraPos.w;
        //p.z = (p.z * 0.5) - 0.5 + offset;

        gl_Position = p;
        TexPos = gl_Position;
        TexCoord = aTexCoord;
    }
)";

static const char* m_NV12VideoFragmentShader = R"(
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

bool NV12VideoMeshRender::Initialize()
{
    bool success = true;

    Shader vs, fs;
    success &= vs.Create(m_NV12VideoVertexShader, GL_VERTEX_SHADER);
    success &= fs.Create(m_NV12VideoFragmentShader, GL_FRAGMENT_SHADER);

    success &= MyProgram.Create();
    success &= MyProgram.Attach(vs);
    success &= MyProgram.Attach(fs);

    if (!success) {
        spdlog::warn("NV12VideoMeshRender: Shader program creation failed");
    }

    if (!MyProgram.Link()) {
        spdlog::error("NV12VideoMeshRender: Unable to link shader program for video texture");
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
    UniformCameraPos = glGetUniformLocation(MyProgram.ProgramId, "CameraPos");

    return IsGLOkay();
}

bool NV12VideoMeshRender::UpdateNV12(
    const uint8_t* y_ptr,
    const uint8_t* uv_ptr,
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
            glDeleteTextures(1, &TexUV);
        }

        WidthPixels = y_width;
        HeightPixels = y_height;

        glGenTextures(1, &TexY);
        glBindTexture(GL_TEXTURE_2D, TexY);

        GLuint linear = GL_LINEAR;
        //GLuint linear = GL_NEAREST;

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
    glPixelStorei(GL_PACK_ROW_LENGTH, uv_stride);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, uv_width, uv_height,
                    0, GL_RG, GL_UNSIGNED_BYTE, uv_ptr);

    return IsGLOkay();
}

bool NV12VideoMeshRender::UpdateMesh(
    const float* xyzuv_ptr,
    int floats_count,
    const uint32_t* indices_ptr,
    int indices_count)
{
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_Coords);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ARRAY_BUFFER,
        floats_count * sizeof(float),
        xyzuv_ptr,
        GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        nullptr,
        GL_STREAM_DRAW);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices_count * sizeof(uint32_t),
        indices_ptr,
        GL_STREAM_DRAW);

    glBindVertexArray(0);

    TriangleIndexCount = indices_count;

    return IsGLOkay();
}

bool NV12VideoMeshRender::Render(Matrix4& mvp, const float* camera_pos)
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

    float* m = mvp.GetPtr();
    glUniformMatrix4fv(UniformMVPMatrix, 1, GL_FALSE, m);

    glUniform4fv(UniformCameraPos, 1, camera_pos);

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

void NV12VideoMeshRender::Shutdown()
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


} // namespace core
