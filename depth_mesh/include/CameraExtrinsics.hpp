// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    From a set of point clouds, find a transform that best fits them together,
    producing the extrinsics for the depth cameras that generated the clouds.
*/

#pragma once

#include <DepthMesh.hpp> // depth_mesh
#include <Eigen/Eigen>

#include <cstdint>
#include <vector>

namespace core {


//------------------------------------------------------------------------------
// Registration

struct VerticesInfo
{
    // Vertices for mesh represented as repeated: x,y,z,u,v
    uint32_t FloatsCount = 0;
    float* XyzuvVertices = nullptr;

    // Accelerometer reading for extrinsics calibration
    float Accelerometer[3];

    CameraCalibration* Calibration = nullptr;

    // Image format is NV12, which is two channels.
    // Size of image and Y channel
    int32_t Width = 0, Height = 0;
    uint8_t* Y = nullptr;

    // Size of U, V channels
    int32_t ChromaWidth = 0, ChromaHeight = 0;
    uint8_t* UV = nullptr;
};

// Transform from this camera to the scene
struct AlignmentTransform
{
    float Transform[16];
    bool Identity = true;

    inline void operator=(const Eigen::Matrix4f& src)
    {
        Identity = src.isIdentity();
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                Transform[row * 4 + col] = src(row, col);
            }
        }
    }
    inline void Set(Eigen::Matrix4f& dest) const
    {
        if (Identity) {
            dest = Eigen::Matrix4f::Identity();
        } else {
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    dest(row, col) = Transform[row * 4 + col];
                }
            }
        }
    }
};

// Returns result in `extrinsics`.
// Returns false if registration was not possible.  Try adding more features to the scene.
bool CalculateExtrinsics(
    const std::vector<VerticesInfo>& vertices,
    std::vector<AlignmentTransform>& extrinsics);

// Requires previous extrinsics otherwise it will fail.
// If successful it will update the extrinsics with the new results.
// Returns false if registration was not possible.  Try adding more features to the scene.
bool RefineExtrinsics(
    const std::vector<VerticesInfo>& vertices,
    std::vector<AlignmentTransform>& extrinsics);


} // namespace core
