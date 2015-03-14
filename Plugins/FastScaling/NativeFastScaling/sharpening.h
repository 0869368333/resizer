/*
 * Copyright (c) Imazen LLC.
 * No part of this project, including this file, may be copied, modified,
 * propagated, or distributed except as permitted in COPYRIGHT.txt.
 * Licensed under the GNU Affero General Public License, Version 3.0.
 * Commercial licenses available at http://imageresizing.net/
 */
#pragma once

#include "shared.h"

#ifdef _MSC_VER
#pragma unmanaged
#endif


static void BgraSharpenInPlaceX(BitmapBgraPtr im, float pct)
{
    const float n = -pct / (pct - 1); //if 0 < pct < 1
    const float outer_coeff = n / -2.0f;
    const float inner_coeff = n + 1;

    uint32_t y, current, prev, next;

    const uint32_t sy = im->h;
    const uint32_t stride = im->stride;
    const uint32_t bpp = im->bpp;


    if (pct <= 0 || im->w < 3 || bpp < 3) return;

    for (y = 0; y < sy; y++)
    {
        unsigned char *row = im->pixels + y * stride;
        for (current = bpp, prev = 0, next = bpp + bpp; next < stride; prev = current, current = next, next += bpp){
            //We never sharpen the alpha channel
            //TODO - we need to buffer the left pixel to prevent it from affecting later calculations
            for (uint32_t i = 0; i < 3; i++)
                row[current + i] = uchar_clamp_ff(outer_coeff * (float)row[prev + i] + inner_coeff * (float)row[current + i] + outer_coeff * (float)row[next + i]);
        }
    }
}


static void
SharpenBgraFloatInPlace(float* buf, unsigned int count, double pct,
int step)
{

    const float n = -pct / (pct - 1); //if 0 < pct < 1
    const float c_o = n / -2.0;
    const float c_i = n + 1;

    unsigned int ndx;

    // if both have alpha, process it
    if (step == 4)
    {
        float left_b = buf[0 * 4 + 0];
        float left_g = buf[0 * 4 + 1];
        float left_r = buf[0 * 4 + 2];
        float left_a = buf[0 * 4 + 3];

        for (ndx = 1; ndx < count - 1; ndx++) {
            const float b = buf[ndx * 4 + 0];
            const float g = buf[ndx * 4 + 1];
            const float r = buf[ndx * 4 + 2];
            const float a = buf[ndx * 4 + 3];
            buf[ndx * 4 + 0] = left_b * c_o + b * c_i + buf[(ndx + 1) * 4 + 0] * c_o;
            buf[ndx * 4 + 1] = left_g * c_o + g * c_i + buf[(ndx + 1) * 4 + 1] * c_o;
            buf[ndx * 4 + 2] = left_r * c_o + r * c_i + buf[(ndx + 1) * 4 + 2] * c_o;
            buf[ndx * 4 + 3] = left_a * c_o + a * c_i + buf[(ndx + 1) * 4 + 3] * c_o;
            left_b = b;
            left_g = g;
            left_r = r;
            left_a = a;
        }
    }
    // otherwise do the same thing without 4th chan
    // (ifs in loops are expensive..)
    else
    {
        float left_b = buf[0 * 3 + 0];
        float left_g = buf[0 * 3 + 1];
        float left_r = buf[0 * 3 + 2];

        for (ndx = 1; ndx < count - 1; ndx++) {
            const float b = buf[ndx * 3 + 0];
            const float g = buf[ndx * 3 + 1];
            const float r = buf[ndx * 3 + 2];
            buf[ndx * 3 + 0] = left_b * c_o + b * c_i + buf[(ndx + 1) * 3 + 0] * c_o;
            buf[ndx * 3 + 1] = left_g * c_o + g * c_i + buf[(ndx + 1) * 3 + 1] * c_o;
            buf[ndx * 3 + 2] = left_r * c_o + r * c_i + buf[(ndx + 1) * 3 + 2] * c_o;
            left_b = b;
            left_g = g;
            left_r = r;
        }

    }

}





static void
SharpenBgraFloatRowsInPlace(BitmapFloatPtr im, uint32_t start_row, uint32_t row_count, double pct){
    for (uint32_t row = start_row; row < start_row + row_count; row++){
        SharpenBgraFloatInPlace(im->pixels + (im->float_stride * row), im->w, pct, im->channels);
    }
}
