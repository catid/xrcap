// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "ColorNormalization.hpp"

#include <core_logging.hpp>

#include "nanoflann.hpp"

namespace core {


//------------------------------------------------------------------------------
// Percentile

// Note: This partially sorts and modifies the provided data
template<typename T>
static inline T GetPercentile(std::vector<T>& data, float percentile)
{
    if (data.empty()) {
        const T empty{};
        return empty;
    }
    if (data.size() == 1) {
        return data[0];
    }

    using offset_t = typename std::vector<T>::size_type;

    const size_t count = data.size();
    const offset_t goalOffset = (offset_t)(count * percentile);

    std::nth_element(data.begin(), data.begin() + goalOffset, data.end());

    return data[goalOffset];
}


//------------------------------------------------------------------------------
// Colorspaces

void YCbCrToRGB(
    uint8_t Y, uint8_t Cb, uint8_t Cr,
    float& R, float& G, float& B)
{
    R = Y + 1.402f * (Cr - 128);
    G = Y - 0.344136f * (Cb - 128) - 0.714136f * (Cr - 128);
    B = Y + 1.772f * (Cb - 128);
}

void RGBToXYZ(
    float R, float G, float B,
    float& X, float& Y, float& Z)
{
    const float f = 1.f / 0.17697f;

    X = f * 0.49f * R + f * 0.31f * G + f * 0.2f * B;
    Y = R + f * 0.8124f * G + f * 0.01063f * B;
    Z = f * 0.01f * G + f * 0.99f * B;
}

/*
    Threshold = 0.01:

    [19:28:35 -07:00] [I] For YCbCr input: Ix(max)=283.815 Ix(min)=-274.968
    [19:28:35 -07:00] [I] For YCbCr input: Iy(max)=154.635 Iy(min)=-156.265
    [19:28:35 -07:00] [I] For YCbCr input: Iz(max)=61.3345 Iz(min)=-99.0792
    [19:28:36 -07:00] [I] For RGB input: Ix(max)=222.59 Ix(min)=-11.3282
    [19:28:36 -07:00] [I] For RGB input: Iy(max)=103.759 Iy(min)=-89.5253
    [19:28:36 -07:00] [I] For RGB input: Iz(max)=61.3345 Iz(min)=-96.9097

    Threshold = 0.1:

    [19:29:58 -07:00] [I] For YCbCr input: Ix(max)=227.138 Ix(min)=-212.626
    [19:29:58 -07:00] [I] For YCbCr input: Iy(max)=123.852 Iy(min)=-126.643
    [19:29:58 -07:00] [I] For YCbCr input: Iz(max)=30.6672 Iz(min)=-99.0792
    [19:29:59 -07:00] [I] For RGB input: Ix(max)=170.073 Ix(min)=-5.66408
    [19:29:59 -07:00] [I] For RGB input: Iy(max)=85.9784 Iy(min)=-89.5253
    [19:29:59 -07:00] [I] For RGB input: Iz(max)=30.6672 Iz(min)=-96.9097

    Threshold = 1.0:

    [19:31:35 -07:00] [I] For YCbCr input: Ix(max)=170.461 Ix(min)=-150.297
    [19:31:35 -07:00] [I] For YCbCr input: Iy(max)=93.0718 Iy(min)=-97.0731
    [19:31:35 -07:00] [I] For YCbCr input: Iz(max)=0 Iz(min)=-99.0792
    [19:31:36 -07:00] [I] For RGB input: Ix(max)=118.181 Ix(min)=-4.38587
    [19:31:36 -07:00] [I] For RGB input: Iy(max)=68.1975 Iy(min)=-89.5253
    [19:31:36 -07:00] [I] For RGB input: Iz(max)=0 Iz(min)=-96.9097

    Threshold = 10.0:

    [22:54:10 -07:00] [I] For YCbCr input: Ix(max)=114.813 Ix(min)=-89.146
    [22:54:10 -07:00] [I] For YCbCr input: Iy(max)=62.346 Iy(min)=-67.5034
    [22:54:10 -07:00] [I] For YCbCr input: Iz(max)=0 Iz(min)=-99.0792
    [22:54:11 -07:00] [I] For RGB input: Ix(max)=73.1628 Ix(min)=-2.01379
    [22:54:11 -07:00] [I] For RGB input: Iy(max)=50.4167 Iy(min)=-63.0544
    [22:54:11 -07:00] [I] For RGB input: Iz(max)=0 Iz(min)=-96.9097
*/
void XYZToIlluminationInvariant(
    float X, float Y, float Z,
    float& Ix, float& Iy, float& Iz)
{
    float Bx, By, Bz;
    Bx = 0.9465229f * X + 0.2946927f * Y - 0.1313419f * Z;
    By = -0.1179179f * X + 0.9929960f * Y + 0.007371554f * Z;
    Bz = 0.09230461f * X - 0.04645794f * Y + 0.9946464f * Z;
    float Lx, Ly, Lz;
    // Authors recommend simple thresholding - Only bright colors work well with this method.
    const float threshold = 1.f;
    if (Bx < threshold) {
        Bx = threshold;
    }
    if (By < threshold) {
        By = threshold;
    }
    if (Bz < threshold) {
        Bz = threshold;
    }
    Lx = std::logf(Bx);
    Ly = std::logf(By);
    Lz = std::logf(Bz);
    Ix = 27.07439f * Lx - 22.80783f * Ly - 1.806681f * Lz;
    Iy = -5.646736f * Lx - 7.722125f * Ly + 12.86503f * Lz;
    Iz = -4.163133f * Lx - 4.579428f * Ly - 4.576049f * Lz;
}

Eigen::Vector3f ReadIlluminationInvariantNV12Color(
    const uint8_t* y_plane,
    const uint8_t* uv_plane,
    unsigned width,
    unsigned x,
    unsigned y)
{
    uint8_t Yc, Cb, Cr;
    Yc = y_plane[y * width + x];

    const unsigned uv_stride = (width / 2) * 2;
    const unsigned uv_offset = (y/2)*uv_stride + (x/2)*2;
    Cb = uv_plane[uv_offset];
    Cr = uv_plane[uv_offset+1];

    float R, G, B;
    YCbCrToRGB(Yc, Cb, Cr, R, G, B);
    float X, Y, Z;
    RGBToXYZ(R, G, B, X, Y, Z);
    float Ix, Iy, Iz;
    XYZToIlluminationInvariant(X, Y, Z, Ix, Iy, Iz);

    Eigen::Vector3f color;
    color.x() = Ix;
    color.y() = Iy;
    color.z() = Iz;
    return color;
}


//------------------------------------------------------------------------------
// Point Cloud

void KdtreePointCloud::Set(const LightCloudInputs& input)
{
    Input = input;
    auto& info = Input.Info;

    XyzuvVertices.resize(info.FloatsCount);
    memcpy(XyzuvVertices.data(), info.XyzuvVertices, sizeof(float) * XyzuvVertices.size());

    YPlane.resize(info.Width * info.Height);
    memcpy(YPlane.data(), info.Y, YPlane.size());

    UVPlane.resize(info.ChromaWidth * info.ChromaHeight * 2);
    memcpy(UVPlane.data(), info.UV, UVPlane.size());
}

void KdtreePointCloud::ApplyTransforms()
{
    Eigen::Matrix4f transform;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            transform(row, col) = Input.Extrinsics.Transform[row * 4 + col];
        }
    }

    Eigen::Matrix4f inv_transform = transform.inverse();

    CameraX = inv_transform(0, 3);
    CameraY = inv_transform(1, 3);
    CameraZ = inv_transform(2, 3);

    for (unsigned i = 0; i < PointCount; ++i)
    {
        float* output = &Floats[i * kStride];

        Eigen::Vector4f p = transform * Eigen::Vector4f(output[0], output[1], output[2], 1.f);
        output[0] = p.x();
        output[1] = p.y();
        output[2] = p.z();
    }
}


//------------------------------------------------------------------------------
// Color Normalization

void ForegroundCreateClouds(
    const std::vector<LightCloudInputs>& inputs,
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds)
{
    clouds.clear();

    for (size_t i = 0; i < inputs.size(); ++i) {
        std::shared_ptr<KdtreePointCloud> cloud = std::make_shared<KdtreePointCloud>();
        cloud->Set(inputs[i]);
        clouds.push_back(cloud);
    }
}

struct ColorHistogram
{
    uint8_t Histogram[256] = { 0 };
    unsigned Count = 0;

    // Expects input in the range 0..1
    // This will convert the float to an integer between 0..255
    void Insert(float value)
    {
        int index = static_cast<int>( value );
        if (index > 255) {
            index = 255;
        }
        else if (index < 0) {
            index = 0;
        }

        Histogram[index]++;
        ++Count;
    }

    bool Median(unsigned& result)
    {
        if (Count <= 0) {
            return false;
        }

        // Calculate median Y value
        unsigned accum = 0;
        const unsigned target = (Count + 1) / 2;
        unsigned median = 0;
        for (; median < 255; ++median) {
            accum += Histogram[median];
            if (accum >= target) {
                break;
            }
        }

        result = median;
        return true;
    }
};

void ExtractCloudLighting(
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds)
{
    for (auto& cloud : clouds)
    {
        auto& info = cloud->Input.Info;

        cloud->PointCount = info.FloatsCount / 5;
        cloud->Floats.resize(info.FloatsCount * KdtreePointCloud::kStride);
        unsigned filled_point_count = 0;

        const float* vertices = cloud->XyzuvVertices.data();

        const int width = info.Width;
        const int height = info.Height;
        const uint8_t* y_plane = cloud->YPlane.data();
        const uint8_t* uv_plane = cloud->UVPlane.data();

        std::vector<float> median_work_s;

        const unsigned count = cloud->PointCount;
        for (unsigned i = 0; i < count; ++i)
        {
            const float* input = vertices + i * 5;
            const float u = input[3];
            const float v = input[4];
            // FIXME: Is v flipped?

            ColorHistogram hist_l;
            median_work_s.clear();

            const int radius = 6;
            const int end_x = static_cast<int>( u * width ) + radius;
            const int end_y = static_cast<int>( v * height ) + radius;
            for (int y = end_y - radius * 2; y < end_y; ++y)
            {
                if (y < 0 || y >= height) {
                    continue;
                }
                const uint8_t* y_ptr = y_plane + y * width;
                const uint8_t* uv_ptr = uv_plane + (y / 2) * (width / 2) * 2;

                for (int x = end_x - radius * 2; x < end_x; ++x)
                {
                    if (x < 0 || x >= width) {
                        continue;
                    }
                    const uint8_t Y = y_ptr[x];
                    const uint8_t* uv = uv_ptr + (x / 2) * 2;
                    const uint8_t Cb = uv[0];
                    const uint8_t Cr = uv[1];

                    float R, G, B;
                    YCbCrToRGB(Y, Cb, Cr, R, G, B);

                    const float Cmax = std::max(R, std::max(G, B));
                    const float Cmin = std::min(R, std::min(G, B));

                    float S, L = (Cmax + Cmin) * 0.5f;

                    if (L < 1.f || L > 254.0f) {
                        S = 0.f;
                    } else {
                        S = (Cmax - Cmin) / (255.f - std::abs(2.f * L - 255.f));

                        if (S > 0.001f) {
                            const float log_s = std::logf(S);
                            median_work_s.push_back(log_s);
                        }
                    }

                    hist_l.Insert(L);
                }
            }

            unsigned median_l;
            if (!hist_l.Median(median_l)) {
                continue;
            }

            const float median_s = GetPercentile(median_work_s, 0.5f);

            float* output = &cloud->Floats[filled_point_count * KdtreePointCloud::kStride];
            output[0] = input[0];
            output[1] = input[1];
            output[2] = input[2];
            output[3] = static_cast<float>( median_l );
            output[4] = median_s;

            ++filled_point_count;
        }

        cloud->Floats.resize(filled_point_count * KdtreePointCloud::kStride);
        cloud->PointCount = filled_point_count;

        cloud->ApplyTransforms();
    }
}

// One of these for Saturation and Lightness
struct NormalizationSolverData
{
    int Count = 0;

    // Luminance of cloud #i - luminance of #smallest_index cloud
    std::vector<float> Deltas;

    // Workspace for calculating medians
    std::vector<float> DeltasWorkspace;

    // Offsets produced by solver
    std::vector<float> Offsets;

    // Workspace for solver
    std::vector<float> NextSteps;


    void Initialize(int count)
    {
        Count = count;

        Deltas.clear();
        DeltasWorkspace.clear();
        Offsets.clear();
        NextSteps.clear();

        Deltas.resize(count * count);
        Offsets.resize(count);
        NextSteps.resize(count);
    }

    // Average of peer offsets is the step we take
    float CalculateStep(int row)
    {
        float sum = 0.f;
        int sum_count = 0;

        const float row_offset = Offsets[row];

        const int count = Count;
        for (int col = 0; col < count; ++col)
        {
            // m(row, col) = cloud[row] - cloud[col]
            float delta = Deltas[row * count + col] + row_offset - Offsets[col];
            if (delta != 0.f) {
                sum += delta;
                ++sum_count;
            }
        }

        if (sum_count <= 0) {
            spdlog::warn("No brightness offset for camera {}", row);
            return 0.f;
        }

        const float avg = sum / sum_count;
        return -avg;
    }

    void Solve()
    {
        const int count = Count;

        for (int iterations = 0; iterations < 200; ++iterations)
        {
            const float step_rate = 0.02f;

            float step_sum = 0.f;

            for (int row = 0; row < count; ++row) {
                const float step = CalculateStep(row);
                //spdlog::info("Row {} step {}", row, step);
                step_sum += std::abs(step);
                NextSteps[row] = step * step_rate;
            }

            for (int row = 0; row < count; ++row) {
                Offsets[row] += NextSteps[row];
            }

            if (step_sum < 0.000001f) {
                break;
            }
        }

        for (int row = 0; row < count; ++row) {
            spdlog::info("Offset {} = {}", row, Offsets[row]);
        }
    }
};

static void RecenterFloats(std::vector<float>& result)
{
    const int count = static_cast<int>( result.size() );
    float sum = 0.f;
    for (int row = 0; row < count; ++row) {
        sum += result[row];
    }
    float avg = sum / count;
    for (int row = 0; row < count; ++row) {
        result[row] -= avg;
    }
}

bool ColorNormalization(
    std::vector<std::shared_ptr<KdtreePointCloud>>& clouds,
    std::vector<float>& brightness_result,
    std::vector<float>& saturation_result)
{
    const int count = static_cast<int>( clouds.size() );
    if (count <= 1) {
        return true;
    }

    const float kMaxDist = 0.025f;

    // construct a kd-tree index:
    typedef nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<float, KdtreePointCloud>,
        KdtreePointCloud,
        3> my_kd_tree_t;

    const nanoflann::KDTreeSingleIndexAdaptorParams adaptor_params(16);

    std::vector<std::shared_ptr<my_kd_tree_t>> trees(count);
    for (int i = 0; i < count; ++i)
    {
        trees[i] = std::make_shared<my_kd_tree_t>(3, *clouds[i], adaptor_params);
        trees[i]->buildIndex();
    }

    // Precompute distances
    std::vector<float> dists(count * count);
    for (int i = 0; i < count; ++i)
    {
        for (int j = i + 1; j < count; ++j)
        {
            const float dx = clouds[i]->CameraX - clouds[j]->CameraX;
            const float dy = clouds[i]->CameraY - clouds[j]->CameraY;
            const float dz = clouds[i]->CameraZ - clouds[j]->CameraZ;
            const float d = dx * dx + dy * dy + dz * dz;
            dists[i * count + j] = dists[j * count + i] = d;

            //spdlog::info("{} -> {} : distance = {}", i, j, d);
        }
    }

    NormalizationSolverData brightness, saturation;
    brightness.Initialize(count);
    saturation.Initialize(count);

    // Find two nearest cameras
    for (int i = 0; i < count; ++i)
    {
        // Find smallest
        int smallest_index = -1;
        float smallest_d = 0.f;
        for (int j = 0; j < count; ++j)
        {
            if (i == j) {
                continue;
            }
            const float d = dists[i * count + j];
            if (smallest_index == -1 || d < smallest_d) {
                smallest_index = j;
                smallest_d = d;
            }
        }

        // Find next smallest
        int next_smallest_index = -1;
        float next_smallest_d = 0.f;
        for (int j = 0; j < count; ++j)
        {
            if (i == j || smallest_index == j) {
                continue;
            }
            const float d = dists[i * count + j];
            if (next_smallest_index == -1 || d < next_smallest_d) {
                next_smallest_index = j;
                next_smallest_d = d;
            }
        }

        int best_indices[2] = {
            smallest_index,
            next_smallest_index
        };

        for (int j = 0; j < 2; ++j)
        {
            const int cloud_index = best_indices[j];
            if (cloud_index == -1) {
                continue;
            }

            //spdlog::info("Closest to camera {} i: camera={}", i, cloud_index);

            if (brightness.Deltas[i * count + cloud_index] != 0.f) {
                continue;
            }

            const size_t num_results = 1;
            nanoflann::KNNResultSet<float> results(num_results);
            const nanoflann::SearchParams search_params;

            brightness.DeltasWorkspace.clear();
            saturation.DeltasWorkspace.clear();

            const float* floats = clouds[i]->Floats.data();
            const unsigned pt_count = clouds[i]->PointCount;
            for (unsigned pt_i = 0; pt_i < pt_count; ++pt_i)
            {
                const float* vertex = floats + pt_i * KdtreePointCloud::kStride;

                size_t out_index;
                float out_dist_sqr;
                results.init(&out_index, &out_dist_sqr);
                trees[cloud_index]->findNeighbors(results, vertex, search_params);

                if (out_dist_sqr > kMaxDist * kMaxDist) {
                    continue;
                }

                const float* other = &clouds[cloud_index]->Floats[out_index * KdtreePointCloud::kStride];
                brightness.DeltasWorkspace.push_back(vertex[3] - other[3]);
                saturation.DeltasWorkspace.push_back(vertex[4] - other[4]);
            }

            const float brightness_median = GetPercentile(brightness.DeltasWorkspace, 0.5f);
            const float saturation_median = GetPercentile(saturation.DeltasWorkspace, 0.5f);

            // m(row, col) = cloud[row] - cloud[col]
            brightness.Deltas[i * count + cloud_index] = brightness_median;
            brightness.Deltas[cloud_index * count + i] = -brightness_median;
            saturation.Deltas[i * count + cloud_index] = saturation_median;
            saturation.Deltas[cloud_index * count + i] = -saturation_median;
        }
    }

    spdlog::info("Luminance deltas:");

    for (int row = 0; row < count; ++row)
    {
        std::string s = "    ";

        for (int col = 0; col < count; ++col)
        {
            const float ratio = brightness.Deltas[row * count + col];

            s += std::to_string(ratio);
            s += ", ";
        }

        spdlog::info("{}", s);
    }

    spdlog::info("Saturation deltas:");

    for (int row = 0; row < count; ++row)
    {
        std::string s = "    ";

        for (int col = 0; col < count; ++col)
        {
            const float ratio = saturation.Deltas[row * count + col];

            s += std::to_string(ratio);
            s += ", ";
        }

        spdlog::info("{}", s);
    }

    brightness_result.resize(count, 0.f);
    saturation_result.resize(count, 1.f);

    brightness.Solve();
    saturation.Solve();

    for (int row = 0; row < count; ++row)
    {
        float current_brightness = clouds[row]->Input.Metadata.Brightness;
        if (current_brightness > 100.f || current_brightness < -100.f) {
            spdlog::warn("Resetting out of control brightness for camera {}", row);
            brightness_result[row] = 0.f;
            continue;
        }

        float current_saturation = clouds[row]->Input.Metadata.Saturation;
        if (current_saturation < 0.f || current_saturation > 10.f) {
            spdlog::warn("Resetting out of control saturation for camera {}", row);
            saturation_result[row] = 1.f;
            continue;
        }

        const float offset_brightness = brightness.Offsets[row];
        if (offset_brightness == 0.f) {
            spdlog::warn("No brightness offset for camera {}", row);
        } else {
            spdlog::info("Adjusting brightness: camera {} current={} delta={}", row, current_brightness, offset_brightness);
        }
        brightness_result[row] = current_brightness + offset_brightness;

        const float offset_saturation = saturation.Offsets[row];
        if (offset_saturation == 0.f) {
            spdlog::warn("No saturation offset for camera {}", row);
        } else {
            spdlog::info("Adjusting saturation: camera {} current={} delta={}", row, current_saturation, offset_saturation);
            saturation_result[row] = std::logf(current_saturation) + offset_saturation;
        }
    }

    // Apply constraint: Values must center about zero
    RecenterFloats(brightness_result);
    RecenterFloats(saturation_result);

    for (int row = 0; row < count; ++row) {
        saturation_result[row] = std::expf(saturation_result[row]);
    }

    return true;
}

unsigned NormalizeAWB(std::vector<uint32_t> awb_readback)
{
    const uint32_t awb = GetPercentile(awb_readback, 0.5f);

    // Azure Kinect DK limits for SDK version 1.3
    // Note that the AWB often hovers around 2000 for common indoor lights,
    // so the lower limit on the manual setting is bogus.
    // https://github.com/microsoft/Azure-Kinect-Sensor-SDK/issues/903
    if (awb < 2500) {
        return 2500;
    }
    if (awb > 4500) {
        return 4500;
    }

    // Setting must be divisible by 10
    return (awb / 10) * 10;
}


} // namespace core
