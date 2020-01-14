// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "Gltf2Writer.hpp"

#include <draco/compression/encode.h>

#include <core.hpp>
#include <core_logging.hpp>
#include <core_string.hpp>

#include <sstream>
#include <fstream>
#include <vector>

#include <turbojpeg.h>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
using JsonAllocatorT = rapidjson::MemoryPoolAllocator<>;
using JsonBufferT = rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocatorT>;

#include <vectormath.hpp>

#ifndef M_PI_FLOAT
# define M_PI_FLOAT 3.14159265f
#endif

namespace core {


//------------------------------------------------------------------------------
// Constants

// 0 = slowest speed, best compression
// 10 = fastest, but worst compression
static const int kDracoEncodeSpeed = 0;
static const int kDracoDecodeSpeed = 0;

//#define GLTF2_PRETTY_JSON_WRITER


//------------------------------------------------------------------------------
// Tools

static unsigned ChunkPadding4(unsigned bytes)
{
    unsigned padding = bytes % 4;
    if (padding != 0) {
        padding = 4 - padding;
    }
    return padding;
}

static unsigned ChunkLengthRoundUp4(unsigned bytes)
{
    return bytes + ChunkPadding4(bytes);
}


//------------------------------------------------------------------------------
// JSON Tools

/*
    "asset": {
        "version": "2.0",
        "generator": "https://github.com/catid/xrcap",
        "copyright": "2019 (c) Christopher A. Taylor"
    }
*/

struct GltfAsset {
    std::string version = "2.0";
    std::string generator = "https://github.com/catid/xrcap";
    std::string copyright = "2019 (c) Christopher A. Taylor";

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("version"); writer.String(version);
        writer.String("generator"); writer.String(generator);
        writer.String("copyright"); writer.String(copyright);
        writer.EndObject();
    }
};

/*
    "buffers": [
        {
            "byteLength": 1024,
            "uri": "model1.bin"
        }
    ],
*/

struct GltfBuffer {
    unsigned byteLength = 0;
    std::string uri;
    bool uri_undefined = true;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("byteLength"); writer.Uint(byteLength);
        if (!uri_undefined) {
            writer.String("uri"); writer.String(uri);
        }
        writer.EndObject();
    }
};

/*
    "bufferViews": [
        {
            "buffer": 0,
            "byteLength": 512,
            "byteStride": 20,
            "byteOffset": 0
        }
    ],
*/

struct GltfBufferView {
    unsigned buffer = 0;
    unsigned byteLength = 1;
    unsigned byteOffset = 0;

    bool byteStrideDefined = false;
    unsigned byteStride = 1;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("buffer"); writer.Uint(buffer);
        writer.String("byteLength"); writer.Uint(byteLength);
        if (byteStrideDefined) {
            writer.String("byteStride"); writer.Uint(byteStride);
        }
        writer.String("byteOffset"); writer.Uint(byteOffset);
        writer.EndObject();
    }
};

/*
    "images": [
        {
            "bufferView": 2,
            "mimeType": "image/jpeg" 
        }
    ],
*/

struct GltfImage {
    unsigned bufferView = 0;
    std::string mimeType = "image/jpeg";

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("bufferView"); writer.Uint(bufferView);
        writer.String("mimeType"); writer.String(mimeType);
        writer.EndObject();
    }
};

/*
    "samplers": [
        {
            "magFilter": 9729,
            "minFilter": 9729,
            "wrapS": 33071,
            "wrapT": 33071
        }
    ],
*/

struct GltfSampler {
    unsigned magFilter = 9729;
    unsigned minFilter = 9729;
    unsigned wrapS = 33071;
    unsigned wrapT = 33071;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("magFilter"); writer.Uint(magFilter);
        writer.String("minFilter"); writer.Uint(minFilter);
        writer.String("wrapS"); writer.Uint(wrapS);
        writer.String("wrapT"); writer.Uint(wrapT);
        writer.EndObject();
    }
};

/*
    "textures" : [
        {
            "source": 0,
            "sampler": 0
        }
    ],
*/

struct GltfTexture {
    unsigned source = 0;
    unsigned sampler = 0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("source"); writer.Uint(source);
        writer.String("sampler"); writer.Uint(sampler);
        writer.EndObject();
    }
};

/*
    "accessors": [
        {
            "bufferView": 0,
            "byteOffset": 0,
            "componentType": 5126,
            "count": 2345,
            "type": "VEC3"
        },
        {
            "bufferView": 0,
            "byteOffset": 0,
            "componentType": 5126,
            "count": 2345,
            "type": "VEC2"
        },
        {
            "bufferView": 1,
            "byteOffset": 0,
            "componentType": 5125,
            "count": 1234,
            "type": "SCALAR"
        }
    ],
*/

struct GltfAccessor {
    bool bufferViewUndefined = false;
    unsigned bufferView = 0;
    unsigned byteOffset = 0;
    unsigned componentType = 5126;
    unsigned count = 2345;
    std::string type = "VEC3";

    bool IncludeUintMinMax = false;
    std::vector<unsigned> UintMins, UintMaxes;

    bool IncludeDoubleMinMax = false;
    std::vector<double> DoubleMins, DoubleMaxes;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        if (!bufferViewUndefined) {
            writer.String("bufferView"); writer.Uint(bufferView);
            writer.String("byteOffset"); writer.Uint(byteOffset);
        }
        writer.String("componentType"); writer.Uint(componentType);
        writer.String("count"); writer.Uint(count);
        writer.String("type"); writer.String(type);
        if (IncludeUintMinMax) {
            writer.String("min"); writer.StartArray();
            for (unsigned min_i : UintMins) {
                writer.Uint(min_i);
            }
            writer.EndArray();
            writer.String("max"); writer.StartArray();
            for (unsigned max_i : UintMaxes) {
                writer.Uint(max_i);
            }
            writer.EndArray();
        }
        if (IncludeDoubleMinMax) {
            writer.String("min"); writer.StartArray();
            for (double min_i : DoubleMins) {
                writer.Double(min_i);
            }
            writer.EndArray();
            writer.String("max"); writer.StartArray();
            for (double max_i : DoubleMaxes) {
                writer.Double(max_i);
            }
            writer.EndArray();
        }
        writer.EndObject();
    }
};

/*
    "materials": [
        {
            "name": "matt",
            "doubleSided": false,
            "pbrMetallicRoughness": {
                "baseColorTexture": {
                    "index": 0,
                    "texCoord": 0
                },
                "baseColorFactor": [ 1.0, 1.0, 1.0, 1.0 ],
                "metallicFactor": 0.0,
                "roughnessFactor": 0.0
            }
        }
    ],
*/

struct GltfBaseColorTexture {
    unsigned index = 0;
    unsigned texCoord = 0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("index"); writer.Uint(index);
        writer.String("texCoord"); writer.Uint(texCoord);
        writer.EndObject();
    }
};

struct GltfPbrMetallicRoughness {
    GltfBaseColorTexture baseColorTexture;
    double baseColorFactor[4] = {
        0.0, 0.0, 1.0, 1.0
    };
    double metallicFactor = 0.0;
    double roughnessFactor = 0.0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();

        writer.String("baseColorFactor"); writer.StartArray();
        for (int i = 0; i < 4; ++i) {
            writer.Double(baseColorFactor[i]);
        }
        writer.EndArray();

        writer.String("baseColorTexture"); baseColorTexture.Serialize(writer);
        writer.String("metallicFactor"); writer.Double(metallicFactor);
        writer.String("roughnessFactor"); writer.Double(roughnessFactor);
        writer.EndObject();
    }
};

struct GltfMaterial {
    std::string name;
    bool doubleSided = false;
    GltfPbrMetallicRoughness pbrMetallicRoughness;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("name"); writer.String(name);
        writer.String("doubleSided"); writer.Bool(doubleSided);
        writer.String("pbrMetallicRoughness"); pbrMetallicRoughness.Serialize(writer);
        writer.EndObject();
    }
};

/*
    "meshes": [
        {
            "name": "Node name"
            "primitives": [
                {
                    "material": 0,
                    "mode": 4,
                    "attributes": {
                        "POSITION": 0,
                        "TEXCOORD_0": 1
                    },
                    "indices": 2
                }
            ]
        }
    ],
*/

struct GltfMeshPrimitive {
    unsigned material = 0;
    unsigned mode = 4;
    unsigned attributes_POSITION = 0;
    unsigned attributes_TEXCOORD_0 = 1;
    unsigned indices = 2;

    bool enableDraco = false;
    unsigned draco_bufferView = 0;
    unsigned draco_POSITION = 0;
    unsigned draco_TEXCOORD_0 = 0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("material"); writer.Uint(material);
        writer.String("mode"); writer.Uint(mode);
        writer.String("indices"); writer.Uint(indices);
        writer.String("attributes"); writer.StartObject();
        writer.String("POSITION"); writer.Uint(attributes_POSITION);
        writer.String("TEXCOORD_0"); writer.Uint(attributes_TEXCOORD_0);
        writer.EndObject();
        if (enableDraco) {
            writer.String("extensions"); writer.StartObject();

            writer.String("KHR_draco_mesh_compression"); writer.StartObject();
            writer.String("bufferView"); writer.Uint(draco_bufferView);

            writer.String("attributes"); writer.StartObject();
            writer.String("POSITION"); writer.Uint(draco_POSITION);
            writer.String("TEXCOORD_0"); writer.Uint(draco_TEXCOORD_0);
            writer.EndObject();

            writer.EndObject();

            writer.EndObject();
        }
        writer.EndObject();
    }
};

struct GltfMesh {
    std::string name;
    std::vector<GltfMeshPrimitive> primitives;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("name"); writer.String(name);
        writer.String("primitives"); writer.StartArray();
        for (const auto& primitive : primitives) {
            primitive.Serialize(writer);
        }
        writer.EndArray();
        writer.EndObject();
    }
};

/*
    "nodes": [
        {
            "name": "model1",
            "mesh": 0,
            "matrix": [
                1, 2, 3, 4,
                1, 2, 3, 4,
                1, 2, 3, 4,
                1, 2, 3, 4
            ]
        }
    ],
*/

struct GltfNode {
    std::string name;
    unsigned mesh;
    double matrix[4 * 4];

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("name"); writer.String(name);
        writer.String("mesh"); writer.Uint(mesh);
        writer.String("matrix"); writer.StartArray();
        for (int i = 0; i < 16; ++i) {
            writer.Double(matrix[i]);
        }
        writer.EndArray();
        writer.EndObject();
    }
};

/*
    "scenes": [
        {
            "name": "singleScene",
            "nodes": [
                0
            ]
        }
    ],
*/

struct GltfScene {
    std::string name;
    std::vector<unsigned> nodes;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("name"); writer.String(name);
        writer.String("nodes"); writer.StartArray();
        for (unsigned node : nodes) {
            writer.Uint(node);
        }
        writer.EndArray();
        writer.EndObject();
    }
};

/*
    "cameras": [
        {
            "name": "Finite perspective camera",
            "type": "perspective",
            "perspective": {
                "aspectRatio": 1.5,
                "yfov": 0.660593,
                "zfar": 100,
                "znear": 0.01
            }      
        }
    ]
*/

struct GltfCameraPerspective {
    double aspectRatio = 1.5;
    double yfov = 0.660593;
    double zfar = 100.0;
    double znear = 0.01;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("aspectRatio"); writer.Double(aspectRatio);
        writer.String("yfov"); writer.Double(yfov);
        writer.String("zfar"); writer.Double(zfar);
        writer.String("znear"); writer.Double
        (znear);
        writer.EndObject();
    }
};

struct GltfCamera
{
    std::string name;
    std::string type;
    GltfCameraPerspective perspective;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("name"); writer.String(name);
        writer.String("type"); writer.String(type);
        writer.String("perspective"); perspective.Serialize(writer);
        writer.EndObject();
    }
};

// JSON file root serializer
struct GltfJsonFile
{
    GltfAsset asset;
    std::vector<GltfBuffer> buffers;
    std::vector<GltfBufferView> bufferViews;
    std::vector<GltfImage> images;
    std::vector<GltfSampler> samplers;
    std::vector<GltfTexture> textures;
    std::vector<GltfAccessor> accessors;
    std::vector<GltfMaterial> materials;
    std::vector<GltfMesh> meshes;
    std::vector<GltfNode> nodes;
    std::vector<GltfCamera> cameras;
    std::vector<GltfScene> scenes;
    std::vector<std::string> extensionsUsed;
    std::vector<std::string> extensionsRequired;
    unsigned scene = 0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("asset"); asset.Serialize(writer);
        writer.String("buffers"); writer.StartArray();
        for (const auto& buffer : buffers) {
            buffer.Serialize(writer);
        }
        writer.EndArray();
        writer.String("bufferViews"); writer.StartArray();
        for (const auto& bufferView : bufferViews) {
            bufferView.Serialize(writer);
        }
        writer.EndArray();
        writer.String("images"); writer.StartArray();
        for (const auto& image : images) {
            image.Serialize(writer);
        }
        writer.EndArray();
        writer.String("samplers"); writer.StartArray();
        for (const auto& sampler : samplers) {
            sampler.Serialize(writer);
        }
        writer.EndArray();
        writer.String("textures"); writer.StartArray();
        for (const auto& texture : textures) {
            texture.Serialize(writer);
        }
        writer.EndArray();
        writer.String("accessors"); writer.StartArray();
        for (const auto& accessor : accessors) {
            accessor.Serialize(writer);
        }
        writer.EndArray();
        writer.String("materials"); writer.StartArray();
        for (const auto& material : materials) {
            material.Serialize(writer);
        }
        writer.EndArray();
        writer.String("meshes"); writer.StartArray();
        for (const auto& mesh : meshes) {
            mesh.Serialize(writer);
        }
        writer.EndArray();
        writer.String("nodes"); writer.StartArray();
        for (const auto& node : nodes) {
            node.Serialize(writer);
        }
        writer.EndArray();
        if (!cameras.empty()) {
            writer.String("cameras"); writer.StartArray();
            for (const auto& camera : cameras) {
                camera.Serialize(writer);
            }
            writer.EndArray();
        }
        writer.String("scenes"); writer.StartArray();
        for (const auto& scene_i : scenes) {
            scene_i.Serialize(writer);
        }
        writer.EndArray();
        writer.String("scene"); writer.Uint(scene);
        if (!extensionsUsed.empty()) {
            writer.String("extensionsUsed"); writer.StartArray();
            for (const auto& extension : extensionsUsed) {
                writer.String(extension);
            }
            writer.EndArray();
        }
        if (!extensionsRequired.empty()) {
            writer.String("extensionsRequired"); writer.StartArray();
            for (const auto& extension : extensionsRequired) {
                writer.String(extension);
            }
            writer.EndArray();
        }
        writer.EndObject();
        CORE_DEBUG_ASSERT(writer.IsComplete());
    }
};


//------------------------------------------------------------------------------
// GLTF Buffers

struct GltfBufferData
{
    uint8_t* Data = nullptr;
    unsigned long Size = 0;
};

struct GltfBuffers
{
    ~GltfBuffers();

    // On success: Returns true.
    // Buffer 0 will be the JSON metadata
    // Remaining buffers are all binary
    // On failure: Returns false.
    bool Serialize(const XrcapFrame& frame, const GltfParams& params);

    std::vector<GltfBufferData> Buffers;

private:
    void Cleanup();
    bool SerializeImage(
        const XrcapPerspective& perspective,
        unsigned& image_offset,
        unsigned& image_bytes,
        const GltfParams& params);
    bool SerializePerspective(
        GltfJsonFile& json,
        const XrcapPerspective& perspective,
        const GltfParams& params);

    char AllocatorBuffer[1024];
    std::unique_ptr<JsonAllocatorT> Allocator;
    std::unique_ptr<JsonBufferT> JsonBuffer; // .GetString() .GetSize()

    tjhandle JpegHandle = nullptr;
    std::vector<uint8_t> TempU, TempV;

    std::vector<GltfBufferData> JpegBuffers;

    // Incremented as BIN chunk is filled in
    unsigned BufferOffset = 0;

    std::vector<std::shared_ptr<draco::EncoderBuffer>> DracoBuffers;
};

GltfBuffers::~GltfBuffers()
{
    Cleanup();
}

void GltfBuffers::Cleanup()
{
    Buffers.clear();

    for (const auto& buffer : JpegBuffers) {
        if (buffer.Data) {
            tjFree(buffer.Data);
        }
    }
    JpegBuffers.clear();
    if (JpegHandle) {
        tjDestroy(JpegHandle);
        JpegHandle = nullptr;
    }

    JsonBuffer.reset();
    Allocator.reset();
    BufferOffset = 0;

    DracoBuffers.clear();
}

bool GltfBuffers::Serialize(const XrcapFrame& frame, const GltfParams& params)
{
    if (!frame.Valid) {
        spdlog::error("Unable to serialize invalid XrcapFrame to glTF");
        return false;
    }

    Cleanup();

    JpegHandle = tjInitCompress();
    if (!JpegHandle) {
        spdlog::error("tjInitCompress failed");
        return false;
    }

    Allocator = std::make_unique<JsonAllocatorT>(AllocatorBuffer, sizeof(AllocatorBuffer));
    JsonBuffer = std::make_unique<JsonBufferT>(Allocator.get());
#ifdef GLTF2_PRETTY_JSON_WRITER
    rapidjson::PrettyWriter<JsonBufferT> writer(*JsonBuffer);
#else
    rapidjson::Writer<JsonBufferT> writer(*JsonBuffer);
#endif

    // Place an empty buffer on the front, which will contain the JSON data
    Buffers.push_back({
        nullptr,
        0
    });

    GltfJsonFile json;

    if (params.EnableDraco) {
        json.extensionsUsed.push_back("KHR_draco_mesh_compression");
        json.extensionsRequired.push_back("KHR_draco_mesh_compression");
    }

    // Prepare a primary scene to be filled in with nodes from each perspective
    std::ostringstream scene_name;
    scene_name << "XrCap_frame:" << frame.FrameNumber << "_msec:" << (frame.VideoStartUsec / 1000);
    GltfScene primary_scene;
    primary_scene.name = scene_name.str();
    json.scenes.push_back(primary_scene);

    // BIN buffer
    GltfBuffer binBuffer;
    binBuffer.byteLength = 0;
    binBuffer.uri_undefined = true;
    json.buffers.push_back(binBuffer);

    unsigned perspective_count = 0;
    for (unsigned perspective_index = 0; perspective_index < XRCAP_PERSPECTIVE_COUNT; ++perspective_index) {
        const auto& perspective = frame.Perspectives[perspective_index];
        if (!perspective.Valid) {
            continue;
        }
        if (!SerializePerspective(json, perspective, params)) {
            spdlog::error("Perspective failed to serialize: guid={} camera={}", perspective.Guid, perspective.CameraIndex);
            continue;
        }
        ++perspective_count;
    }
    if (perspective_count <= 0) {
        spdlog::error("No valid perspectives to serialize");
        return false;
    }

#if 0 // Camera is unused
    GltfCamera camera;
    camera.name = "Finite perspective camera";
    camera.type = "perspective";
    json.cameras.push_back(camera);
#endif

    CORE_DEBUG_ASSERT(!json.buffers.empty() && BufferOffset > 0);
    json.buffers[0].byteLength = BufferOffset;

    json.Serialize(writer);

    CORE_DEBUG_ASSERT(!Buffers.empty());
    Buffers[0].Data = (uint8_t*)JsonBuffer->GetString();
    Buffers[0].Size = static_cast<unsigned long>( JsonBuffer->GetSize() );

    //spdlog::warn("TEST: {}", std::string(JsonBuffer->GetString(), JsonBuffer->GetSize()));

    spdlog::info("Successfully serialized {} perspectives", perspective_count);

    return true;
}

static const int kJpegFlags = TJFLAG_ACCURATEDCT | TJFLAG_PROGRESSIVE;

bool GltfBuffers::SerializeImage(
    const XrcapPerspective& perspective,
    unsigned& image_offset,
    unsigned& image_bytes,
    const GltfParams& params)
{
    if (perspective.Width < 16 || perspective.Height < 16) {
        spdlog::error("Image dimensions invalid: w={} h={}", perspective.Width, perspective.Height);
        return false;
    }
    if (perspective.ChromaWidth < 16 || perspective.ChromaHeight < 16) {
        spdlog::error("Chroma dimensions invalid: w={} h={}", perspective.ChromaWidth, perspective.ChromaHeight);
        return false;
    }

    TempU.resize(perspective.ChromaWidth * perspective.ChromaHeight);
    TempV.resize(perspective.ChromaWidth * perspective.ChromaHeight);
    uint8_t* u = TempU.data();
    uint8_t* v = TempV.data();
    const uint8_t* uv = perspective.UV;
    for (int row = 0; row < perspective.ChromaHeight; ++row) {
        for (int col = 0; col < perspective.ChromaWidth; ++col) {
            u[col] = uv[col * 2];
            v[col] = uv[col * 2 + 1];
        }
        uv += perspective.ChromaWidth * 2;
        u += perspective.ChromaWidth;
        v += perspective.ChromaWidth;
    }

    const uint8_t* planes[3] = {
        perspective.Y,
        TempU.data(),
        TempV.data()
    };
    const int strides[3] = {
        perspective.Width,
        perspective.ChromaWidth,
        perspective.ChromaWidth
    };
    uint8_t* jpeg_buf = nullptr;
    unsigned long jpeg_size = 0;

    const int success = tjCompressFromYUVPlanes(
        JpegHandle,
        planes,
        perspective.Width,
        strides,
        perspective.Height,
        TJSAMP_420, // YUV 4:2:0
        &jpeg_buf,
        &jpeg_size,
        params.JpegQuality,
        kJpegFlags);

    if (success != 0) {
        spdlog::error("Failed to compress JPEG: {} [{}]", tjGetErrorStr2(JpegHandle), tjGetErrorCode(JpegHandle));
        return false;
    }

    JpegBuffers.push_back({
        jpeg_buf,
        jpeg_size
    });

    image_offset = BufferOffset;
    image_bytes = static_cast<unsigned>( jpeg_size );
    BufferOffset += ChunkLengthRoundUp4(image_bytes);

    Buffers.push_back({
        jpeg_buf,
        jpeg_size
    });

    return true;
}

static const double kIdentityMatrix[16] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
};

bool GltfBuffers::SerializePerspective(
    GltfJsonFile& json,
    const XrcapPerspective& perspective,
    const GltfParams& params)
{
    // Convert to JPEG and store in file buffer list
    unsigned image_offset = 0, image_bytes = 0;
    if (!SerializeImage(perspective, image_offset, image_bytes, params)) {
        return false;
    }

    // Generate names
    std::ostringstream node_name;
    node_name << "Node::" << perspective.Guid << "::" << perspective.CameraIndex;
    const std::string node_name_str = node_name.str();

    // Image:

    const unsigned image_buffer_view = static_cast<unsigned>( json.bufferViews.size() );
    GltfBufferView imageBufferView;
    imageBufferView.buffer = 0; // All buffers concatenated
    imageBufferView.byteLength = image_bytes;
    imageBufferView.byteOffset = image_offset;
    imageBufferView.byteStrideDefined = false;
    json.bufferViews.push_back(imageBufferView);

    const unsigned image_index = static_cast<unsigned>( json.images.size() );
    GltfImage image;
    image.bufferView = image_buffer_view;
    image.mimeType = "image/jpeg";
    json.images.push_back(image);

    const unsigned sampler_index = static_cast<unsigned>( json.samplers.size() );
    GltfSampler sampler;
    sampler.magFilter = 9729; // GL_CLAMP_TO_EDGE
    sampler.minFilter = 9729;
    sampler.wrapS = 33071; // GL_LINEAR
    sampler.wrapT = 33071;
    json.samplers.push_back(sampler);

    const unsigned texture_index = static_cast<unsigned>( json.textures.size() );
    GltfTexture texture;
    texture.sampler = sampler_index;
    texture.source = image_index;
    json.textures.push_back(texture);

    const unsigned material_index = static_cast<unsigned>( json.materials.size() );
    GltfMaterial material;
    material.name = node_name_str;
    material.doubleSided = false;
    auto &baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
    baseColorTexture.index = texture_index;
    baseColorTexture.texCoord = 0; // TEXCOORD_0
    json.materials.push_back(material);

    // Draco:

    int draco_position_index = 0;
    int draco_texcoord_index = 0;
    unsigned draco_buffer_view_index = 0;
    unsigned xyz_buffer_view_index = 0;
    unsigned uv_buffer_view_index = 0;
    if (params.EnableDraco)
    {
        std::shared_ptr<draco::Mesh> dracoMesh(std::make_shared<draco::Mesh>());

        const unsigned numVertices = perspective.FloatsCount / 5;
        dracoMesh->set_num_points(numVertices);

        const unsigned triangleCount = perspective.IndicesCount / 3;
        dracoMesh->SetNumFaces(triangleCount);

        //draco::DataBuffer xyzuv_buffer;
        //xyzuv_buffer.Resize(perspective.FloatsCount * sizeof(float));
        //xyzuv_buffer.Write(0, perspective.XyzuvVertices, perspective.FloatsCount * sizeof(float));

        draco::GeometryAttribute geoatt_position;
        geoatt_position.Init(draco::GeometryAttribute::Type::POSITION, nullptr, 3, draco::DataType::DT_FLOAT32, false, 12, 0);
        draco_position_index = dracoMesh->AddAttribute(geoatt_position, false, numVertices);
        {
            auto pTexAttrib = dracoMesh->attribute(draco_position_index);
            pTexAttrib->Reset(numVertices);
            pTexAttrib->buffer()->Resize(12 * numVertices);
            float* data = (float*)pTexAttrib->buffer()->data();
            const float* xyzuv = perspective.XyzuvVertices;
            for (unsigned i = 0; i < numVertices; ++i) {
                data[0] = xyzuv[0];
                data[1] = xyzuv[1];
                data[2] = xyzuv[2];
                data += 3;
                xyzuv += 5;
            }
        }

        draco::GeometryAttribute geoatt_texcoord;
        geoatt_texcoord.Init(draco::GeometryAttribute::Type::TEX_COORD, nullptr, 2, draco::DataType::DT_FLOAT32, false, 8, 0);
        draco_texcoord_index = dracoMesh->AddAttribute(geoatt_texcoord, false, numVertices);
        {
            auto pTexAttrib = dracoMesh->attribute(draco_texcoord_index);
            pTexAttrib->Reset(numVertices);
            pTexAttrib->buffer()->Resize(8 * numVertices);
            float* data = (float*)pTexAttrib->buffer()->data();
            const float* xyzuv = perspective.XyzuvVertices + 3;
            for (unsigned i = 0; i < numVertices; ++i) {
                data[0] = xyzuv[0];
                data[1] = xyzuv[1];
                data += 2;
                xyzuv += 5;
            }
        }

        for (unsigned i = 0; i < triangleCount; ++i) {
            draco::Mesh::Face face;
            for (unsigned k = 0; k < 3; ++k) {
                face[k] = 3 * i + k;
            }
            dracoMesh->SetFace(draco::FaceIndex(i), face);
        }

        const int numAttribs = dracoMesh->num_attributes();
        for (int i = 0; i < numAttribs; ++i) {
            draco::PointAttribute* pAttrib = dracoMesh->attribute(i);
            pAttrib->SetExplicitMapping(triangleCount * 3);
            for (unsigned j = 0; j < triangleCount; ++j) {
                for (unsigned k = 0; k < 3; ++k) {
                    pAttrib->SetPointMapEntry(draco::PointIndex(j * 3 + k), draco::AttributeValueIndex(perspective.Indices[j * 3 + k]));
                }
            }
        }

        // Required: DRACO_ATTRIBUTE_DEDUPLICATION_SUPPORTED
        dracoMesh->DeduplicateAttributeValues();
        //dracoMesh->DeduplicatePointIds();

        draco::Encoder encoder;
        encoder.SetSpeedOptions(kDracoEncodeSpeed, kDracoDecodeSpeed);
        //encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 8);
        //encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 6);

        std::shared_ptr<draco::EncoderBuffer> encoder_buffer;
        encoder_buffer = std::make_shared<draco::EncoderBuffer>();
        draco::Status status = encoder.EncodeMeshToBuffer(*dracoMesh, encoder_buffer.get());
        if (status.code() != draco::Status::OK) {
            spdlog::error("Draco EncodeMeshToBuffer failed: {}", status.error_msg_string());
            return false;
        }

        //spdlog::info("Draco compressed = {}", dracoBuffer.size());
        DracoBuffers.push_back(encoder_buffer);

        const unsigned draco_buffer_offset = BufferOffset;
        GltfBufferData draco_buffer_data;
        draco_buffer_data.Data = (uint8_t*)encoder_buffer->data();
        draco_buffer_data.Size = static_cast<unsigned long>( encoder_buffer->size() );
        Buffers.push_back(draco_buffer_data);
        BufferOffset += ChunkLengthRoundUp4(draco_buffer_data.Size);

        draco_buffer_view_index = static_cast<unsigned>( json.bufferViews.size() );
        GltfBufferView dracoBufferView;
        dracoBufferView.buffer = 0; // All buffers concatenated
        dracoBufferView.byteLength = static_cast<unsigned>( draco_buffer_data.Size );
        dracoBufferView.byteOffset = draco_buffer_offset;
        dracoBufferView.byteStrideDefined = false;
        json.bufferViews.push_back(dracoBufferView);
    }
    else // Uncompressed (Draco disabled):
    {
        // XYZ, UV buffers:

        const unsigned xyzuv_buffer_offset = BufferOffset;
        GltfBufferData xyzuv_buffer_data;
        xyzuv_buffer_data.Data = reinterpret_cast<uint8_t*>( perspective.XyzuvVertices );
        xyzuv_buffer_data.Size = static_cast<unsigned long>( perspective.FloatsCount * sizeof(float) );
        Buffers.push_back(xyzuv_buffer_data);
        BufferOffset += ChunkLengthRoundUp4(xyzuv_buffer_data.Size);

        xyz_buffer_view_index = static_cast<unsigned>( json.bufferViews.size() );
        GltfBufferView xyzBufferView;
        xyzBufferView.buffer = 0; // All buffers concatenated
        xyzBufferView.byteLength = xyzuv_buffer_data.Size;
        xyzBufferView.byteOffset = xyzuv_buffer_offset;
        xyzBufferView.byteStrideDefined = true;
        xyzBufferView.byteStride = 20; // x, y, z, u, v
        json.bufferViews.push_back(xyzBufferView);

        uv_buffer_view_index = static_cast<unsigned>( json.bufferViews.size() );
        GltfBufferView uvBufferView;
        uvBufferView.buffer = 0; // All buffers concatenated
        uvBufferView.byteLength = xyzuv_buffer_data.Size;
        uvBufferView.byteOffset = xyzuv_buffer_offset + 12;
        uvBufferView.byteStrideDefined = true;
        uvBufferView.byteStride = 20; // x, y, z, u, v
        json.bufferViews.push_back(uvBufferView);
    }

    const unsigned xyz_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor xyzAccessor;
    if (params.EnableDraco) {
        xyzAccessor.bufferViewUndefined = true;
    } else {
        xyzAccessor.bufferView = xyz_buffer_view_index;
        xyzAccessor.byteOffset = 0;
    }
    xyzAccessor.componentType = 5126; // GL_FLOAT
    xyzAccessor.count = perspective.FloatsCount / 5;
    xyzAccessor.type = "VEC3";

    // Calculate xyz min/max
    float xyz_mins[3], xyz_maxes[3];
    const float* xyz_data = perspective.XyzuvVertices;
    xyz_mins[0] = xyz_maxes[0] = xyz_data[0];
    xyz_mins[1] = xyz_maxes[1] = xyz_data[1];
    xyz_mins[2] = xyz_maxes[2] = xyz_data[2];
    xyz_data += 5;
    for (unsigned i = 1; i < xyzAccessor.count; ++i, xyz_data += 5) {
        for (unsigned j = 0; j < 3; ++j) {
            const float value = xyz_data[j];
            if (xyz_mins[j] > value) {
                xyz_mins[j] = value;
            }
            if (xyz_maxes[j] < value) {
                xyz_maxes[j] = value;
            }
        }
    }
    xyzAccessor.IncludeDoubleMinMax = true;
    xyzAccessor.DoubleMins.resize(3);
    xyzAccessor.DoubleMaxes.resize(3);
    for (unsigned j = 0; j < 3; ++j) {
        xyzAccessor.DoubleMins[j] = static_cast<double>( xyz_mins[j] );
        xyzAccessor.DoubleMaxes[j] = static_cast<double>( xyz_maxes[j] );
    }

    json.accessors.push_back(xyzAccessor);

    const unsigned uv_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor uvAccessor;
    if (params.EnableDraco) {
        uvAccessor.bufferViewUndefined = true;
    } else {
        uvAccessor.bufferView = uv_buffer_view_index;
        uvAccessor.byteOffset = 0;
    }
    uvAccessor.componentType = 5126; // GL_FLOAT
    uvAccessor.count = perspective.FloatsCount / 5;
    uvAccessor.type = "VEC2";

    // Calculate uv min/max
    float uv_mins[2], uv_maxes[2];
    const float* uv_data = perspective.XyzuvVertices + 3; // offset to u,v data
    uv_mins[0] = uv_maxes[0] = uv_data[0];
    uv_mins[1] = uv_maxes[1] = uv_data[1];
    uv_data += 5;
    for (unsigned i = 1; i < uvAccessor.count; ++i, uv_data += 5) {
        for (unsigned j = 0; j < 2; ++j) {
            const float value = uv_data[j];
            if (uv_mins[j] > value) {
                uv_mins[j] = value;
            }
            if (uv_maxes[j] < value) {
                uv_maxes[j] = value;
            }
        }
    }
    uvAccessor.IncludeDoubleMinMax = true;
    uvAccessor.DoubleMins.resize(2);
    uvAccessor.DoubleMaxes.resize(2);
    for (unsigned j = 0; j < 2; ++j) {
        uvAccessor.DoubleMins[j] = static_cast<double>( uv_mins[j] );
        uvAccessor.DoubleMaxes[j] = static_cast<double>( uv_maxes[j] );
    }

    json.accessors.push_back(uvAccessor);

    // Indices buffers:

    unsigned indices_buffer_view_index = 0;
    if (!params.EnableDraco)
    {
        const unsigned indices_buffer_offset = BufferOffset;
        GltfBufferData indices_buffer_data;
        indices_buffer_data.Data = reinterpret_cast<uint8_t*>( perspective.Indices );
        indices_buffer_data.Size = static_cast<unsigned long>( perspective.IndicesCount * sizeof(uint32_t) );
        Buffers.push_back(indices_buffer_data);
        BufferOffset += ChunkLengthRoundUp4(indices_buffer_data.Size);

        indices_buffer_view_index = static_cast<unsigned>( json.bufferViews.size() );
        GltfBufferView indicesBufferView;
        indicesBufferView.buffer = 0; // All buffers concatenated
        indicesBufferView.byteLength = indices_buffer_data.Size;
        indicesBufferView.byteOffset = indices_buffer_offset;
        indicesBufferView.byteStrideDefined = false;
        json.bufferViews.push_back(indicesBufferView);
    }

    const unsigned indices_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor indicesAccessor;
    if (params.EnableDraco) {
        indicesAccessor.bufferViewUndefined = true;
    } else {
        indicesAccessor.bufferView = indices_buffer_view_index;
        indicesAccessor.byteOffset = 0;
    }
    indicesAccessor.componentType = 0x1405; // uint32
    indicesAccessor.count = perspective.IndicesCount;
    indicesAccessor.type = "SCALAR";

    // Calculate uint min/max
    unsigned indices_min, indices_max;
    const uint32_t* indices_data = perspective.Indices;
    indices_min = indices_max = indices_data[0];
    for (unsigned i = 1; i < indicesAccessor.count; ++i) {
        const uint32_t value = indices_data[i];
        if (indices_min > value) {
            indices_min = value;
        }
        if (indices_max < value) {
            indices_max = value;
        }
    }
    indicesAccessor.IncludeUintMinMax = true;
    indicesAccessor.UintMins.resize(1);
    indicesAccessor.UintMaxes.resize(1);
    indicesAccessor.UintMins[0] = indices_min;
    indicesAccessor.UintMaxes[0] = indices_max;

    json.accessors.push_back(indicesAccessor);

    // Mesh:

    const unsigned mesh_index = static_cast<unsigned>( json.meshes.size() );
    GltfMeshPrimitive primitive;
    primitive.mode = 4; // GL_TRIANGLES
    primitive.material = material_index;
    primitive.indices = indices_accessor_index;
    primitive.attributes_TEXCOORD_0 = uv_accessor_index;
    primitive.attributes_POSITION = xyz_accessor_index;
    primitive.enableDraco = params.EnableDraco;
    if (params.EnableDraco) {
        primitive.draco_bufferView = draco_buffer_view_index;
        primitive.draco_POSITION = static_cast<unsigned>( draco_position_index );
        primitive.draco_TEXCOORD_0 = static_cast<unsigned>( draco_texcoord_index );
    }
    GltfMesh mesh;
    mesh.name = node_name_str;
    mesh.primitives.push_back(primitive);
    json.meshes.push_back(mesh);

    // Node:

    Matrix4 transform;
    if (perspective.Extrinsics->IsIdentity) {
        transform = Matrix4::identity();
    } else {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                transform.setElem(j, i, perspective.Extrinsics->Transform[i * 4 + j]);
            }
        }
    }

    // Rotate mesh so that it is oriented right-side-up
    transform = Matrix4::rotationZ(M_PI_FLOAT) * transform;

    const unsigned node_index = static_cast<unsigned>( json.nodes.size() );
    GltfNode node;
    node.name = node_name_str;
    node.mesh = mesh_index;
    // Convert row-major to column-major order:
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            node.matrix[j * 4 + i] = static_cast<double>( transform.getElem(j, i) );
        }
    }
    json.nodes.push_back(node);

    // Add node to primary scene
    CORE_DEBUG_ASSERT(!json.scenes.empty());
    json.scenes[0].nodes.push_back(node_index);

    return true;
}


//------------------------------------------------------------------------------
// GLTF Writer

bool WriteFrameToGlbFile(const XrcapFrame& frame, const GltfParams& params)
{
    std::ofstream file(params.OutputFilePath, std::ios::binary);
    if (!file) {
        spdlog::error("Unable to open file: {}", params.OutputFilePath);
        return false;
    }

    GltfBuffers buffers;
    if (!buffers.Serialize(frame, params)) {
        return false;
    }

    const unsigned count = static_cast<unsigned>( buffers.Buffers.size() );
    CORE_DEBUG_ASSERT(count >= 2);

    // Write file header:

    GlbFileHeader file_header;
    unsigned bin_data_length = 0;
    for (unsigned i = 1; i < count; ++i) {
        bin_data_length += ChunkLengthRoundUp4(buffers.Buffers[i].Size);
    }
    const unsigned file_length = kGlbFileHeaderBytes + kGlbChunkHeaderBytes + ChunkLengthRoundUp4(buffers.Buffers[0].Size) + kGlbChunkHeaderBytes + bin_data_length;
    file_header.Length = static_cast<uint32_t>( file_length );
    file.write((const char*)&file_header, kGlbFileHeaderBytes);

    // Write JSON section:

    GlbChunkHeader json_header;
    json_header.Type = kGlbChunkType_Json;
    json_header.Length = static_cast<uint32_t>( buffers.Buffers[0].Size );

    const unsigned json_padding = ChunkPadding4(json_header.Length);
    json_header.Length += json_padding;

    file.write((const char*)&json_header, kGlbChunkHeaderBytes);
    file.write((const char*)buffers.Buffers[0].Data, buffers.Buffers[0].Size);
    if (json_padding > 0) {
        file.write("    ", json_padding);
    }

    // Write BIN section:

    GlbChunkHeader bin_header;
    bin_header.Type = kGlbChunkType_Bin;
    bin_header.Length = bin_data_length;
    file.write((const char*)&bin_header, kGlbChunkHeaderBytes);

    for (unsigned i = 1; i < count; ++i) {
        const unsigned buffer_size = buffers.Buffers[i].Size;
        file.write((const char*)buffers.Buffers[i].Data, buffer_size);

        const unsigned bin_padding = ChunkPadding4(buffer_size);
        if (bin_padding > 0) {
            file.write("\0\0\0\0", bin_padding);
        }
    }
    file.flush();
    file.close();

    return true;
}


} // namespace core
