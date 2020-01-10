// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Common OpenGL code
*/

#pragma once

#include "core.hpp"
#include "glad.h"

namespace core {


//------------------------------------------------------------------------------
// OpenGL Tools

// Returns true if no OpenGL error occurred
bool IsGLOkay();

// Used for offset argument of glVertexAttribPointer
#define GL_VERTEX_ATTRIB_OFFSET(i) (GLvoid*)( (char*)NULL + (i) )


//------------------------------------------------------------------------------
// OpenGL Shader Helper Class

struct Shader
{
    GLuint ShaderId = 0;

    bool Create(
        const char* shaderCode,
        GLenum type /* e.g. GL_VERTEX_SHADER */);

    void Delete();
};


//------------------------------------------------------------------------------
// OpenGL Program Helper Class

/**
 * Program must be created with a sequence of calls:
 * program.Create()
 * program.Attach(shader)
 * program.Attach(shader)
 * program.Attach(shader)
 * if (!program.Link()) { // handle error
 */
struct Program
{
    GLuint ProgramId = 0;

    bool Create();

    bool Attach(const Shader& shader);

    // Note after linking shaders, they can be deleted immediately
    bool Link();

    void Use();

    void SetBool(const std::string &name, bool value) const;
    void SetInt(const std::string &name, int value) const;
    void SetFloat(const std::string &name, float value) const;

    void Delete();
};


} // namespace core
