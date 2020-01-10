// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

/*
    Using a lot of algorithms from intrinsic_transformation.c from the Microsoft
    Azure Kinect DK SDK, which is licensed under the MIT License.
*/

#include "DepthMesh.hpp"

#include <core_logging.hpp>

namespace core {


/*
    // Only 73% of data is non-zero, so first compress it:

    Workspace.resize(n * 3 + kLanes * 3); // Pad to packet size
    float* workspace_next = Workspace.data();
    for (int i = 0; i < n; ++i)
    {
        const uint16_t depth_mm = depth[i];
        if (depth_mm == 0) {
            continue;
        }

        const k4a_float2_t scale = lookup[i];
        if (isnan(scale.xy.x)) {
            depth[i] = 0;
            continue;
        }

        // Convert to 3D (millimeters) relative to depth camera
        workspace_next[0] = depth_mm * scale.xy.x;
        workspace_next[1] = depth_mm * scale.xy.y;
        workspace_next[2] = depth_mm;
        workspace_next += 3;
    }
    uintptr_t workspace_size = static_cast<uintptr_t>( workspace_next - Workspace.data() );
    workspace_next = Workspace.data();

    coordinates.clear();
    coordinates.resize(n * 5 + kLanes * 5); // Pad to packet size
    float* coordinates_data = coordinates.data();

    Indices3 midx(0, 1, 2);
    const Matrix3f unif_R = enoki::gather<Matrix3f>(R, midx);
    const Vector3f unif_t = enoki::load_unaligned<Vector3f>(t);
    const IndicesP index = enoki::arange<IndicesP>();

    // For each workspace packet to process:
    for (unsigned i = 0; i < workspace_size; i += 3 * kLanes)
    {
        const Vector3fP input = enoki::gather<Vector3fP>(workspace_next + i, index);

        Vector3fP color_pos = unif_R * input + unif_t;

        FloatP inv_z = 1.f / color_pos.z();
        FloatP x_proj = color_pos.x() * inv_z;
        FloatP y_proj = color_pos.y() * inv_z;

        FloatP xp = x_proj - codx;
        FloatP yp = y_proj - cody;

        FloatP xp2 = xp * xp;
        FloatP yp2 = yp * yp;
        FloatP xyp = xp * yp;
        FloatP rs = xp2 + yp2;

        // If mask = 0, then we should invalidate this later
        //enoki::mask_t<FloatP> mask = (rs <= max_radius_for_projection * max_radius_for_projection);

        FloatP rss = rs * rs;
        FloatP rsc = rss * rs;
        FloatP a = 1.f + k1 * rs + k2 * rss + k3 * rsc;
        FloatP b = 1.f + k4 * rs + k5 * rss + k6 * rsc;
        FloatP bi = enoki::select(b != 0.f, 1.f / b, 1.f);
        FloatP d = a * bi;

        FloatP xp_d = xp * d;
        FloatP yp_d = yp * d;

        FloatP rs_2xp2 = rs + 2.f * xp2;
        FloatP rs_2yp2 = rs + 2.f * yp2;

        xp_d += rs_2xp2 * p2 + dist_coeff * xyp * p1;
        yp_d += rs_2yp2 * p1 + dist_coeff * xyp * p2;

        FloatP xp_d_cx = xp_d + codx;
        FloatP yp_d_cy = yp_d + cody;

        // Convert xyz to meters and normalized uv
        FloatP u = xp_d_cx * fx + cx;
        FloatP v = yp_d_cy * fy + cy;

        Vector5fP temp;
        temp.x() = color_pos.x() * kInverseMeters;
        temp.y() = color_pos.y() * kInverseMeters;
        temp.z() = color_pos.z() * kInverseMeters;
        temp[3] = u * inv_color_width;
        temp[4] = v * inv_color_height;

        enoki::scatter(coordinates_data, temp, index);
        coordinates_data += 5 * kLanes;
    } // next packet

    coordinates.resize((workspace_size / 3) * 5);
*/


//------------------------------------------------------------------------------
// Tools

static inline bool FloatsNotEqual(float a, float b, const float eps = 0.000001f)
{
    return std::fabs(a - b) > eps;
}

bool CameraIntrinsics::operator==(const CameraIntrinsics& rhs) const
{
    if (Width != rhs.Width || Height != rhs.Height || LensModel != rhs.LensModel) {
        return false;
    }

    if (FloatsNotEqual(cx, rhs.cx) || FloatsNotEqual(cy, rhs.cy) || FloatsNotEqual(fx, rhs.fx) || FloatsNotEqual(fy, rhs.fy)) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        if (FloatsNotEqual(k[i], rhs.k[i])) {
            return false;
        }
    }

    if (FloatsNotEqual(codx, rhs.codx) || FloatsNotEqual(cody, rhs.cody) || FloatsNotEqual(p1, rhs.p1) || FloatsNotEqual(p2, rhs.p2)) {
        return false;
    }

    return true;
}

bool CameraCalibration::operator==(const CameraCalibration& rhs) const
{
    if (Depth != rhs.Depth || Color != rhs.Color) {
        return false;
    }
    for (int i = 0; i < 9; ++i) {
        if (FloatsNotEqual(RotationFromDepth[i], rhs.RotationFromDepth[i])) {
            return false;
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (FloatsNotEqual(TranslationFromDepth[i], rhs.TranslationFromDepth[i])) {
            return false;
        }
    }

    return true;
}

bool ImageCropRegion::Grow(const ImageCropRegion& other)
{
    bool grown = false;
    if (CropX > other.CropX) {
        CropX = other.CropX;
        grown = true;
    }
    if (CropY > other.CropY) {
        CropY = other.CropY;
        grown = true;
    }
    const unsigned old_x_end = CropX + CropW;
    const unsigned new_x_end = other.CropX + other.CropW;
    if (old_x_end < new_x_end) {
        CropW = new_x_end - CropX;
        grown = true;
    }
    const unsigned old_y_end = CropY + CropH;
    const unsigned new_y_end = other.CropY + other.CropH;
    if (old_y_end < new_y_end) {
        CropH = new_y_end - CropY;
        grown = true;
    }
    return grown;
}

static bool ProjectInternal(
    const CameraIntrinsics& intrinsics,
    const float xy[2],
    float uv[2],
    float J_xy[2 * 2])
{
    const float cx = intrinsics.cx;
    const float cy = intrinsics.cy;
    const float fx = intrinsics.fx;
    const float fy = intrinsics.fy;
    const float k1 = intrinsics.k[0];
    const float k2 = intrinsics.k[1];
    const float k3 = intrinsics.k[2];
    const float k4 = intrinsics.k[3];
    const float k5 = intrinsics.k[4];
    const float k6 = intrinsics.k[5];
    const float codx = intrinsics.codx; // center of distortion is set to 0 for Brown Conrady model
    const float cody = intrinsics.cody;
    const float p1 = intrinsics.p1;
    const float p2 = intrinsics.p2;
    //const float max_radius_for_projection2 = intrinsics.MaxRadiusForProjection * intrinsics.MaxRadiusForProjection;

    float xp = xy[0] - codx;
    float yp = xy[1] - cody;

    float xp2 = xp * xp;
    float yp2 = yp * yp;
    float xyp = xp * yp;
    float rs = xp2 + yp2;
    //if (rs > max_radius_for_projection2) {
    //    return false;
    //}
    float rss = rs * rs;
    float rsc = rss * rs;
    float a = 1.f + k1 * rs + k2 * rss + k3 * rsc;
    float b = 1.f + k4 * rs + k5 * rss + k6 * rsc;
    float bi;
    if (b != 0.f) {
        bi = 1.f / b;
    }
    else {
        bi = 1.f;
    }
    float d = a * bi;

    float xp_d = xp * d;
    float yp_d = yp * d;

    float rs_2xp2 = rs + 2.f * xp2;
    float rs_2yp2 = rs + 2.f * yp2;

    float multiplier = 1.f;
    if (intrinsics.LensModel != LensModel_Rational_6KT) {
        // the only difference from Rational6ktCameraModel is 2 multiplier for the tangential coefficient term xyp*p1
        // and xyp*p2
        multiplier = 2.f;
    }

    xp_d += rs_2xp2 * p2 + multiplier * xyp * p1;
    yp_d += rs_2yp2 * p1 + multiplier * xyp * p2;

    float xp_d_cx = xp_d + codx;
    float yp_d_cy = yp_d + cody;

    uv[0] = xp_d_cx * fx + cx;
    uv[1] = yp_d_cy * fy + cy;

    // compute Jacobian matrix
    float dudrs = k1 + 2.f * k2 * rs + 3.f * k3 * rss;
    // compute d(b)/d(r^2)
    float dvdrs = k4 + 2.f * k5 * rs + 3.f * k6 * rss;
    float bis = bi * bi;
    float dddrs = (dudrs * b - a * dvdrs) * bis;

    float dddrs_2 = dddrs * 2.f;
    float xp_dddrs_2 = xp * dddrs_2;
    float yp_xp_dddrs_2 = yp * xp_dddrs_2;
    // compute d(u)/d(xp)
    J_xy[0] = fx * (d + xp * xp_dddrs_2 + 6.f * xp * p2 + multiplier * yp * p1);
    J_xy[1] = fx * (yp_xp_dddrs_2 + 2.f * yp * p2 + multiplier * xp * p1);
    J_xy[2] = fy * (yp_xp_dddrs_2 + 2.f * xp * p1 + multiplier * yp * p2);
    J_xy[3] = fy * (d + yp * yp * dddrs_2 + 6.f * yp * p1 + multiplier * xp * p2);

    return true;
}

static void Invert2x2(const float J[2 * 2], float Jinv[2 * 2])
{
    float detJ = J[0] * J[3] - J[1] * J[2];
    float inv_detJ = 1.f / detJ;

    Jinv[0] = inv_detJ * J[3];
    Jinv[3] = inv_detJ * J[0];
    Jinv[1] = -inv_detJ * J[1];
    Jinv[2] = -inv_detJ * J[2];
}

static bool IterativeUnproject(
    const CameraIntrinsics& intrinsics,
    const float* uv,
    float* xy,
    int max_passes = 20)
{
    float Jinv[2 * 2];
    float best_xy[2] = { 0.f, 0.f };
    float best_err = FLT_MAX;

    for (int pass = 0; pass < max_passes; pass++)
    {
        float p[2];
        float J[2 * 2];

        const bool valid = ProjectInternal(
            intrinsics,
            xy,
            p,
            J);
        if (!valid) {
            return false;
        }

        float err_x = uv[0] - p[0];
        float err_y = uv[1] - p[1];
        float err = err_x * err_x + err_y * err_y;
        if (err >= best_err) {
            xy[0] = best_xy[0];
            xy[1] = best_xy[1];
            break;
        }

        best_err = err;
        best_xy[0] = xy[0];
        best_xy[1] = xy[1];
        Invert2x2(J, Jinv);
        if (pass + 1 == max_passes || best_err < 1e-22f) {
            break;
        }

        float dx = Jinv[0] * err_x + Jinv[1] * err_y;
        float dy = Jinv[2] * err_x + Jinv[3] * err_y;

        xy[0] += dx;
        xy[1] += dy;
    }

    return best_err <= 1e-6f;
}

// Precompute the scale factor for each depth image pixel
static void PrecomputeScaleFactor2D(
    const CameraCalibration& calibration,
    const float uv[2],
    float xy[2])
{
    // Note that K4a data is all represented in millimeters.
    // Scale factor is relative to 1 mm.

    const CameraIntrinsics& intrinsics = calibration.Depth;

    float cx = intrinsics.cx;
    float cy = intrinsics.cy;
    float fx = intrinsics.fx;
    float fy = intrinsics.fy;
    float k1 = intrinsics.k[0];
    float k2 = intrinsics.k[1];
    float k3 = intrinsics.k[2];
    float k4 = intrinsics.k[3];
    float k5 = intrinsics.k[4];
    float k6 = intrinsics.k[5];
    float codx = intrinsics.codx; // center of distortion is set to 0 for Brown Conrady model
    float cody = intrinsics.cody;
    float p1 = intrinsics.p1;
    float p2 = intrinsics.p2;

    // correction for radial distortion
    float xp_d = (uv[0] - cx) / fx - codx;
    float yp_d = (uv[1] - cy) / fy - cody;

    float rs = xp_d * xp_d + yp_d * yp_d;
    float rss = rs * rs;
    float rsc = rss * rs;
    float a = 1.f + k1 * rs + k2 * rss + k3 * rsc;
    float b = 1.f + k4 * rs + k5 * rss + k6 * rsc;
    float di;
    if (a != 0.f) {
        di = b / a;
    }
    else {
        di = b;
    }

    xy[0] = xp_d * di;
    xy[1] = yp_d * di;

    // approximate correction for tangential params
    float two_xy = 2.f * xy[0] * xy[1];
    float xx = xy[0] * xy[0];
    float yy = xy[1] * xy[1];

    xy[0] -= (yy + 3.f * xx) * p2 + two_xy * p1;
    xy[1] -= (xx + 3.f * xx) * p1 + two_xy * p2;

    // add on center of distortion
    xy[0] += codx;
    xy[1] += cody;

    if (!IterativeUnproject(intrinsics, uv, xy)) {
        xy[0] = xy[1] = nanf("");
    }
}


//------------------------------------------------------------------------------
// DepthMesher

void DepthMesher::Initialize(const CameraCalibration& calibration)
{
    Calibration = calibration;

    const int width = Calibration.Depth.Width;
    const int height = Calibration.Depth.Height;

    // Precalculate affine factors for 2D -> 3D conversion:

    const int n = width * height;
    DepthLookup.resize(n * 2);
    float* lookup = DepthLookup.data();

    int invalids = 0;

    int index = 0;
    for (int y = 0; y < height; y++)
    {
        float uv[2];
        uv[1] = static_cast<float>( y );

        for (int x = 0; x < width; x++)
        {
            uv[0] = static_cast<float>( x );

            float xy[2];
            PrecomputeScaleFactor2D(calibration, uv, xy);

            if (std::isnan(xy[0])) {
                ++invalids;
            }

            lookup[index * 2] = xy[0];
            lookup[index * 2 + 1] = xy[1];
            ++index;
        }
    }

    if (invalids > 0) {
        spdlog::warn("Unexpected invalid projections {} during depth precomputation", invalids);
    }
}

void DepthMesher::GenerateCoordinates(
    uint16_t* depth,
    const ClipRegion* clip,
    //ImageCropRegion* crop,
    std::vector<float>& coordinates,
    bool face_painting_fix,
    bool cull_depth)
{
    const int width = Calibration.Depth.Width;
    const int height = Calibration.Depth.Height;
    const int n = width * height;

    const float* lookup = DepthLookup.data();

    const float kInverseMeters = 1.f / 1000.f;
    const float inv_color_width = 1.f / static_cast<float>( Calibration.Color.Width );
    const float inv_color_height = 1.f / static_cast<float>( Calibration.Color.Height );

    Eigen::Vector3f clip_p0, clip_d;
    if (clip)
    {
        // define pt2 as 1 meter from pt1

        Eigen::Matrix4f inv_exstrinsics = clip->Extrinsics.inverse();

        Eigen::Vector4f q0 = inv_exstrinsics * Eigen::Vector4f(0.f, 0.f, 0.f, 1.f);
        // TBD: We do not support skewed matrix
        clip_p0 = Eigen::Vector3f(q0(0), q0(1), q0(2));

        Eigen::Vector4f q1 = inv_exstrinsics * Eigen::Vector4f(0.f, 1.f, 0.f, 1.f);
        // TBD: We do not support skewed matrix
        clip_d = Eigen::Vector3f(q1(0), q1(1), q1(2)) - clip_p0;
    }

    // Extrinsics transform from depth -> color camera
    const float* R = Calibration.RotationFromDepth;
    const float* T = Calibration.TranslationFromDepth;

    const CameraIntrinsics& intrinsics = Calibration.Color;
    const float cx = intrinsics.cx;
    const float cy = intrinsics.cy;
    const float fx = intrinsics.fx;
    const float fy = intrinsics.fy;
    const float k1 = intrinsics.k[0];
    const float k2 = intrinsics.k[1];
    const float k3 = intrinsics.k[2];
    const float k4 = intrinsics.k[3];
    const float k5 = intrinsics.k[4];
    const float k6 = intrinsics.k[5];
    const float codx = intrinsics.codx; // center of distortion is set to 0 for Brown Conrady model
    const float cody = intrinsics.cody;
    const float p1 = intrinsics.p1;
    const float p2 = intrinsics.p2;
    //const float max_radius_for_projection2 = intrinsics.MaxRadiusForProjection * intrinsics.MaxRadiusForProjection;

    float dist_coeff = 1.f;
    if (intrinsics.LensModel != LensModel_Rational_6KT) {
        // the only difference from Rational6ktCameraModel is 2 multiplier for the tangential coefficient term xyp*p1
        // and xyp*p2
        dist_coeff = 2.f;
    }

    coordinates.clear();
    coordinates.resize(n * 5);
    float* coordinates_next = coordinates.data();

    //float u_min = Calibration.Color.Width;
    //float u_max = 0.f;
    //float v_min = Calibration.Color.Height;
    //float v_max = 0.f;

    int depth_row_offset = 0;
    for (int depth_y = 0; depth_y < height; ++depth_y, depth_row_offset += width)
    {
        // This is used to avoid painting foreground on background due to disocclusion.
        // The depth/RGB cameras are physically separated by a few mm distance, so the
        // depth camera can see things the RGB camera cannot - We must cull the depth
        // mesh where there is no texture information.  This is the reason we walk the
        // mesh from right to left.
        unsigned depth_limit = 65536;

        // This is tuned for NFOV2x2BINNED mode 320x288 mesh.
        // FIXME: Add support for other modes
        unsigned limit_increment = 40;

        for (int depth_x = width - 1; depth_x >= 0; --depth_x, depth_limit += limit_increment)
        {
            const int depth_index = depth_row_offset + depth_x;
            const uint16_t depth_mm = depth[depth_index];
            if (depth_mm == 0) {
                continue;
            }

            const float* scale = &lookup[depth_index * 2];
            CORE_DEBUG_ASSERT(!isnan(scale[0]));
#if 0
            if (isnan(scale[0])) {
                depth[depth_index] = 0;
                continue;
            }
#endif

            // 73% of data is non-zero:

            // Convert to 3D (millimeters) relative to depth camera
            const float depth_x_mm = depth_mm * scale[0];
            const float depth_y_mm = depth_mm * scale[1];
            const float depth_z_mm = depth_mm;

            // Convert to 3D relative to color camera
            const float color_x_mm = R[0] * depth_x_mm + R[1] * depth_y_mm + R[2] * depth_z_mm + T[0];
            const float color_y_mm = R[3] * depth_x_mm + R[4] * depth_y_mm + R[5] * depth_z_mm + T[1];
            const float color_z_mm = R[6] * depth_x_mm + R[7] * depth_y_mm + R[8] * depth_z_mm + T[2];

            const float x = color_x_mm * kInverseMeters;
            const float y = color_y_mm * kInverseMeters;
            const float z = color_z_mm * kInverseMeters;

            if (face_painting_fix)
            {
                if (depth_mm > depth_limit) {
                    if (cull_depth) {
                        depth[depth_index] = 0;
                    } else {
                        coordinates_next[0] = x;
                        coordinates_next[1] = y;
                        coordinates_next[2] = z;
                        coordinates_next[3] = 0.f;
                        coordinates_next[4] = 0.f;
                        coordinates_next += 5;
                    }
                    continue;
                } else {
                    depth_limit = depth_mm;
                    limit_increment = (depth_mm * 44) / 1000;
                }
            }

            // Cylinder clip:
            if (clip)
            {
                Eigen::Vector3f testpt(x, y, z);
                const Eigen::Vector3f pd = testpt - clip_p0;
                const float dot = -pd.dot(clip_d);
                if (dot < clip->Floor || dot > clip->Ceiling || (pd.squaredNorm() - dot*dot) > clip->Radius) {
                    if (cull_depth) {
                        depth[depth_index] = 0;
                    } else {
                        coordinates_next[0] = x;
                        coordinates_next[1] = y;
                        coordinates_next[2] = z;
                        coordinates_next[3] = 0.f;
                        coordinates_next[4] = 0.f;
                        coordinates_next += 5;
                    }
                    continue;
                }
            }

            const float inv_z = 1.f / color_z_mm;
            const float x_proj = color_x_mm * inv_z;
            const float y_proj = color_y_mm * inv_z;

            const float xp = x_proj - codx;
            const float yp = y_proj - cody;

            const float xp2 = xp * xp;
            const float yp2 = yp * yp;
            const float xyp = xp * yp;
            const float rs = xp2 + yp2;

#if 0
            if (rs > max_radius_for_projection2) {
                if (cull_depth) {
                    depth[depth_index] = 0;
                } else {
                    coordinates_next[0] = x;
                    coordinates_next[1] = y;
                    coordinates_next[2] = z;
                    coordinates_next[3] = 0.f;
                    coordinates_next[4] = 0.f;
                    coordinates_next += 5;
                }
                continue;
            }
#endif

            const float rss = rs * rs;
            const float rsc = rss * rs;
            const float a = 1.f + k1 * rs + k2 * rss + k3 * rsc;
            const float b = 1.f + k4 * rs + k5 * rss + k6 * rsc;
            float bi = 1.f;
            if (b != 0.f) {
                bi /= b;
            }
            const float d = a * bi;

            float xp_d = xp * d;
            float yp_d = yp * d;

            const float rs_2xp2 = rs + 2.f * xp2;
            const float rs_2yp2 = rs + 2.f * yp2;

            xp_d += rs_2xp2 * p2 + dist_coeff * xyp * p1;
            yp_d += rs_2yp2 * p1 + dist_coeff * xyp * p2;

            const float xp_d_cx = xp_d + codx;
            const float yp_d_cy = yp_d + cody;

            // Convert xyz to meters and normalized uv
            const float u_pixels = xp_d_cx * fx + cx;
            const float v_pixels = yp_d_cy * fy + cy;

    #if 0
            if (u_min > u_pixels) {
                u_min = u_pixels;
            }
            if (u_max < u_pixels) {
                u_max = u_pixels;
            }
            if (v_min > v_pixels) {
                v_min = v_pixels;
            }
            if (v_max < v_pixels) {
                v_max = v_pixels;
            }
    #endif

            const float u = u_pixels * inv_color_width;
            const float v = v_pixels * inv_color_height;

            // If it is sampling off the edge of the image:
            if (v < 0.0001f || v >= 1.0001f ||
                u < 0.0001f || u >= 1.0001f)
            {
                if (cull_depth) {
                    depth[depth_index] = 0;
                } else {
                    coordinates_next[0] = x;
                    coordinates_next[1] = y;
                    coordinates_next[2] = z;
                    coordinates_next[3] = 0.f;
                    coordinates_next[4] = 0.f;
                    coordinates_next += 5;
                }
                continue;
            }

            coordinates_next[0] = x;
            coordinates_next[1] = y;
            coordinates_next[2] = z;
            coordinates_next[3] = u;
            coordinates_next[4] = v;
            coordinates_next += 5;
        } // next x
    } // next y

    // Resize to fit
    const uintptr_t size = static_cast<uintptr_t>( coordinates_next - coordinates.data() );
    coordinates.resize(size);

#if 0
    if (crop)
    {
        const int fuzz = 16; // pixels
        int x_max = static_cast<int>(u_max) + fuzz;
        if (x_max > Calibration.Color.Width) {
            x_max = Calibration.Color.Width;
        }
        int x_min = static_cast<int>(u_min) - fuzz;
        if (x_min < 0) {
            x_min = 0;
        }
        int y_max = static_cast<int>(v_max) + fuzz;
        if (y_max > Calibration.Color.Height) {
            y_max = Calibration.Color.Height;
        }
        int y_min = static_cast<int>(v_min) - fuzz;
        if (y_min < 0) {
            y_min = 0;
        }

        // If the entire image is cropped:
        if (x_max <= x_min || y_max <= y_min)
        {
            // Show something near the center of the image
            x_min = Calibration.Color.Width / 2;
            y_min = Calibration.Color.Height / 2;
            x_max = x_min + 32;
            y_max = y_min + 32;
        }

        crop->CropX = static_cast<unsigned>( x_min );
        crop->CropY = static_cast<unsigned>( y_min );
        crop->CropW = static_cast<unsigned>( x_max - x_min );
        crop->CropH = static_cast<unsigned>( y_max - y_min );
    }
#endif
}

// Throw out triangles with too much depth mismatch
static bool CheckDepth(
    int a,
    int b,
    int c,
    int thresh_mm)
{
    if (abs(a - b) > thresh_mm) {
        return false;
    }
    if (abs(a - c) > thresh_mm) {
        return false;
    }
    if (abs(b - c) > thresh_mm) {
        return false;
    }
    return true;
}

void DepthMesher::GenerateTriangleIndices(
    const uint16_t* depth,
    std::vector<uint32_t>& indices)
{
    const int width = Calibration.Depth.Width;
    const int height = Calibration.Depth.Height;
    const int n = width * height;

    indices.clear();
    indices.resize(n * 2 * 3);
    unsigned* indices_next = indices.data();

    std::vector<unsigned> RowIndices(width * 2);
    unsigned* row_indices = RowIndices.data();

    unsigned index = 0;
    for (int y = 0; y < height; ++y, depth += width)
    {
        // Offset into row_indices for current and previous rows
        const unsigned current_row_offset = (y % 2 == 0) ? width : 0;
        const unsigned prev_row_offset = (y % 2 != 0) ? width : 0;

        // Unroll first loop
        if (y == 0) {
            for (int x = width - 1; x >= 0; --x) {
                if (depth[x] != 0) {
                    row_indices[x + current_row_offset] = index++;
                }
            }
            continue;
        }

        // For remaining rows:

        // Unroll last column:
        int x = width - 1;
        uint16_t depth_mm = depth[x];
        uint16_t d_depth = depth_mm;
        if (depth_mm != 0) {
            row_indices[x + current_row_offset] = index++;
        }

        for (--x; x >= 0; --x, d_depth = depth_mm)
        {
            depth_mm = depth[x];
            if (depth_mm == 0) {
                continue;
            }

            const unsigned c_index = index;
            row_indices[x + current_row_offset] = index++;

            /*
                We are at position C.  If A,B,D are available,
                then we construct triangles from them where possible,
                and these triangles will be unique and complete.

                    A -- B
                    |  / |
                    | /  |
                    C*-- D
            */

            const uint16_t a_depth = depth[x - width];
            const uint16_t b_depth = depth[x + 1 - width];

            // 10 mm threshold per 1 meter
            // This is hand-tuned based on the fact that the sensor has accuracy linear with depth.
            const int thresh_mm = depth_mm * 22 / 1000;

            if (b_depth != 0) {
                if (a_depth != 0 && CheckDepth(a_depth, b_depth, depth_mm, thresh_mm)) {
                    const unsigned a_index = row_indices[x + prev_row_offset];
                    const unsigned b_index = row_indices[x + prev_row_offset + 1];
                    indices_next[0] = c_index; // C
                    indices_next[1] = b_index; // B
                    indices_next[2] = a_index; // A
                    indices_next += 3;
                }
                if (b_depth != 0 && CheckDepth(b_depth, d_depth, depth_mm, thresh_mm)) {
                    const unsigned b_index = row_indices[x + prev_row_offset + 1];
                    indices_next[0] = c_index;     // C
                    indices_next[1] = c_index - 1; // D
                    indices_next[2] = b_index;     // B
                    indices_next += 3;
                }
            } else if (a_depth != 0 && d_depth != 0 &&
                CheckDepth(a_depth, d_depth, depth_mm, thresh_mm))
            {
                const unsigned a_index = row_indices[x + prev_row_offset];
                indices_next[0] = c_index;     // C
                indices_next[1] = c_index - 1; // D
                indices_next[2] = a_index;     // A
                indices_next += 3;
            } // end if
        } // next x
    } // next y

    // Resize to fit
    const uintptr_t size = static_cast<uintptr_t>( indices_next - indices.data() );
    indices.resize(size);
}

void DepthMesher::CalculateCrop(
    const ClipRegion& clip,
    ImageCropRegion& crop)
{
    // Evaluate u,v coordinates for vertex coordinates around ring
    // cross-sections of the crop cylinder.  The range of u,v becomes
    // the crop region for the video.

    const Eigen::Matrix4f inv_exstrinsics = clip.Extrinsics.inverse();

    const CameraIntrinsics& intrinsics = Calibration.Color;
    const float cx = intrinsics.cx;
    const float cy = intrinsics.cy;
    const float fx = intrinsics.fx;
    const float fy = intrinsics.fy;
    const float k1 = intrinsics.k[0];
    const float k2 = intrinsics.k[1];
    const float k3 = intrinsics.k[2];
    const float k4 = intrinsics.k[3];
    const float k5 = intrinsics.k[4];
    const float k6 = intrinsics.k[5];
    const float codx = intrinsics.codx; // center of distortion is set to 0 for Brown Conrady model
    const float cody = intrinsics.cody;
    const float p1 = intrinsics.p1;
    const float p2 = intrinsics.p2;
    //const float max_radius_for_projection2 = intrinsics.MaxRadiusForProjection * intrinsics.MaxRadiusForProjection;

    float dist_coeff = 1.f;
    if (intrinsics.LensModel != LensModel_Rational_6KT) {
        // the only difference from Rational6ktCameraModel is 2 multiplier for the tangential coefficient term xyp*p1
        // and xyp*p2
        dist_coeff = 2.f;
    }

    float u_max = 0.f;
    float u_min = static_cast<float>( Calibration.Color.Width );
    float v_max = 0.f;
    float v_min = static_cast<float>( Calibration.Color.Height );

    // For each slice of cylinder:
    const float y_iter = 0.2f;
    for (float y = clip.Floor; y < clip.Ceiling; y += y_iter)
    {
        // Parametric form of circle
        const float t_samples = 64.f;
        const float t_iter = 3.1415926535f * 2.f / t_samples;
        for (float t = -3.1415926535f; t < 3.1415926535f; t += t_iter)
        {
            float x = std::sinf(t) * clip.Radius;
            float z = std::cosf(t) * clip.Radius;

            Eigen::Vector4f q = inv_exstrinsics * Eigen::Vector4f(x, -y, z, 1.f);

            // Convert to u,v

            const float inv_z = 1.f / q(2);
            const float x_proj = q(0) * inv_z;
            const float y_proj = q(1) * inv_z;

            const float xp = x_proj - codx;
            const float yp = y_proj - cody;

            const float xp2 = xp * xp;
            const float yp2 = yp * yp;
            const float xyp = xp * yp;
            const float rs = xp2 + yp2;

            //if (rs > max_radius_for_projection2) {
            //    continue;
            //}

            const float rss = rs * rs;
            const float rsc = rss * rs;
            const float a = 1.f + k1 * rs + k2 * rss + k3 * rsc;
            const float b = 1.f + k4 * rs + k5 * rss + k6 * rsc;
            float bi = 1.f;
            if (b != 0.f) {
                bi /= b;
            }
            const float d = a * bi;

            float xp_d = xp * d;
            float yp_d = yp * d;

            const float rs_2xp2 = rs + 2.f * xp2;
            const float rs_2yp2 = rs + 2.f * yp2;

            xp_d += rs_2xp2 * p2 + dist_coeff * xyp * p1;
            yp_d += rs_2yp2 * p1 + dist_coeff * xyp * p2;

            const float xp_d_cx = xp_d + codx;
            const float yp_d_cy = yp_d + cody;

            const float u_pixels = xp_d_cx * fx + cx;
            const float v_pixels = yp_d_cy * fy + cy;

            if (u_max < u_pixels) {
                u_max = u_pixels;
            }
            if (v_max < v_pixels) {
                v_max = v_pixels;
            }
            if (u_min > u_pixels) {
                u_min = u_pixels;
            }
            if (v_min > v_pixels) {
                v_min = v_pixels;
            }
        }
    }

    const int fuzz = 4; // pixels
    int x_max = static_cast<int>(u_max) + fuzz;
    if (x_max > Calibration.Color.Width) {
        x_max = Calibration.Color.Width;
    }
    int x_min = static_cast<int>(u_min) - fuzz;
    if (x_min < 0) {
        x_min = 0;
    }
    int y_max = static_cast<int>(v_max) + fuzz;
    if (y_max > Calibration.Color.Height) {
        y_max = Calibration.Color.Height;
    }
    int y_min = static_cast<int>(v_min) - fuzz;
    if (y_min < 0) {
        y_min = 0;
    }

    // If crop would be empty:
    if (x_max <= x_min || y_max <= y_min) {
        x_min = Calibration.Color.Width / 2;
        y_min = Calibration.Color.Height / 2;
        x_max = x_min + 32;
        y_max = y_min + 32;
    }

    crop.CropX = static_cast<unsigned>( x_min );
    crop.CropY = static_cast<unsigned>( y_min );
    crop.CropW = static_cast<unsigned>( x_max - x_min );
    crop.CropH = static_cast<unsigned>( y_max - y_min );
}


//------------------------------------------------------------------------------
// TemporalDepthFilter

void TemporalDepthFilter::Filter(uint16_t* depth, int w, int h)
{
    const int n = w * h;
    if (Width != w || Height != h) {
        Width = w;
        Height = h;
        Count = 0;
        Index = 0;
        History.resize(kStride * n);
    }

    uint16_t* history = History.data();
    const int index = Index;

    // If history is still filling:
    if (Count < kStride)
    {
        ++Count;
        history += index;
        for (int i = 0; i < n; ++i) {
            history[i * kStride] = depth[i];
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const uint16_t x = depth[i];
            uint16_t* hist = history + i * kStride;

            unsigned sum = x;
            unsigned nonzero_count = sum != 0;
            unsigned h_min = sum;
            unsigned h_max = sum;
            for (int j = 0; j < kStride; ++j) {
                const uint16_t y = hist[j];
                if (y == 0) {
                    continue;
                }
                sum += y;
                ++nonzero_count;
                if (h_max < y) {
                    h_max = y;
                }
                if (h_min > y) {
                    h_min = y;
                }
            }

            // Shortcut for all zeroes:
            if (nonzero_count <= 0) {
                continue;
            }

            hist[index] = x;

            if (nonzero_count < kStride / 2) {
                continue;
            }

            const unsigned h_avg = sum / nonzero_count;
            const unsigned range = h_max - h_min;

            // Static objects are identified by max-min < 0.4% of avg range
            const unsigned uncertainty = h_avg / 256;

            // If the depth value is static:
            if (range < uncertainty) {
                depth[i] = static_cast<uint16_t>( h_avg );
            }
            // Otherwise allow the deviation through
        }
    }

    ++Index;
    if (Index >= kStride) {
        Index = 0;
    }
}


//------------------------------------------------------------------------------
// DepthEdgeFilter

void DepthEdgeFilter::Filter(uint16_t* depth, int w, int h)
{
    const int end_y = h - 1;
    const int end_x = w - 1;

    uint16_t* prior_row = depth;
    uint16_t* row = depth + w;
    uint16_t* next_row = depth + w * 2;

    // First step: Filter gradients beyond some limit, and build integral image

    const unsigned T = 200;

    const int ii_w = w + 1;
    const int ii_h = h + 1;
    IntegralImage.resize(ii_w * ii_h);
    uint16_t* ii_row = IntegralImage.data();

    // First row of integral image is all zeroes
    for (int ii_x = 0; ii_x < ii_w; ++ii_x) {
        ii_row[ii_x] = 0;
    }
    ii_row += ii_w;

    // Second row of integral image is unrolled
    {
        ii_row[0] = 0;
        ++ii_row;

        uint16_t row_sum = prior_row[0] != 0 ? 1 : 0;
        ii_row[0] = row_sum + ii_row[0 - ii_w];

        for (int x = 1; x < w; ++x) {
            row_sum += prior_row[x] != 0 ? 1 : 0;
            ii_row[x] = row_sum + ii_row[x - ii_w];
        }
        ii_row += w; // Note: Incremented by one above
    }

    for (int y = 1; y < end_y; ++y)
    {
        uint16_t left, current, right;

        left = row[0];
        current = row[1];

        uint16_t row_sum;

        // Unroll first column
        {
            ii_row[0] = 0;
            ii_row++;

            row_sum = left != 0 ? 1 : 0;
            ii_row[0] = row_sum + ii_row[0 - ii_w];
        }

        int x;
        for (x = 1; x < end_x; ++x)
        {
            // Access this here to avoid reading off the right side
            right = row[x + 1];

            if (current != 0) {
                // Remove foreground in the case of a level transition
                if (left != 0 && current > left + T) {
                    row[x] = 0;
                    current = 0;
                }
                else if (right != 0 && current > right + T) {
                    row[x] = 0;
                    current = 0;
                }
                else
                {
                    const uint16_t up = prior_row[x];
                    if (up != 0 && current > up + T) {
                        row[x] = 0;
                        current = 0;
                    } else {
                        const uint16_t down = next_row[x];
                        if (down != 0 && current > down + T) {
                            row[x] = 0;
                            current = 0;
                        }
                    }
                }
            }

            row_sum += current != 0 ? 1 : 0;
            ii_row[x] = row_sum + ii_row[x - ii_w];

            left = current;
            current = right;
        }

        // Unroll last column
        {
            row_sum += current != 0 ? 1 : 0;
            ii_row[x] = row_sum + ii_row[x - ii_w];
        }

        prior_row = row;
        row = next_row;
        next_row += w;

        ii_row += w; // Note: Already incremented by 1 above
    }

    // Unroll last row
    {
        ii_row[0] = 0;
        ++ii_row;

        uint16_t row_sum = row[0] != 0 ? 1 : 0;
        ii_row[0] = row_sum + ii_row[0 - ii_w];

        for (int x = 1; x < w; ++x) {
            row_sum += row[x] != 0 ? 1 : 0;
            ii_row[x] = row_sum + ii_row[x - ii_w];
        }
    }

    // Second step: Eliminate edges by referencing the integral image

    const uint16_t* ii_above = IntegralImage.data() + 1;
    const uint16_t* ii_below = ii_above + ii_w * 3;

    row = depth + w;

    for (int y = 1; y < end_y; ++y)
    {
        for (int x = 1; x < end_x; ++x)
        {
            const uint16_t d = row[x];
            if (d == 0) {
                continue;
            }

            const uint16_t ul = ii_above[x - 2];
            const uint16_t ur = ii_above[x + 1];
            const uint16_t ll = ii_below[x - 2];
            const uint16_t lr = ii_below[x + 1];

            const uint16_t neighbor_sum = ul + lr - ur - ll;

            // Not well connected enough:
            if (neighbor_sum < 7) {
                row[x] = 0;
            }
        }

        ii_above += ii_w;
        ii_below += ii_w;

        row += w;
    }
}


} // namespace core
