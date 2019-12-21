// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#include <core_logging.hpp>
#include "ColorNormalization.hpp"
using namespace core;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


//------------------------------------------------------------------------------
// Illumination-invariant test

void IlluminationInvariantTest()
{
    spdlog::info("Illumination-invariant test");

    {
        float IxMin = 1.f;
        float IxMax = 0.f;
        float IyMin = 1.f;
        float IyMax = 0.f;
        float IzMin = 1.f;
        float IzMax = 0.f;

        for (int y = 0; y < 256; ++y)
        {
            for (int u = 0; u < 256; ++u)
            {
                for (int v = 0; v < 256; ++v)
                {
                    float X, Y, Z;
                    RGBToXYZ(y, u, v, X, Y, Z);
                    float Ix, Iy, Iz;
                    Ix = 0.9465229f * X + 0.2946927f * Y - 0.1313419f * Z;
                    Iy = -0.1179179f * X + 0.9929960f * Y + 0.007371554f * Z;
                    Iz = 0.09230461f * X - 0.04645794f * Y + 0.9946464f * Z;
                    if (Ix < IxMin) {
                        IxMin = Ix;
                    }
                    if (Ix > IxMax) {
                        IxMax = Ix;
                    }
                    if (Iy < IyMin) {
                        IyMin = Iy;
                    }
                    if (Iy > IyMax) {
                        IyMax = Iy;
                    }
                    if (Iz < IzMin) {
                        IzMin = Iz;
                    }
                    if (Iz > IzMax) {
                        IzMax = Iz;
                    }
                }
            }
        }

        spdlog::info("For RGB input: Bx(max)={} Bx(min)={}", IxMax, IxMin);
        spdlog::info("For RGB input: By(max)={} By(min)={}", IyMax, IyMin);
        spdlog::info("For RGB input: Bz(max)={} Bz(min)={}", IzMax, IzMin);
    }

    {
        float IxMin = 1.f;
        float IxMax = 0.f;
        float IyMin = 1.f;
        float IyMax = 0.f;
        float IzMin = 1.f;
        float IzMax = 0.f;

        for (int y = 0; y < 256; ++y)
        {
            for (int u = 0; u < 256; ++u)
            {
                for (int v = 0; v < 256; ++v)
                {
                    float r, g, b;
                    YCbCrToRGB(y, u, v, r, g, b);
                    float X, Y, Z;
                    RGBToXYZ(r, g, b, X, Y, Z);
                    float Ix, Iy, Iz;
                    Ix = 0.9465229f * X + 0.2946927f * Y - 0.1313419f * Z;
                    Iy = -0.1179179f * X + 0.9929960f * Y + 0.007371554f * Z;
                    Iz = 0.09230461f * X - 0.04645794f * Y + 0.9946464f * Z;
                    if (Ix < IxMin) {
                        IxMin = Ix;
                    }
                    if (Ix > IxMax) {
                        IxMax = Ix;
                    }
                    if (Iy < IyMin) {
                        IyMin = Iy;
                    }
                    if (Iy > IyMax) {
                        IyMax = Iy;
                    }
                    if (Iz < IzMin) {
                        IzMin = Iz;
                    }
                    if (Iz > IzMax) {
                        IzMax = Iz;
                    }
                }
            }
        }

        spdlog::info("For YCbCr input: Bx(max)={} Bx(min)={}", IxMax, IxMin);
        spdlog::info("For YCbCr input: By(max)={} By(min)={}", IyMax, IyMin);
        spdlog::info("For YCbCr input: Bz(max)={} Bz(min)={}", IzMax, IzMin);
    }

    {
        float IxMin = 1.f;
        float IxMax = 0.f;
        float IyMin = 1.f;
        float IyMax = 0.f;
        float IzMin = 1.f;
        float IzMax = 0.f;

        for (int y = 0; y < 256; ++y)
        {
            for (int u = 0; u < 256; ++u)
            {
                for (int v = 0; v < 256; ++v)
                {
                    float r, g, b;
                    YCbCrToRGB(y, u, v, r, g, b);
                    float X, Y, Z;
                    RGBToXYZ(r, g, b, X, Y, Z);
                    float Ix, Iy, Iz;
                    XYZToIlluminationInvariant(X, Y, Z, Ix, Iy, Iz);
                    if (Ix < IxMin) {
                        IxMin = Ix;
                    }
                    if (Ix > IxMax) {
                        IxMax = Ix;
                    }
                    if (Iy < IyMin) {
                        IyMin = Iy;
                    }
                    if (Iy > IyMax) {
                        IyMax = Iy;
                    }
                    if (Iz < IzMin) {
                        IzMin = Iz;
                    }
                    if (Iz > IzMax) {
                        IzMax = Iz;
                    }
                }
            }
        }

        spdlog::info("For YCbCr input: Ix(max)={} Ix(min)={}", IxMax, IxMin);
        spdlog::info("For YCbCr input: Iy(max)={} Iy(min)={}", IyMax, IyMin);
        spdlog::info("For YCbCr input: Iz(max)={} Iz(min)={}", IzMax, IzMin);
    }

    {
        float IxMin = 1.f;
        float IxMax = 0.f;
        float IyMin = 1.f;
        float IyMax = 0.f;
        float IzMin = 1.f;
        float IzMax = 0.f;

        for (int r = 0; r < 256; ++r)
        {
            for (int g = 0; g < 256; ++g)
            {
                for (int b = 0; b < 256; ++b)
                {
                    float X, Y, Z;
                    RGBToXYZ(r, g, b, X, Y, Z);
                    float Ix, Iy, Iz;
                    XYZToIlluminationInvariant(X, Y, Z, Ix, Iy, Iz);
                    if (Ix < IxMin) {
                        IxMin = Ix;
                    }
                    if (Ix > IxMax) {
                        IxMax = Ix;
                    }
                    if (Iy < IyMin) {
                        IyMin = Iy;
                    }
                    if (Iy > IyMax) {
                        IyMax = Iy;
                    }
                    if (Iz < IzMin) {
                        IzMin = Iz;
                    }
                    if (Iz > IzMax) {
                        IzMax = Iz;
                    }
                }
            }
        }

        spdlog::info("For RGB input: Ix(max)={} Ix(min)={}", IxMax, IxMin);
        spdlog::info("For RGB input: Iy(max)={} Iy(min)={}", IyMax, IyMin);
        spdlog::info("For RGB input: Iz(max)={} Iz(min)={}", IzMax, IzMin);
    }

    int w,h,n;
    unsigned char *data = stbi_load("test.jpg", &w, &h, &n, 3);

    float R0, R1, R2;
    {
        float X, Y, Z;
        RGBToXYZ(200, 200, 200, X, Y, Z);
        XYZToIlluminationInvariant(X, Y, Z, R0, R1, R2);
    }

    uint8_t* image = data;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x, image += 3)
        {
            uint8_t r = image[0];
            uint8_t g = image[1];
            uint8_t b = image[2];
            float X, Y, Z;
            RGBToXYZ(r, g, b, X, Y, Z);
            float Ix, Iy, Iz;
            XYZToIlluminationInvariant(X, Y, Z, Ix, Iy, Iz);
            Ix -= R0;
            Iy -= R1;
            Iz -= R2;
            float delta = Ix * Ix + Iy * Iy + Iz * Iz;
            delta = std::sqrtf(delta) / 178.f;
            image[0] = static_cast<uint8_t>( delta * 255.f );
            image[1] = static_cast<uint8_t>( delta * 255.f );
            image[2] = static_cast<uint8_t>( delta * 255.f );
        }
    }

    int result = stbi_write_bmp("output.bmp", w, h, 3, data);
    if (result == 0) {
        spdlog::error("Failed to write output image");
    } else {
        spdlog::info("Successfully wrote output image");
    }
}


//------------------------------------------------------------------------------
// Entrypoint

int main(int argc, char* argv[])
{
    CORE_UNUSED2(argc, argv);

    SetCurrentThreadName("Main");

    SetupAsyncDiskLog("depth_mesh_tests.txt");

    IlluminationInvariantTest();

    return 0;
}
