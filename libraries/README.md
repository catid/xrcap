# Libraries

## capture

Thread-safe Azure Kinect multi-camera capture library supporting tight synchronization, video compression, and other stages in the pipeline.
Produces batches of video data to transmit or store via a callback.

## capture_protocol

Network protocol definitions shared between client/server applications.

## core

Common system software shared by all other libraries:
+ Logging
+ Memory-mapped files
+ Video parsers
+ Serialization

## depth_mesh

Provides depth mesh-related tools:
+ API agnostic: Can be trivially extended to work for non-Azure Kinect sensors also.
+ Multi-camera extrinsics calculation.
+ Multi-camera color normalization.
+ Filter that removes rough edges from the mesh where data tends to be worse.
+ Temporal depth filter that helps to keep static scene meshes from wobbling, without affecting moving objects.
+ Mesh tool to unpacked 2D depth map into triangles that can be rendered.

## depthengine

Redistribution of the Azure Kinect DLL, so the Azure Kinect SDK is not required

## glad

OpenGL graphics rendering tools built on top of GLAD

## tonk

Tonk peer2peer network library
