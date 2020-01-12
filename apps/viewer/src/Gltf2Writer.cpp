// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "Gltf2Writer.hpp"

#include <draco/compression/encode.h>

#include <core.hpp>
#include <core_logging.hpp>

#include <sstream>
#include <fstream>
#include <vector>

#include <turbojpeg.h>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
using JsonAllocatorT = rapidjson::MemoryPoolAllocator<>;
using JsonBufferT = rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocatorT>;

namespace core {


//------------------------------------------------------------------------------
// JSON Tools

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

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("byteLength"); writer.Uint(byteLength);
        writer.String("uri"); writer.String(uri);
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
    unsigned byteStride = 1;
    unsigned byteOffset = 0;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("buffer"); writer.Uint(buffer);
        writer.String("byteLength"); writer.Uint(byteLength);
        writer.String("byteStride"); writer.Uint(byteStride);
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
    unsigned bufferView = 0;
    unsigned byteOffset = 0;
    unsigned componentType = 5126;
    unsigned count = 2345;
    std::string type = "VEC3";

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("bufferView"); writer.Uint(bufferView);
        writer.String("byteOffset"); writer.Uint(byteOffset);
        writer.String("componentType"); writer.Uint(componentType);
        writer.String("count"); writer.Uint(count);
        writer.String("type"); writer.String(type);
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

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("material"); writer.Uint(material);
        writer.String("mode"); writer.Uint(mode);
        writer.String("indices"); writer.Uint(indices);
        writer.String("attributes");
        writer.StartObject();
        writer.String("POSITION"); writer.Uint(attributes_POSITION);
        writer.String("TEXCOORD_0"); writer.Uint(attributes_TEXCOORD_0);
        writer.EndObject();
        writer.EndObject();
    }
};

struct GltfMesh {
    std::vector<GltfMeshPrimitive> primitives;

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
        writer.String("primitives");
        writer.StartArray();
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
        writer.String("matrix");
        writer.StartArray();
        for (int i = 0; i < 16; ++i) {
            writer.Double(matrix[i]);
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

/*
    {
        "buffers": [
        ],
        "bufferViews": [
        ],
        "images": [
        ],
        "samplers": [
        ],
        "textures" : [
        ],
        "accessors": [
        ],
        "materials": [
        ],
        "meshes": [
        ],
        "nodes": [
        ],
        "cameras": [
        ]
    }
*/

struct GltfJsonFile
{
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

    template<class W> void Serialize(W& writer) const {
        writer.StartObject();
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
        writer.String("cameras"); writer.StartArray();
        for (const auto& camera : cameras) {
            camera.Serialize(writer);
        }
        writer.EndArray();
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
    bool Serialize(const XrcapFrame& frame);

    std::vector<GltfBufferData> Buffers;

private:
    void Cleanup();
    bool SerializeImage(const XrcapPerspective& perspective, unsigned& image_buffer_index);
    bool SerializePerspective(GltfJsonFile& json, const XrcapPerspective& perspective);

    char AllocatorBuffer[1024];
    std::unique_ptr<JsonAllocatorT> Allocator;
    std::unique_ptr<JsonBufferT> JsonBuffer; // .GetString() .GetSize()

    tjhandle JpegHandle = nullptr;
    std::vector<uint8_t> TempU, TempV;

    std::vector<GltfBufferData> JpegBuffers;
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
}

bool GltfBuffers::Serialize(const XrcapFrame& frame)
{
    if (frame.Valid) {
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
    rapidjson::Writer<JsonBufferT> writer(*JsonBuffer);

    GltfJsonFile json;

    for (unsigned perspective_index = 0; perspective_index < XRCAP_PERSPECTIVE_COUNT; ++perspective_index) {
        SerializePerspective(json, frame.Perspectives[perspective_index]);
    }

    GltfCamera camera;
    // TBD: Do we need this?
    json.cameras.push_back(camera);

    json.Serialize(writer);

    return true;
}

static const int kJpegQuality = 90;
static const int kJpegFlags = TJFLAG_ACCURATEDCT | TJFLAG_PROGRESSIVE;

bool GltfBuffers::SerializeImage(const XrcapPerspective& perspective, unsigned& image_buffer_index)
{
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
        kJpegQuality,
        kJpegFlags);

    if (success != 0) {
        spdlog::error("Failed to compress JPEG: {} [{}]", tjGetErrorStr2(JpegHandle), tjGetErrorCode(JpegHandle));
        return false;
    }

    JpegBuffers.push_back({
        jpeg_buf,
        jpeg_size
    });

    image_buffer_index = static_cast<unsigned>( Buffers.size() );
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

bool GltfBuffers::SerializePerspective(GltfJsonFile& json, const XrcapPerspective& perspective)
{
    unsigned file_image_buffer_number = 0;
    if (!SerializeImage(perspective, file_image_buffer_number)) {
        return false;
    }

    std::ostringstream node_name;
    node_name << "Node::" << perspective.Guid << "::" << perspective.CameraIndex;
    const std::string node_name_str = node_name.str();

    std::ostringstream model_buffer_uri;
    model_buffer_uri << node_name_str << ".model";

    std::ostringstream image_buffer_uri;
    image_buffer_uri << node_name_str << ".jpg";

    const unsigned mesh_index = static_cast<unsigned>( json.meshes.size() );

    GltfNode node;
    node.name = node_name_str;
    node.mesh = mesh_index;
    if (perspective.Extrinsics->IsIdentity) {
        for (int i = 0; i < 16; ++i) {
            node.matrix[i] = kIdentityMatrix[i];
        }
    } else {
        for (int i = 0; i < 16; ++i) {
            node.matrix[i] = static_cast<double>( perspective.Extrinsics->Transform[i] );
        }
    }
    json.nodes.push_back(node);

    const unsigned model_buffer_index = static_cast<unsigned>( json.buffers.size() );
    GltfBuffer modelBuffer;
    json.buffers.push_back(modelBuffer);

    const unsigned xyzuv_buffer_view = static_cast<unsigned>( json.bufferViews.size() );
    GltfBufferView xyzuvBufferView;
    json.bufferViews.push_back(xyzuvBufferView);

    const unsigned image_buffer_index = static_cast<unsigned>( json.buffers.size() );
    GltfBuffer imageBuffer;
    imageBuffer.byteLength = 0;
    imageBuffer.uri = node_name_str;
    json.buffers.push_back(imageBuffer);

    const unsigned image_buffer_view = static_cast<unsigned>( json.bufferViews.size() );
    GltfBufferView imageBufferView;
    imageBufferView.buffer = image_buffer_index;
    imageBufferView.byteLength = imageBuffer.byteLength;
    imageBufferView.byteOffset = 0;
    imageBufferView.byteStride = 0;
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

    GltfTexture texture;
    texture.sampler = sampler_index;
    texture.source = image_index;
    json.textures.push_back(texture);

    const unsigned indices_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor indicesAccessor;
    indicesAccessor.bufferView = image_buffer_view;
    json.accessors.push_back(indicesAccessor);

    const unsigned position_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor positionAccessor;
    positionAccessor.bufferView = image_buffer_view;
    json.accessors.push_back(positionAccessor);

    const unsigned texcoord_accessor_index = static_cast<unsigned>( json.accessors.size() );
    GltfAccessor texcoordAccessor;
    texcoordAccessor.bufferView = image_buffer_view;
    json.accessors.push_back(texcoordAccessor);

    const unsigned material_index = static_cast<unsigned>( json.materials.size() );
    GltfMaterial material;
    material.name = node_name_str;
    material.doubleSided = false;
    //material.pbrMetallicRoughness; // TBD
    json.materials.push_back(material);

    GltfMeshPrimitive primitive;
    primitive.mode = 4; // GL_TRIANGLES
    primitive.material = material_index;
    primitive.indices = indices_accessor_index;
    primitive.attributes_TEXCOORD_0 = texcoord_accessor_index;
    primitive.attributes_POSITION = position_accessor_index;
    GltfMesh mesh;
    mesh.primitives.push_back(primitive);
    json.meshes.push_back(mesh);

    return true;
}


//------------------------------------------------------------------------------
// GLTF Writer

bool WriteFrameToGlbFile(const XrcapFrame& frame, const char* file_path)
{
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        spdlog::error("Unable to open file: {}", file_path);
        return false;
    }

    GltfBuffers buffers;
    if (!buffers.Serialize(frame)) {
        return false;
    }

    const unsigned count = static_cast<unsigned>( buffers.Buffers.size() );
    CORE_DEBUG_ASSERT(count > 0);

    GlbFileHeader file_header;
    unsigned length = kGlbFileHeaderBytes;
    for (const auto& buffer : buffers.Buffers) {
        length += kGlbChunkHeaderBytes + buffer.Size;
    }
    file_header.Length = static_cast<uint32_t>( length );
    file.write((const char*)&file_header, kGlbFileHeaderBytes);

    // Unroll buffer 0: Pad JSON out to a multiple of 4 bytes
    {
        GlbChunkHeader chunk_header;
        chunk_header.Type = kGlbChunkType_Json;
        chunk_header.Length = static_cast<uint32_t>( buffers.Buffers[0].Size );

        unsigned padding = chunk_header.Length % 4;
        if (padding != 0) {
            padding = 4 - padding;
        }

        chunk_header.Length += padding;
        file.write((const char*)&chunk_header, kGlbChunkHeaderBytes);
        file.write((const char*)buffers.Buffers[0].Data, buffers.Buffers[0].Size);
        file.write("    ", padding);
    }

    for (unsigned i = 1; i < count; ++i) {
        GlbChunkHeader chunk_header;
        chunk_header.Type = kGlbChunkType_Bin;
        chunk_header.Length = static_cast<uint32_t>( buffers.Buffers[i].Size );
        file.write((const char*)&chunk_header, kGlbChunkHeaderBytes);
        file.write((const char*)buffers.Buffers[i].Data, buffers.Buffers[i].Size);
    }

    return true;
}


} // namespace core
