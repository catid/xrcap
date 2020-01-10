// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "GLCore.hpp"

#include "core_logging.hpp"

namespace core {


//------------------------------------------------------------------------------
// OpenGL Tools

const char* GLErrorToString(GLenum code)
{
    switch (code)
    {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default: break;
    }
    return "(unknown)";
}

bool IsGLOkay()
{
    GLenum code = glGetError();
    if (code != GL_NO_ERROR) {
        spdlog::error("OpenGL error code={} {}", code, GLErrorToString(code));
        return false; // Error occurred
    }
    return true; // No error
}


//------------------------------------------------------------------------------
// OpenGL Shader Helper Class

bool Shader::Create(
    const char* shaderCode,
    GLenum type /* e.g. GL_VERTEX_SHADER */)
{
    ShaderId = glCreateShader(type);
    if (ShaderId == 0) {
        spdlog::error("glCreateShader failed");
        IsGLOkay();
        return false;
    }

    // Compile the shader code
    glShaderSource(ShaderId, 1, &shaderCode, NULL);
    glCompileShader(ShaderId);

    GLint success = 0;
    glGetShaderiv(ShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLint len = 0;
        glGetShaderiv(ShaderId, GL_INFO_LOG_LENGTH, &len);
        if (len > 0)
        {
            std::vector<char> info(len, '\0');
            char *info_ptr = &info[0];

            glGetShaderInfoLog(ShaderId, len, &len, info_ptr);
            spdlog::error("Shader compilation failed: {}", info_ptr);
        }

        return false;
    }

    return IsGLOkay();
}

void Shader::Delete()
{
    glDeleteShader(ShaderId);
    ShaderId = 0;
}


//------------------------------------------------------------------------------
// OpenGL Program Helper Class

bool Program::Create()
{
    ProgramId = glCreateProgram();
    if (ProgramId == 0) {
        spdlog::error("glCreateProgram failed");
        IsGLOkay();
        return false;
    }

    return IsGLOkay();
}

bool Program::Attach(const Shader& shader)
{
    glAttachShader(ProgramId, shader.ShaderId);
    return IsGLOkay();
}

bool Program::Link()
{
    glLinkProgram(ProgramId);

    GLint success = 0;
    glGetProgramiv(ProgramId, GL_LINK_STATUS, &success);
    if (!success)
    {
        GLint len = 0;
        glGetProgramiv(ProgramId, GL_INFO_LOG_LENGTH, &len);
        if (len > 0)
        {
            std::vector<char> info(len, '\0');
            char *info_ptr = &info[0];

            glGetProgramInfoLog(ProgramId, len, &len, info_ptr);
            spdlog::error("Program linking failed: {}", info_ptr);
        }

        return false;
    }

    return IsGLOkay();
}

void Program::Use()
{
    glUseProgram(ProgramId);
}

void Program::SetBool(const std::string &name, bool value) const
{
    glUniform1i(glGetUniformLocation(ProgramId, name.c_str()), (int)value);
}

void Program::SetInt(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(ProgramId, name.c_str()), value);
}

void Program::SetFloat(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(ProgramId, name.c_str()), value);
}

void Program::Delete()
{
    if (ProgramId != 0) {
        glDeleteProgram(ProgramId);
        ProgramId = 0;
    }
}


} // namespace core
