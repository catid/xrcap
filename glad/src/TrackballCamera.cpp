// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "TrackballCamera.hpp"

#include <core_logging.hpp>

namespace core {


//------------------------------------------------------------------------------
// Tools

/*
    RotationFromEulerAngles()

    This solves the issue where rotating on one axis and then by another causes
    the second rotation to get distorted.  Instead the rotation on both axes are
    performed at once.

    +yaw (radians) rotates the camera clockwise.
    +pitch (radians) rotates the camera so it is looking slant-downward.

    Coordinate system used by mesh:
    (+x, +y, +z) = (+right, +down, +forward)
*/
static Quat RotationFromEulerAngles(float yaw, float pitch)
{
    /*
        Math review:

        Quaternion from axis-angle:
            qx = ax * sin(angle / 2)
            qy = ay * sin(angle / 2)
            qz = az * sin(angle / 2)
            qw = cos(angle / 2)

        cross(a, b) = |a|*|b|*sin(angle)*(normal vector to a, b)
        dot(a, b) = |a|*|b|*cos(angle)
    */

    // Signs and offsets are tuning to fix up the coordinate system
    yaw += M_PI_FLOAT;

    // Half angle for quaternion formula
    yaw *= 0.5f;
    pitch *= 0.5f;

    // Points on sphere chosen by yaw, pitch
    Vector3 p_f(cosf(yaw), 0.f, -sinf(yaw));
    Vector3 p_t(0.f, -cosf(pitch), -sinf(pitch));

    // Calculate cross- and dot-products for quaternion formula
    Vector3 c = cross(p_f, p_t);
    const float cos_angle = dot(p_f, p_t);

    return Quat(c.getX(), c.getY(), c.getZ(), cos_angle);
}


//------------------------------------------------------------------------------
// Trackball Camera

void TrackballCamera::UpdateViewTransform()
{
    CameraViewTransform = Matrix4::translation(Vector3(0.f, 0.f, -CenterDistance));
    CameraViewTransform *= Matrix4::rotation(RotationFromEulerAngles(Yaw, Pitch));
}

void TrackballCamera::Reset()
{
    RotationDragging = false;

    CenterDistance = 2.0f;
    Yaw = 0.f;
    Pitch = 0.f;
    UpdateViewTransform();
}

void TrackballCamera::SnapToAngle(float yaw, float pitch)
{
    RotationDragging = false;
    Yaw = yaw;
    Pitch = pitch;
    UpdateViewTransform();
}

void TrackballCamera::SnapToPose(float x, float y, float z)
{
    // Coordinate system: (+x, +y, +z) = (right, down, forward)

    Vector3 p(x, y, z);

    const float d = length(p);
    if (d <= 0.00001f) {
        return; // Cannot snap to origin
    }
    const float inv_d = 1.f / d;

    RotationDragging = false;
    CenterDistance = d;
    p *= inv_d; // Normalize p

    Yaw = atan2f(x, z);

    const float w = length(Vector2(x, z));
    Pitch = atan2f(-y, w);

    UpdateViewTransform();
}

void TrackballCamera::OnMouseDown(int button, float x, float y)
{
    if (button == 0) {
        RotationDragging = true;
        DragStartX = x;
        DragStartY = y;
    }
}

void TrackballCamera::OnMouseMove(float x, float y)
{
    if (RotationDragging)
    {
        float dx = (x - DragStartX) / 500.f;
        float dy = (y - DragStartY) / 500.f;

        Yaw += dx;
        Pitch += dy;
        if (Pitch > M_PI_FLOAT * 0.5f) {
            Pitch = M_PI_FLOAT * 0.5f;
        }
        else if (Pitch < -M_PI_FLOAT * 0.5f) {
            Pitch = -M_PI_FLOAT * 0.5f;
        }
        if (Yaw >= M_PI_FLOAT * 2.f) {
            Yaw -= M_PI_FLOAT * 2.f;
        }
        if (Yaw < 0.f) {
            Yaw += M_PI_FLOAT * 2.f;
        }

        DragStartX = x;
        DragStartY = y;

        UpdateViewTransform();
    }
}

void TrackballCamera::OnMouseUp(int button)
{
    if (button == 0) {
        RotationDragging = false;
    }
}

void TrackballCamera::OnMouseScroll(float x, float y)
{
    CORE_UNUSED(x);
    if (y == 0.f) {
        return;
    }

    CenterDistance -= y * 0.25f;
    if (CenterDistance > 20.0f) {
        CenterDistance = 20.0f;
    }
    else if (CenterDistance < 0.01f) {
        CenterDistance = 0.01f;
    }

    UpdateViewTransform();
}


} // namespace core
