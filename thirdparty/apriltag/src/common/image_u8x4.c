/* Copyright (C) 2013-2016, The Regents of The University of Michigan.
All rights reserved.
This software was developed in the APRIL Robotics Lab under the
direction of Edwin Olson, ebolson@umich.edu. This software may be
available under alternative licensing terms; contact the address above.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the Regents of The University of Michigan.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_u8x4.h"

// least common multiple of 64 (sandy bridge cache line) and 64 (stride needed
// for 16byte-wide RGBA processing).
#define DEFAULT_ALIGNMENT_U8X4 64

image_u8x4_t *image_u8x4_create(unsigned int width, unsigned int height)
{
    return image_u8x4_create_alignment(width, height, DEFAULT_ALIGNMENT_U8X4);
}

image_u8x4_t *image_u8x4_create_alignment(unsigned int width, unsigned int height, unsigned int alignment)
{
    int stride = 4*width;

    if ((stride % alignment) != 0)
        stride += alignment - (stride % alignment);

    uint8_t *buf = calloc(height*stride, sizeof(uint8_t));

    // const initializer
    image_u8x4_t tmp = { .width = width, .height = height, .stride = stride, .buf = buf };

    image_u8x4_t *im = calloc(1, sizeof(image_u8x4_t));
    memcpy(im, &tmp, sizeof(image_u8x4_t));
    return im;
}

image_u8x4_t *image_u8x4_copy(const image_u8x4_t *in)
{
    uint8_t *buf = malloc(in->height*in->stride*sizeof(uint8_t));
    memcpy(buf, in->buf, in->height*in->stride*sizeof(uint8_t));

    // const initializer
    image_u8x4_t tmp = { .width = in->width, .height = in->height, .stride = in->stride, .buf = buf };

    image_u8x4_t *copy = calloc(1, sizeof(image_u8x4_t));
    memcpy(copy, &tmp, sizeof(image_u8x4_t));
    return copy;
}

void image_u8x4_destroy(image_u8x4_t *im)
{
    if (!im)
        return;

    free(im->buf);
    free(im);
}
