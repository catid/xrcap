// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Trackball Camera
*/

#pragma once

#include <core.hpp>
#include <vectormath.hpp>

namespace core {


//------------------------------------------------------------------------------
// Constants

#ifndef M_PI_FLOAT
#define M_PI_FLOAT 3.1415926535f
#endif


//------------------------------------------------------------------------------
// Trackball Camera

class TrackballCamera
{
public:
    TrackballCamera()
    {
        Reset();
    }
    void Reset();

    void OnMouseDown(int button, float x, float y);
    void OnMouseMove(float x, float y);
    void OnMouseUp(int button);
    void OnMouseScroll(float x, float y);

    Matrix4 GetCameraViewTransform() const
    {
        return CameraViewTransform;
    }

    // Note left-hand coordinate system: (+x = right, +y = down, +z = forward)
    void SnapToAngle(float yaw, float pitch);

    // e.g. Show scene from position of camera
    void SnapToPose(float x, float y, float z);

protected:
    bool RotationDragging = false;
    float DragStartX = 0.f;
    float DragStartY = 0.f;

    float CenterDistance = 0.f;
    float Yaw = 0.f, Pitch = 0.f;

    Matrix4 CameraViewTransform;

    void UpdateViewTransform();
};


} // namespace core
