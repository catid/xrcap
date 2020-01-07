// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include "GltfWriter.hpp"

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
                "byteStride": 32,
                "byteOffset": 0
            },
            {
                "buffer": 0,
                "byteLength": 512,
                "byteStride": 32,
                "byteOffset": 500
            },
            {
                "buffer": 1,
                "byteLength": 10242,
                "byteOffset": 0
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
                "byteOffset": 12,
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
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "TEXCOORD_0": 1
                        },
                        "indices": 2,
                        "material": 3,
                        "mode": 4
                    }
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
        ],
        "nodes": [
            {
                "name": "model1",
                "mesh": 0,
                "rotation": [
                    0,
                    0,
                    0,
                    1
                ],
                "translation": [
                    -17.7082,
                    -11.4156,
                    2.0922
                ]
            }
        ]
    }
*/
bool WriteFrameToGltfFile(const XrcapFrame& frame, const char* file_path)
{
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        return false;
    }

    const uint32_t version = 2;
    file.write("glTF", 4);
    const uint32_t length = 2;
    file.write((const char*)&version, 4);

    return true;
}


} // namespace core
