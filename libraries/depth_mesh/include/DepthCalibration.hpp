// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Camera calibration

    Based on the Azure Kinect calibration software but may be extensible to other
    cameras.  This needed to be independent of the Kinect SDK because we want to
    run it on iOS and other platforms the SDK does not support.
*/

#pragma once

#include <cstdint>

namespace core {


//------------------------------------------------------------------------------
// Calibration

enum LensModels
{
    LensModel_Unknown,
    LensModel_Theta,
    LensModel_Polynomial_3K,
    LensModel_Rational_6KT,
    LensModel_Brown_Conrady,
    LensModel_Count
};

// Should be kept in sync with FileFormat.hpp ChunkIntrinsics structure
struct CameraIntrinsics
{
    // Sensor resolution
    int32_t Width, Height;

    // Quick reject for projections on the plane
    // Note: Have not seen this affect any of the calculations so disabled.
    //float MaxRadiusForProjection;

    // How to interpret the intrinsics (mostly has no effect)
    uint32_t LensModel = LensModel_Unknown;

    // Intrinsics
    float cx, cy;
    float fx, fy;
    float k[6];
    float codx, cody;
    float p1, p2;

    bool operator==(const CameraIntrinsics& rhs) const;
    inline bool operator!=(const CameraIntrinsics& rhs) const
    {
        return !(*this == rhs);
    }
};

struct CameraCalibration
{
    // Must be identical to XrcapCameraCalibration
    // Intrinsics for each camera
    CameraIntrinsics Color, Depth;

    // Extrinsics transform from 3D depth camera point to 3D point relative to color camera
    float RotationFromDepth[3*3];
    float TranslationFromDepth[3];

    bool operator==(const CameraCalibration& rhs) const;
    inline bool operator!=(const CameraCalibration& rhs) const
    {
        return !(*this == rhs);
    }
};


} // namespace core
