// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "CameraExtrinsics.hpp"

#include <core_logging.hpp>

#include <sstream>
#include <vector>
#include <future>
#include <cmath>

#include <Open3D/Geometry/PointCloud.h>
#include <Open3D/Registration/FastGlobalRegistration.h>
#include <Open3D/Registration/Feature.h>
#include <Open3D/Registration/Registration.h>
#include <Open3D/Registration/ColoredICP.h>
#include <Open3D/Utility/Console.h>

#include <apriltag.h>
#include <tagStandard41h12.h>
#include <apriltag_pose.h>

namespace core {


//------------------------------------------------------------------------------
// Registration

static bool GenerateCloudFromVertices(
    VerticesInfo vertices,
    std::shared_ptr<open3d::geometry::PointCloud>& cloud,
    std::shared_ptr<open3d::registration::Feature>& feature)
{
    if (!vertices.XyzuvVertices || vertices.FloatsCount <= 0) {
        return false;
    }

    std::shared_ptr<open3d::geometry::PointCloud> full_cloud = std::make_shared<open3d::geometry::PointCloud>();

    const int count = vertices.FloatsCount;
    const float* coords = vertices.XyzuvVertices;
    const int coord_stride = 5;
    full_cloud->points_.reserve(count / coord_stride);

    for (int i = 0; i < count; i += coord_stride) {
        Eigen::Vector3d q(coords[i], coords[i + 1], coords[i + 2]);
        full_cloud->points_.push_back(q);

        // FIXME: Calculate the overlap between cloud_0 and cloud_i and color only the shared pixels.
        Eigen::Vector3d color{};
#if 0
        color = ReadIlluminationInvariantNV12Color(
            vertices.Y,
            vertices.UV,
            vertices.Width,
            vertices.Height,
            coords[i + 3],
            coords[i + 4]));
#endif
        full_cloud->colors_.push_back(color);
    }

    const double voxel_size = 0.01;

    // Downsample the point cloud
    cloud = full_cloud->VoxelDownSample(voxel_size);
    if (!cloud) {
        spdlog::error("VoxelDownSample failed");
        return false;
    }

    // Estimate normals with full resolution point cloud
    const double normal_radius = voxel_size * 2.0;
    open3d::geometry::KDTreeSearchParamHybrid normals_params(normal_radius, 30);
    const bool fast_normal_computation = false;
    bool success = cloud->EstimateNormals(normals_params, fast_normal_computation);
    if (!success) {
        spdlog::error("EstimateNormals failed");
        return false;
    }

    // Incorporate the assumption that normals should be pointed towards the camera
    success = cloud->OrientNormalsTowardsCameraLocation(Eigen::Vector3d(0,0,0));
    if (!success) {
        spdlog::error("OrientNormalsTowardsCameraLocation failed");
        return false;
    }

    // Generate cloud features
    const double feature_radius = voxel_size * 5.0;
    const open3d::geometry::KDTreeSearchParamHybrid features_params(feature_radius, 100);
    feature = open3d::registration::ComputeFPFHFeature(*cloud, features_params);
    if (!feature) {
        spdlog::error("ComputeFPFHFeature failed for i=0");
        return false;
    }

    return true;
}

bool CalculateExtrinsics(
    const std::vector<VerticesInfo>& vertices,
    std::vector<AlignmentTransform>& output)
{
    output.clear();
    if (vertices.empty()) {
        spdlog::warn("No images provided to registration");
        return false;
    }
    open3d::utility::SetVerbosityLevel(open3d::utility::VerbosityLevel::Debug);

    const uint64_t t0 = GetTimeUsec();

    const int camera_count = static_cast<int>( vertices.size() );
    output.resize(camera_count);

    // Estimate camera poses from April tag:

    apriltag_family_t *tf = tagStandard41h12_create();
    ScopedFunction tf_scope([tf]() {
        tagStandard41h12_destroy(tf);
    });

    apriltag_detector_t* td = apriltag_detector_create();
    ScopedFunction td_scope([td]() {
        apriltag_detector_destroy(td);
    });

    apriltag_detector_add_family_bits(td, tf, 1);
    td->quad_decimate = 1.f;
    td->quad_sigma = 0.8f;
    td->nthreads = 1;
    td->refine_edges = 1;
    td->decode_sharpening = 0.25;

    std::vector<Eigen::Matrix4f> tag_poses(camera_count);

    for (int camera_index = 0; camera_index < camera_count; ++camera_index)
    {
        image_u8_t orig {
            vertices[camera_index].Width,
            vertices[camera_index].Height,
            vertices[camera_index].Width,
            vertices[camera_index].Y
        };
        zarray_t* detections = apriltag_detector_detect(td, &orig);

        spdlog::info("Detected {} fiducial markers", zarray_size(detections));

        bool found = false;
        for (int i = 0; i < zarray_size(detections); i++)
        {
            apriltag_detection_t *det;
            zarray_get(detections, i, &det);

            CameraCalibration* calibration = vertices[camera_index].Calibration;

            if (det->id != 0) {
                spdlog::warn("Camera {} detected incorrect marker #{}", camera_index, det->id);
                continue;
            }

            spdlog::info("Camera {} detected marker: {}", camera_index, det->id);

            spdlog::info("cx={} cy={} fx={} fy={}", calibration->Color.cx, calibration->Color.cy, calibration->Color.fx, calibration->Color.fy);

            apriltag_detection_info_t info;
            info.det = det;
            info.cx = calibration->Color.cx; // pixels
            info.cy = calibration->Color.cy;
            info.fx = calibration->Color.fx; // mm
            info.fy = calibration->Color.fy;
            info.tagsize = 0.118f; // 8 inches on a side, in meters

            apriltag_pose_t pose;
            double err = estimate_tag_pose(&info, &pose);

            const double* tr = &pose.R->data[0];
            const double* tt = &pose.t->data[0];

            spdlog::info("Object-space error = {}", err);
            spdlog::info("R = [ {}, {}, {} \\", tr[0], tr[1], tr[2]);
            spdlog::info("      {}, {}, {} \\", tr[3], tr[4], tr[5]);
            spdlog::info("      {}, {}, {} ]", tr[6], tr[7], tr[8]);
            spdlog::info("t = [ {}, {}, {} ]", tt[0], tt[1], tt[2]);

            Eigen::Matrix4f transform;
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    transform(row, col) = static_cast<float>( tr[row * 3 + col] );
                }
            }
            for (int row = 0; row < 3; ++row) {
                transform(row, 3) = static_cast<float>( tt[row] );
            }
            for (int col = 0; col < 3; ++col) {
                transform(3, col) = 0.f;
            }
            transform(3, 3) = 1.f;
            tag_poses[camera_index] = transform;

            found = true;
        }

        if (!found) {
            spdlog::error("Camera {} did not observe the fiducial marker - Waiting for the next frame", camera_index);
            return false;
        }
    }

    spdlog::info("All cameras observed the fiducial marker");

    // Calculate scene yaw relative to marker:

#ifndef M_PI_FLOAT
# define M_PI_FLOAT 3.14159265f
#endif

    auto& pose0 = tag_poses[0];
    Eigen::Vector3f euler0 = pose0.block<3, 3>(0, 0).eulerAngles(0, 1, 2);
    float yaw = euler0.z();
    spdlog::info("Detected marker yaw = {} degrees", yaw * 180.f / M_PI_FLOAT);
    Eigen::AngleAxis<float> yaw_rot(-yaw, Eigen::Vector3f(0.f, 1.f, 0.f));
    Eigen::Matrix4f yaw_transform = Eigen::Affine3f(yaw_rot).matrix();

    // Center scene on marker:

    Eigen::Vector3f marker_offset_0(pose0(0, 3), pose0(1, 3), pose0(2, 3));

    // Correct camera tilt based on accelerometer:

    Eigen::Matrix4f tilt_transform = Eigen::Matrix4f::Identity();

    // Use first camera as reference
    auto& accel = vertices[0].Accelerometer;
    if (accel[0] == 0.f && accel[1] == 0.f && accel[2] == 0.f) {
        spdlog::error("IMU acceleration reading not available for tilt correction");
    }
    else
    {
        spdlog::info("Correcting tilt of primary camera using gravity down-vector [ {}, {}, {} ]", accel[0], accel[1], accel[2]);

        // Accelerometer frame: (x, y, z) = (+forward, +right, +up)
        // Pointcloud frame:    (x, y, z) = (+right,   -up,    +forward)
        Eigen::Quaternionf q;
        q.setFromTwoVectors(
            Eigen::Vector3f(accel[1], accel[2], accel[0]),
            Eigen::Vector3f(0.f, -1.f, 0.f));
        Eigen::Matrix3f tilt_r = q.toRotationMatrix();
        tilt_transform.block<3, 3>(0, 0) = tilt_r.transpose();

        marker_offset_0 = tilt_r.inverse() * marker_offset_0;
    }

    Eigen::Translation3f translation(-marker_offset_0);
    Eigen::Matrix4f translation_transform = Eigen::Affine3f(translation).matrix();

    const Eigen::Matrix4f center_transform = yaw_transform * translation_transform * tilt_transform;

    spdlog::info("===========================================================");
    spdlog::info("!!! Starting extrinsics calibration for {} cameras...", camera_count);

    output[0] = center_transform;

    std::shared_ptr<open3d::geometry::PointCloud> cloud_0;
    std::shared_ptr<open3d::registration::Feature> feature_0;
    if (!GenerateCloudFromVertices(vertices[0], cloud_0, feature_0)) {
        spdlog::error("GenerateCloudFromVertices failed for i=0");
        return false;
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("===========================================================");
    spdlog::info("Generated cloud 0 in {} msec", (t1 - t0) / 1000.f);

    for (int camera_index = 1; camera_index < camera_count; ++camera_index)
    {
        const uint64_t t2 = GetTimeUsec();

        std::shared_ptr<open3d::geometry::PointCloud> cloud_i;
        std::shared_ptr<open3d::registration::Feature> feature_i;
        if (!GenerateCloudFromVertices(vertices[camera_index], cloud_i, feature_i)) {
            spdlog::error("GenerateCloudFromVertices failed for i={}", camera_index);
            return false;
        }

        const uint64_t t3 = GetTimeUsec();
        spdlog::info("===========================================================");
        spdlog::info("Generated cloud {} in {} msec", camera_index, (t3 - t2) / 1000.f);

#if 0
        // Some notes are at the bottom of this README:
        // https://github.com/mylxiaoyi/FastGlobalRegistration
        open3d::registration::FastGlobalRegistrationOption fgr_option{};
        fgr_option.use_absolute_scale_ = true;
        fgr_option.decrease_mu_ = true;
        fgr_option.division_factor_ = 1.4;
        fgr_option.maximum_correspondence_distance_ = 0.05f; // meters
        fgr_option.iteration_number_ = 128;
        fgr_option.tuple_scale_ = 0.95;
        fgr_option.maximum_tuple_count_ = 3000;

        auto result = open3d::registration::FastGlobalRegistration(
            *cloud_i,
            *cloud_0,
            *feature_i,
            *feature_0,
            fgr_option);

        const uint64_t t4 = GetTimeUsec();
        spdlog::info("===========================================================");
        spdlog::info("Global registration for {} -> 0 in {} msec", i, (t4 - t3) / 1000.f);
#endif

        const Eigen::Matrix4f tag_pose = tag_poses[0] * tag_poses[camera_index].inverse();
        const Eigen::Matrix4d initial_transform = tag_pose.cast<double>();

        for (int j = 0; j < 4; ++j)
        {
            spdlog::info("{} {}, {}, {}, {}{}",
                j == 0 ? "initial_transform = [" : " ",
                tag_pose(j, 0),
                tag_pose(j, 1),
                tag_pose(j, 2),
                tag_pose(j, 3),
                j == 3 ? " ]" : ",");
        }

        const double max_distance = 0.03; // meters
        open3d::registration::ICPConvergenceCriteria criteria(1e-16, 1e-16, 500);

        // How much it tends towards using the geometry instead of the color
        const double lambda_geometric = 0.97;

        auto result = open3d::registration::RegistrationColoredICP(
            *cloud_i,
            *cloud_0,
            max_distance,
            initial_transform,
            lambda_geometric);

        const uint64_t t5 = GetTimeUsec();
        spdlog::info("===========================================================");
        spdlog::info("Color ICP refinement for {} -> 0 in {} msec", camera_index, (t5 - t3) / 1000.f);

        auto transform4x4 = result.transformation_.cast<float>();

        for (int j = 0; j < 4; ++j)
        {
            spdlog::info("{} {}, {}, {}, {}{}",
                j == 0 ? "transform = [" : " ",
                transform4x4(j, 0),
                transform4x4(j, 1),
                transform4x4(j, 2),
                transform4x4(j, 3),
                j == 3 ? " ]" : ",");
        }

        output[camera_index] = center_transform * transform4x4;
    } // next image

    const uint64_t t6 = GetTimeUsec();
    spdlog::info("===========================================================");
    spdlog::info("Full registration in {} msec", (t6 - t0) / 1000.f);

    return true;
}

bool RefineExtrinsics(
    const std::vector<VerticesInfo>& vertices,
    std::vector<AlignmentTransform>& extrinsics)
{
    if (extrinsics.size() != vertices.size()) {
        spdlog::error("Invalid input");
        return false;
    }

    const int camera_count = static_cast<int>( vertices.size() );

    spdlog::info("===========================================================");
    spdlog::info("!!! Starting extrinsics calibration for {} cameras...", camera_count);

    Eigen::Matrix4f center_transform;
    extrinsics[0].Set(center_transform);

    Eigen::Matrix4f inv_center_transform = center_transform.inverse();

    const uint64_t t0 = GetTimeUsec();

    std::shared_ptr<open3d::geometry::PointCloud> cloud_0;
    std::shared_ptr<open3d::registration::Feature> feature_0;
    if (!GenerateCloudFromVertices(vertices[0], cloud_0, feature_0)) {
        spdlog::error("GenerateCloudFromVertices failed for i=0");
        return false;
    }

    const uint64_t t1 = GetTimeUsec();
    spdlog::info("===========================================================");
    spdlog::info("Generated cloud 0 in {} msec", (t1 - t0) / 1000.f);

    for (int camera_index = 1; camera_index < camera_count; ++camera_index)
    {
        const uint64_t t2 = GetTimeUsec();

        std::shared_ptr<open3d::geometry::PointCloud> cloud_i;
        std::shared_ptr<open3d::registration::Feature> feature_i;
        if (!GenerateCloudFromVertices(vertices[camera_index], cloud_i, feature_i)) {
            spdlog::error("GenerateCloudFromVertices failed for i={}", camera_index);
            return false;
        }

        const uint64_t t3 = GetTimeUsec();
        spdlog::info("===========================================================");
        spdlog::info("Generated cloud {} in {} msec", camera_index, (t3 - t2) / 1000.f);

        const double max_distance = 0.02; // meters
        open3d::registration::ICPConvergenceCriteria criteria(1e-16, 1e-16, 500);

        // How much it tends towards using the geometry instead of the color
        const double lambda_geometric = 1.0;

        Eigen::Matrix4f transform_i;
        extrinsics[camera_index].Set(transform_i);

        // Left multiply to undo the "center transform" from full registration,
        // leaving just the prior transform from cloud_i to cloud_0
        Eigen::Matrix4f initial_transform_f = inv_center_transform * transform_i;
        Eigen::Matrix4d initial_transform_d = initial_transform_f.cast<double>();

        auto result = open3d::registration::RegistrationColoredICP(
            *cloud_i,
            *cloud_0,
            max_distance,
            initial_transform_d,
            lambda_geometric);

        const uint64_t t5 = GetTimeUsec();
        spdlog::info("===========================================================");
        spdlog::info("Color ICP refinement for {} -> 0 in {} msec", camera_index, (t5 - t3) / 1000.f);

        auto transform4x4 = result.transformation_.cast<float>();

        for (int j = 0; j < 4; ++j)
        {
            spdlog::info("{} {}, {}, {}, {}{}",
                j == 0 ? "transform = [" : " ",
                transform4x4(j, 0),
                transform4x4(j, 1),
                transform4x4(j, 2),
                transform4x4(j, 3),
                j == 3 ? " ]" : ",");
        }

        extrinsics[camera_index] = center_transform * transform4x4;
    } // next image

    const uint64_t t6 = GetTimeUsec();
    spdlog::info("===========================================================");
    spdlog::info("Registration refinement in {} msec", (t6 - t0) / 1000.f);

    return true;
}


} // namespace core
