// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Depth mesh generator

    Takes 16-bit depth image and calibration as input.
    Produces OpenGL-compatible mesh x,y,z,u,v float vertices and a triangle index buffer.
    Zeroes out depth image values that cannot be used for the mesh for better compression.
*/

#pragma once

#include "DepthCalibration.hpp"

#include <Eigen/Eigen>

#include <vector>

namespace core {


//------------------------------------------------------------------------------
// Datatypes

// Cull parts of the mesh outside of a clipped region of interest
struct ClipRegion
{
    Eigen::Matrix4f Extrinsics;

    // Clip limits
    float Radius = 0.f;
    float Floor = 0.f;
    float Ceiling = 0.f;
};

struct ImageCropRegion
{
    unsigned CropX = 0, CropY = 0;
    unsigned CropW = 0, CropH = 0;

    // Grow to include the provided region
    // Returns true if it had to grow
    bool Grow(const ImageCropRegion& other);
};


//------------------------------------------------------------------------------
// DepthMesher

// After initialization this is safe to use from multiple threads in parallel.
// The idea would be to have one of these for each capture device.
class DepthMesher
{
public:
    // Must be called before other functions
    void Initialize(const CameraCalibration& calibration);

    // OpenGL-compatible x, y, z, u, v coordinates without padding.
    // depth: Must match the dimensions from Initialize().
    // Zeroes out depth entries that are not useful.
    // Optional: Zeroes out depth entries that are outside the clip region.
    // Optional: Provides image crop region.
    // face_painting_fix: Remove nearfield objects from the backdrop.
    //      Recommended for close-ups, should disable for 2 meter+ stand-off.
    void GenerateCoordinates(
        uint16_t* depth,
        const ClipRegion* clip,
        //ImageCropRegion* crop,
        std::vector<float>& coordinates,
        bool face_painting_fix = true,
        bool cull_depth = true);

    // OpenGL-compatible 3 indices for each triangle.
    // Call after GenerateCoordinates().
    // Triangle vertices are wound such that the right-hand rule yields
    // normals pointing towards the camera.
    void GenerateTriangleIndices(
        const uint16_t* depth,
        std::vector<uint32_t>& indices);

    // Get image crop from mesh clip region
    void CalculateCrop(
        const ClipRegion& clip,
        ImageCropRegion& crop);

protected:
    CameraCalibration Calibration;

    std::vector<float> DepthLookup;
};


//------------------------------------------------------------------------------
// TemporalDepthFilter

/*
    Applies a One Euro filter to depth video.

    Keeps a recent history for each depth pixel, and for pixels that do not
    change drastically, it applies median filter smoothing to restore missing
    data and to improve depth accuracy.

    Applications:
        Improving accuracy of depth meshes for extrinsics calibration of cameras
        by using the Iterative Closest Points (ICP) method to align the meshes.
        Improving accuracy of depth meshes for static background scene objects.
        Expected to be applied on the capture server.
*/
class TemporalDepthFilter
{
public:
    void Filter(uint16_t* depth, int w, int h);

protected:
    static const int kStride = 8;

    int Width = 0, Height = 0, Count = 0, Index = 0;
    std::vector<uint16_t> History;
};


//------------------------------------------------------------------------------
// DepthEdgeFilter

/*
    This filter cuts away edges of a mesh where there is the most uncertainty.

    The first pass through the image will filter large changes in depth as edges
    by setting the closer depth pixel to zero.  In this pass we also construct
    an integral image (see below).

    The second pass through the image we query the integral image to determine
    the number of neighbors for each depth image pixel.  We cull any pixel with
    fewer than 6 neighbors, which is considered an edge.
*/

class DepthEdgeFilter
{
public:
    void Filter(uint16_t* depth, int w, int h);

protected:
    /*
        Integral image counting the number of non-zero neighbors for each depth
        image pixel.  It will overflow and wrap around but since we are only
        querying the I.I. for small regions it is still unambiguous.

        Good reference for this data structure:
        https://www.mathworks.com/help/images/ref/integralimage.html
    */
    std::vector<uint16_t> IntegralImage;
};


} // namespace core
