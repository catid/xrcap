# XRcap 3D Video File Format (.xrcap)

This binary format describes a simple RGB + depth video format that can be used for high compression ratio raw volumetric recordings without reprocessing into a mesh.

The purpose of the format is to allow for real-time raw recording of sensor data, live preview,
and live streaming via MPEG-DASH type approaches.

## Chunk Framing Format

The file is built up of chunks with a common framing format:

```
    <Length(32 bits, unsigned)>
    <Type(32 bits, unsigned)>
    <Data(Variable)>
```

The first 8 bytes is the chunk header.  The fields are stored in little-endian byte order.

Length is the number of bytes in the `Data` field.

Type is one of the chunk types defined below.

## Chunks

Chunk types sent infrequently:

+ 0 = Calibration
+ 1 = Extrinsics
+ 2 = Video Info

Chunk types sent for each frame:

+ 3 = Batch Info (Start of a set of frames from multiple cameras)
+ 4 = Frame (Keyframe or P-frame from a single camera, indicates last frame in a set)

C++ structures for these chunk types are defined in `FileFormat.hpp`.

## Chunk 0: Calibration

This provides updated intrinsics for each camera.  This is only expected to change if capture is restarted during recording.

Each camera is uniquely identified by a pair of numbers: the server GUID, and the camera index.  The GUID is a random number assigned for each capture server, and the index is a number starting from 0 and incrementing by one for each camera attached to that capture server.  This pair of numbers is referenced in other chunks.

```
    <ServerGuid(64 bits, unsigned)>
    <CameraIndex(32 bits, unsigned)>

    Intrinsics repeated 2x for Color and Depth:

        <Width(32 bits, signed)>
        <Height(32 bits, signed)>
        <LensModel(32 bits, unsigned)>

        <cx(32 bits, floating-point)>
        <cy(32 bits, floating-point)>
        <fx(32 bits, floating-point)>
        <fy(32 bits, floating-point)>
        <k1(32 bits, floating-point)>
        <k2(32 bits, floating-point)>
        <k3(32 bits, floating-point)>
        <k4(32 bits, floating-point)>
        <k5(32 bits, floating-point)>
        <k6(32 bits, floating-point)>
        <codx(32 bits, floating-point)>
        <cody(32 bits, floating-point)>
        <p1(32 bits, floating-point)>
        <p2(32 bits, floating-point)>

    3x3 Rotation Matrix from Depth to Color (row-first):

        <3x3 Rotation Matrix 0(32 bits, floating-point)>
        <3x3 Rotation Matrix 1(32 bits, floating-point)>
        ...
        <3x3 Rotation Matrix 7(32 bits, floating-point)>
        <3x3 Rotation Matrix 8(32 bits, floating-point)>

    Translation Vector from Depth to Color:

        <Translation X(32 bits, floating-point)>
        <Translation Y(32 bits, floating-point)>
        <Translation Z(32 bits, floating-point)>
```

Intrinsics are provided for color and depth cameras.
The transfrom from the depth camera to the color camera is provided.
To apply the transform on depth point P to color point Q:

```
    Q(x,y,z) = P(x, y, z) * R + T
```

Lens Models:

```
    0 = Unknown
    1 = Theta
    2 = Polynomial 3K
    3 = Rational 6KT
    4 = Brown Conrady (common)
```

The intrinsics are used during playback to generate a 3D point relative to the color camera, triangle indices and uv coordinates.

## Chunk 1: Extrinsics

This provides updated extrinsics for each camera.  This is only expected to change if recalibration occurs during recording.

Each camera is uniquely identified by a pair of numbers: the server GUID, and the camera index.  The GUID is a random number assigned for each capture server, and the index is a number starting from 0 and incrementing by one for each camera attached to that capture server.  This pair of numbers is referenced in other chunks.

```
    <ServerGuid(64 bits, unsigned)>
    <CameraIndex(32 bits, unsigned)>

    3x3 Rotation Matrix (row-first):

        <3x3 Rotation Matrix 0(32 bits, floating-point)>
        <3x3 Rotation Matrix 1(32 bits, floating-point)>
        ...
        <3x3 Rotation Matrix 7(32 bits, floating-point)>
        <3x3 Rotation Matrix 8(32 bits, floating-point)>

    Translation Vector:

        <Translation X(32 bits, floating-point)>
        <Translation Y(32 bits, floating-point)>
        <Translation Z(32 bits, floating-point)>
```

After applying the intrinsics to generate a 3D point relative to the color camera, triangle indices and uv coordinates, this extrinsics transform orients the mesh so that meshes from multiple cameras are aligned.

To apply the transform on mesh point P to color point Q:

```
    Q(x,y,z) = P(x, y, z) * R + T
```

This matrix multiplication is expected to be performed inside the graphics shader rather than on the CPU, with the matrix expanded to a 4x4 transform and provided as a uniform.

## Chunk 2: Video Info

This provides parameters for the color video stream that are needed for decoding.

```
    <ServerGuid(64 bits, unsigned)>
    <CameraIndex(32 bits, unsigned)>

    <VideoType(32 bits, unsigned)>
    <Width(32 bits, unsigned)>
    <Height(32 bits, unsigned)>
    <Framerate(32 bits, unsigned)>
    <Bitrate(32 bits, unsigned)>
```

Video types:

```
    0 = Lossless (Not implemented)
    1 = H.264
    2 = H.265
```

The other fields thank video type are purely informational and may be incorrect.  The source of truth is inside the coded video data itself in the VPS, SPS, PPS parameter sets.

## Chunk 3: Batch Info

This provides metadata for a batch of camera frames.  It indicates the start of a new multi-camera mesh for render.

```
    <MaxCameraCount(32 bits, unsigned)>
    <VideoUsec(64 bits, unsigned)>
    <VideoEpochUsec(64 bits, unsigned)>
```

The `MaxCameraCount` is the maximum number of frames that will be sent as part of the batch.  The actual number of frames may be between zero and this number.  For example if a camera failed to return a frame on the capture server then it will not be available in the recording.

The `FrameNumber` is a number that increments by one for each frame that is in the recording.

The `VideoUsec` field is a monotonic microsecond timestamp on the video frame used for the presentation timestamp of the video frame.

The `VideoEpochUsec` field is the best estimate of the middle of exposure time for all the color camera frames in the batch and is useful for synchronizing the video with other data streams.

## Chunk 4: Frame

This provides the compressed color and depth data from a single perspective in the multi-camera rig.

```
    <IsFinalFrame(8 bits, boolean)>

    <ServerGuid(64 bits, unsigned)>
    <CameraIndex(32 bits, unsigned)>

    <FrameNumber(32 bits, unsigned)>
    <BackReference(32 bits, signed)>

    <ImageBytes(32 bits, unsigned)>
    <DepthBytes(32 bits, unsigned)>

    <Accelerometer X(32 bits, floating-point)>
    <Accelerometer Y(32 bits, floating-point)>
    <Accelerometer Z(32 bits, floating-point)>
    <ExposureUsec(32 bits, unsigned)>
    <AutoWhiteBalanceUsec(32 bits, unsigned)>
    <ISOSpeed(32 bits, unsigned)>
    <Brightness(32 bits, floating-point)>
    <Saturation(32 bits, floating-point)>

    Compressed image data:

    <Image(Variable)>

    Compressed depth data:

    <Depth(Variable)>
```

If this is the final frame in the batch then `IsFinalFrame` will be non-zero.  This should be used by the application to indicate the complete batch has been delivered.

Intrinsics and extrinsics for the color and depth cameras in this frame can be looked up given the `ServerGuid` and `CameraIndex` fields.

The `BackReference` is either `0` or `-1` and indicates which prior frame is referenced by this frame.  A value of `0` means this is a keyframe and can be used as a sync point in the video stream.  A value of `-1` means this frame depends on receipt of the prior frame.  The player may decide to go ahead and attempt to decode the frame but will either fail to decode, or it will play back with reduced quality.

The `ImageBytes` is the number of trailing bytes of `Image` field data.  It should be interpreted as raw H.264 or H.265 coded video based on the `Video Info (Chunk type 2)` `VideoType` field.  The video is not expected to be seekable only to keyframes indicated by `Batch Info (Chunk type 3)` `BackReference` field = 0.  Other frames must be decoded in sequence starting from the keyframes.  There is no MP4 or other container inside this field.  Instead it is using Annex B formatted NAL units (starting with 00 00 01..).

The `DepthBytes` is the number of trailing bytes of `Depth` field data.  Its format is defined by the Zdepth library.  The format itself indicates whether it is a lossy or lossless format.

The remaining fields are all optional metadata:

The `Accelerometer` vector is optional data that is an accelerometer sample near the time of capture.

The `ExposureUsec` is the number of microseconds that the camera shutter was open for the color camera.  The `AutoWhiteBalanceUsec` is the AWB setting used for this frame.  The `ISOSpeed` is the camera's reported ISO speed for this frame.

The `Brightness` value is the lighting adjustment performed during capture.  It is additive.  A value of 0 indicates no modification.

The `Saturation` value is the saturation adjustment performed during capture.  It is multiplicative.  A value for 1 indicates no modification.
