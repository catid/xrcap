// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    From a set of color images and associated point clouds, finds lightness
    and saturation values that best normalize overlapping points, in the hope
    that multiple captures will blend together cleanly.

    Depends on camera extrinsics.
*/

#pragma once

#include "CameraExtrinsics.hpp"

#include <cstdint>
#include <vector>
#include <memory>

namespace core {


//------------------------------------------------------------------------------
// Colorspaces

/*
    Conversion from NV12 to illumination-invariant color space:

    YCbCr -> RGB
    https://en.wikipedia.org/wiki/YCbCr#JPEG_conversion

    RGB -> XYZ
    https://en.wikipedia.org/wiki/CIE_1931_color_space

    XYZ -> Illumination invariant color space
    http://www.cs.harvard.edu/~sjg/papers/cspace.pdf
    https://github.com/jflalonde/utils/blob/master/mycode/color/xyz2ill.m

    RGB -> HSL
    https://www.rapidtables.com/convert/color/rgb-to-hsl.html
*/
void YCbCrToRGB(
    uint8_t Y, uint8_t Cb, uint8_t Cr,
    float& R, float& G, float& B);

void RGBToXYZ(
    float R, float G, float B,
    float& X, float& Y, float& Z);

void XYZToIlluminationInvariant(
    float X, float Y, float Z,
    float& Ix, float& Iy, float& Iz);

Eigen::Vector3f ReadIlluminationInvariantNV12Color(
    const uint8_t* y_plane,
    const uint8_t* uv_plane,
    unsigned width,
    unsigned x,
    unsigned y);


//------------------------------------------------------------------------------
// Point Cloud

struct PerspectiveMetadata
{
    uint64_t Guid = 0;
    uint32_t CameraIndex = 0;

    float Brightness = 0.f; // -100 to +100 (default 0)
    float Saturation = 1.f; // 0.0 to 10.0 (default 1)
};

struct LightCloudInputs
{
    VerticesInfo Info;
    PerspectiveMetadata Metadata;
    AlignmentTransform Extrinsics;
};

// nanoflann-compatible point cloud objects with median luminance
struct KdtreePointCloud
{
    LightCloudInputs Input;
    std::vector<uint8_t> YPlane, UVPlane;
    std::vector<float> XyzuvVertices;

    // +X = Right
    // +Y = Down
    // +Z = Forward
    float CameraX = 0.f, CameraY = 0.f, CameraZ = 0.f;

    // [ x, y, z, brightness, saturation ]
    static const int kStride = 5;
    std::vector<float> Floats;
    unsigned PointCount = 0;


    void Set(const LightCloudInputs& input);

    void ApplyTransforms();

    inline size_t kdtree_get_point_count() const
    {
        return PointCount;
    }

    // Returns the dim'th component of the idx'th point in the class:
    // Since this is inlined and the "dim" argument is typically an immediate value, the
    //  "if/else's" are actually solved at compile time.
    inline float kdtree_get_pt(const size_t idx, const size_t dim) const
    {
        const float* f = Floats.data();
        return f[idx * kStride + dim];
    }

    // Optional bounding-box computation: return false to default to a standard bbox computation loop.
    //   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
    //   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }
};


//------------------------------------------------------------------------------
// Color Normalization

/*
    This samples image colors near mesh points that are similar in multiple,
    registered depth camera meshes to determine how to adjust the brightness
    of each camera image using post-processing to normalize lighting between
    cameras.

    Algorithm:

        (0) Use extrinsics information to compare only neighboring cameras.
        (1) Use a fast K-Nearest Neighbors approach to identify the points
        that are roughly shared between cameras.
        (2) Sample the brightness of the nearest image patch in each camera.
        (3) Compare these in aggregate to determine how much brighter one
        camera is as compared to the other.

    The output of the algorithm is a fairly rough measure of how much brighter
    or dimmer each camera needs to be to match each other camera.  We provide
    this information back to the capture server so that it can adjust the 
    brightness of each image using post-processing before video encoding, and
    then we iteratively improve this estimate as the new images arrive.

    Once the user is satisfied with the lighting matching, they disable the
    auto-exposure setting in the UI to lock this configuration for capture.

    Why are we doing it this way?

    The Azure Kinect DK at this time does not support manual configuration
    well enough to do everything with the camera configuration.  Instead we
    can only reliably lock the auto-exposure settings.  The AWB cannot be
    locked because there is a bug where its value can go out of range when
    configured automatically, and so we cannot manually set it to that value.

    This means we are stuck with post-processing, which must be done via
    the Intel VPP ProcAmp feature in order to run in real-time without
    introducing a new expensive GPU processing step.  It's not clear how the
    ProcAmp settings will affect the perceived brightness, so we must do it
    in a large feedback loop.
*/

// Quickly copy luminance and point data to cloud in foreground thread.
// We split this up because clouds are not reference counted and we need to
// mainly just copy data in the foreground to avoid blocking rendering.
void ForegroundCreateClouds(
    const std::vector<LightCloudInputs>& inputs,
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds);

// Extract lighting information in background thread
void ExtractCloudLighting(
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds);

// Solve for lighting offsets for each camera in background thread
// Returns false if normalization was not possible.  Ensure that cameras
// are viewing a lot of the same well-lit objects in the scene.
// Brightness ranges from -100 to +100 (default 0)
// Saturation ranges from 0.0 to 10.0 (default 1)
bool ColorNormalization(
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds,
    std::vector<float>& brightness_result,
    std::vector<float>& saturation_result);

// Given the read-back AWB from each camera, select one AWB to configure on all cameras.
// This is currently the median AWB, bounded by the range allowed by the hardware.
unsigned NormalizeAWB(std::vector<uint32_t> awb_readback);


} // namespace core
