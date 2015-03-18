/*
 * Copyright (c) Imazen LLC.
 * No part of this project, including this file, may be copied, modified,
 * propagated, or distributed except as permitted in COPYRIGHT.txt.
 * Licensed under the GNU Affero General Public License, Version 3.0.
 * Commercial licenses available at http://imageresizing.net/
 */

#ifdef _MSC_VER
#pragma unmanaged
#endif

#include "fastscaling_private.h"
#include <string.h>



void ScaleBgraFloatRows(BitmapFloat * from, uint32_t from_row, BitmapFloat * to, uint32_t to_row, uint32_t row_count, ContributionType * weights)
{

    const uint32_t from_step = from->channels;
    const uint32_t to_step = to->channels;
    const uint32_t dest_buffer_count = to->w;
    const uint32_t min_channels = umin(from_step, to_step);
    uint32_t ndx;
    //TODO: assert min_channels < 5
    float avg[4];


    // if both have alpha, process it
    if (from_step == 4 && to_step == 4)
    {
        for (uint32_t row = 0; row < row_count; row++)
        {
            const float* __restrict source_buffer = from->pixels + ((from_row + row) * from->float_stride);
            float* __restrict dest_buffer = to->pixels + ((to_row + row) * to->float_stride);


            for (ndx = 0; ndx < dest_buffer_count; ndx++) {
                float r = 0, g = 0, b = 0, a = 0;
                const int left = weights[ndx].Left;
                const int right = weights[ndx].Right;

                const float* __restrict weightArray = weights[ndx].Weights;
                int i;

                /* Accumulate each channel */
                for (i = left; i <= right; i++) {
                    const float weight = weightArray[i - left];

                    b += weight * source_buffer[i * from_step];
                    g += weight * source_buffer[i * from_step + 1];
                    r += weight * source_buffer[i * from_step + 2];
                    a += weight * source_buffer[i * from_step + 3];
                }

                dest_buffer[ndx * to_step] = b;
                dest_buffer[ndx * to_step + 1] = g;
                dest_buffer[ndx * to_step + 2] = r;
                dest_buffer[ndx * to_step + 3] = a;
            }
        }
    }
    else if (from_step == 3 && to_step == 3)
    {
        for (uint32_t row = 0; row < row_count; row++)
        {
            const float* __restrict source_buffer = from->pixels + ((from_row + row) * from->float_stride);
            float* __restrict dest_buffer = to->pixels + ((to_row + row) * to->float_stride);


            for (ndx = 0; ndx < dest_buffer_count; ndx++) {
                float r = 0, g = 0, b = 0;
                const int left = weights[ndx].Left;
                const int right = weights[ndx].Right;

                const float * weightArray = weights[ndx].Weights;
                int i;

                /* Accumulate each channel */
                for (i = left; i <= right; i++) {
                    const float weight = weightArray[i - left];

                    b += weight * source_buffer[i * from_step];
                    g += weight * source_buffer[i * from_step + 1];
                    r += weight * source_buffer[i * from_step + 2];
                }

                dest_buffer[ndx * to_step] = b;
                dest_buffer[ndx * to_step + 1] = g;
                dest_buffer[ndx * to_step + 2] = r;
            }
        }
    }
    else{
        for (uint32_t row = 0; row < row_count; row++)
        {
            const float* __restrict source_buffer = from->pixels + ((from_row + row) * from->float_stride);
            float* __restrict dest_buffer = to->pixels + ((to_row + row) * to->float_stride);

            avg[0] = 0;
            avg[1] = 0;
            avg[2] = 0;
            avg[3] = 0;
            for (ndx = 0; ndx < dest_buffer_count; ndx++) {
                const int left = weights[ndx].Left;
                const int right = weights[ndx].Right;

                const float* __restrict weightArray = weights[ndx].Weights;

                /* Accumulate each channel */
                for (int i = left; i <= right; i++) {
                    const float weight = weightArray[i - left];

                    for (uint32_t j = 0; j < min_channels; j++)
                        avg[j] += weight * source_buffer[i * from_step + j];
                }

                for (uint32_t j = 0; j < min_channels; j++)
                    dest_buffer[ndx * to_step + j] = avg[j];
            }
        }
    }
}


static inline void HalveRowByDivisor(const unsigned char* from, unsigned short * to, const unsigned int to_count, const int divisor, const int from_step, const int to_step){
    int to_b, from_b;
    const int to_bytes = to_count * to_step;
    const int divisor_stride = from_step * divisor;

    if (divisor == 2)
    {
        if (to_count % 2 == 0){
            for (to_b = 0, from_b = 0; to_b < to_bytes; to_b += 2 * to_step, from_b += 4 * from_step){
                for (int i = 0; i < 2 * to_step; i++){
                    to[to_b + i] += from[from_b + i] + from[from_b + i + from_step];
                }
            }
        }
        else{
            for (to_b = 0, from_b = 0; to_b < to_bytes; to_b += to_step, from_b += 2 * from_step){
                for (int i = 0; i < to_step; i++){
                    to[to_b + i] += from[from_b + i] + from[from_b + i + from_step];
                }
            }
        }

    }
    else if (divisor == 3){
        for (to_b = 0, from_b = 0; to_b < to_bytes; to_b += to_step, from_b += 3 * from_step){
            for (int i = 0; i < to_step; i++){
                to[to_b + i] += from[from_b + i] + from[from_b + i + from_step] + from[from_b + i + 2 * from_step];
            }
        }
    }
    else if (divisor == 4){
        for (to_b = 0, from_b = 0; to_b < to_bytes; to_b += to_step, from_b += 4 * from_step){
            for (int i = 0; i < to_step; i++){
                to[to_b + i] += from[from_b + i] + from[from_b + i + from_step] + from[from_b + i + 2 * from_step] + from[from_b + i + 3 * from_step];
            }
        }
    }
    else{
        for (to_b = 0, from_b = 0; to_b < to_bytes; to_b += to_step, from_b += divisor_stride){
            for (int i = 0; i < to_step; i++){
                for (int f = 0; f < divisor_stride; f += from_step){
                    to[to_b + i] += from[from_b + i + f];

                }
            }
        }
    }
}


static int HalveInternal(const BitmapBgra * from,
    const BitmapBgra * to, const int to_w, const int to_h, const int to_stride, const int divisor)
{

    const int to_w_bytes = to_w * BitmapPixelFormat_bytes_per_pixel (to->fmt);
    unsigned short *buffer = (unsigned short *)calloc(to_w_bytes, sizeof(unsigned short));
    if (buffer == NULL) return 0;

    int y, b, d;
    const unsigned short divisorSqr = divisor * divisor;
    unsigned int shift = 0;
    if (isPowerOfTwo(divisorSqr)){
        shift = intlog2(divisorSqr);
    }

    const uint32_t from_bytes_pp = BitmapPixelFormat_bytes_per_pixel (from->fmt);
    const uint32_t to_bytes_pp = BitmapPixelFormat_bytes_per_pixel (to->fmt);

    //TODO: Ensure that from is equal or greater than divisorx to_w and t_h

    for (y = 0; y < to_h; y++){
        memset(buffer, 0, sizeof(short) * to_w_bytes);
        for (d = 0; d < divisor; d++){
            HalveRowByDivisor (from->pixels + (y * divisor + d) * from->stride, buffer, to_w, divisor, from_bytes_pp, to_bytes_pp);
        }
        unsigned char * dest_line = to->pixels + y * to_stride;

        if (shift == 2){
            for (b = 0; b < to_w_bytes; b++){
                dest_line[b] = buffer[b] >> 2;
            }
        }
        else if (shift == 3){
            for (b = 0; b < to_w_bytes; b++){
                dest_line[b] = buffer[b] >> 3;
            }
        }
        else if (shift > 0){
            for (b = 0; b < to_w_bytes; b++){
                dest_line[b] = buffer[b] >> shift;
            }
        }
        else{
            for (b = 0; b < to_w_bytes; b++){
                dest_line[b] = buffer[b] / divisorSqr;
            }
        }
    }

    free(buffer);

    return 1;
}

// int as divisior???
int Halve(const BitmapBgra * from, const BitmapBgra * to, int divisor){
    return HalveInternal(from, to, to->w, to->h, to->stride, divisor);
}


// is it correct to use an int as the divisor here?
int HalveInPlace(BitmapBgra * from, int divisor)
{
    int to_w = from->w / divisor;
    int to_h = from->h / divisor;
    int to_stride = to_w * BitmapPixelFormat_bytes_per_pixel (from->fmt);
    int r = HalveInternal(from, from, to_w, to_h, to_stride, divisor);
    from->w = to_w;
    from->h = to_h;
    from->stride = to_stride;
    return r;
}

