// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "GltfWriter.hpp"

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <sstream>
#include <fstream>

namespace core {


//------------------------------------------------------------------------------
// GLTF Writer

/*
    {
        "buffers": [
            {
                "byteLength": 1024,
                "uri": "model1.bin"
            },
            {
                "byteLength": 1024,
                "uri": "texture1.jpg"
            }
        ],
        "bufferViews": [
            {
                "buffer": 0,
                "byteLength": 512,
                "byteStride": 20,
                "byteOffset": 0
            },
            {
                "buffer": 0,
                "byteLength": 512,
                "byteStride": 20,
                "byteOffset": 12
            },
            {
                "buffer": 1,
                "byteLength": 10242,
                "byteStride": 4
                "byteOffset": 0,
            }
        ],
        "images": [
            {
                "bufferView": 2,
                "mimeType": "image/jpeg" 
            }
        ],
        "samplers": [
            {
                "magFilter": 9729,
                "minFilter": 9729,
                "wrapS": 33071,
                "wrapT": 33071
            }
        ],
        "textures" : [
            "source": 0,
            "sampler": 0
        ],
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
        "materials": [
            {
                "name": "matt",
                "doubleSided": false,
                "pbrMetallicRoughness": {
                    "baseColorFactor": [ 1.0, 1.0, 1.0, 1.0 ],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.0
                }
            }
        ],
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
    }
*/

bool WriteFrameToGlbFile(const XrcapFrame& frame, const char* file_path)
{
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::string chunkData1;
    std::string chunkData2;

    std::ostringstream json;
    json << "{" << std::endl;
    json << "    \"buffers\": [" << std::endl;
    json << "        {" << std::endl;
    json << "            \"byteLength\": " << chunkData1.size() << "," << std::endl;
    json << "            \"uri\": \"model1.bin\"" << std::endl;
    json << "        }," << std::endl;
    json << "        {" << std::endl;
    json << "            \"byteLength\": " << chunkData2.size() << "," << std::endl;
    json << "            \"uri\": \"texture1.jpg\"" << std::endl;
    json << "        }" << std::endl;
    json << "    ]," << std::endl;
    json << "    \"bufferViews\": [" << std::endl;
    json << "        {" << std::endl;
    json << "            \"buffer\": 0," << std::endl;
    json << "            \"byteLength\": " << chunkData1.size() << "," << std::endl;
    json << "            \"byteStride\": " << 20 << "," << std::endl;
    json << "            \"byteOffset\": 0" << std::endl;
    json << "        }," << std::endl;
    json << "        {" << std::endl;
    json << "            \"buffer\": 0," << std::endl;
    json << "            \"byteLength\": " << 512 << "," << std::endl;
    json << "            \"byteStride\": " << 20 << "," << std::endl;
    json << "            \"byteOffset\": " << 12 << std::endl;
    json << "        }," << std::endl;
    json << "        {" << std::endl;
    json << "            \"buffer\": 1," << std::endl;
    json << "            \"byteLength\": " << 10242 << "," << std::endl;
    json << "            \"byteStride\": " << 4 << "," << std::endl;
    json << "            \"byteOffset\": " << 0 << std::endl;
    json << "        }" << std::endl;
    json << "    ]," << std::endl;
    json << "    \"images\": [" << std::endl;
    json << "        {" << std::endl;
    json << "            \"bufferView\": " << 2 << "," << std::endl;
    json << "            \"mimeType\": \"image/jpeg\"" << std::endl;
    json << "        }" << std::endl;
    json << "    ]," << std::endl;

    std::string chunkData0;

    const uint32_t chunkLength0 = static_cast<uint32_t>( chunkData0.size() );
    const uint32_t chunkLength1 = static_cast<uint32_t>( chunkData1.size() );
    const uint32_t length = 12 + 8 + chunkLength0 + 8 + chunkLength1;

    const uint32_t version = 2;
    file.write("glTF", 4);
    file.write((const char*)&version, 4);
    file.write((const char*)&length, 4);

    file.write((const char*)&chunkLength0, 4);
    file.write("JSON", 4);
    file.write(chunkData0.c_str(), chunkData0.size());

    file.write((const char*)&chunkLength1, 4);
    file.write("BIN\0", 4);
    file.write(chunkData1.c_str(), chunkData1.size());

    return true;
}


} // namespace core
