// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Nuklear library with GLFW + OpenGL3.3 options enabled
*/

#pragma once

#include "core.hpp"

#include "glad.h"
#include <glfw/glfw3.h>

#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_STANDARD_IO

#include "nuklear.h"
#include "nuklear_glfw_gl3.h"

#include "GLCore.hpp" // Some extra tools
