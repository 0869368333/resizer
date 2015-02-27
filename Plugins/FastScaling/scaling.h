#include "Stdafx.h"
#include "shared.h"
#include "sharpening.h"
#pragma once
#pragma unmanaged




static inline void
ScaleBgraFloatRows(BitmapFloatPtr from, const uint32_t from_row, BitmapFloatPtr to, const uint32_t to_row, const uint32_t row_count, ContributionType * weights){

    const uint32_t from_step = from->channels;
    const uint32_t to_step = to->channels;
    const uint32_t dest_buffer_count = to->w;



    for (uint32_t row = 0; row < row_count; row++)
    {
        const float* __restrict source_buffer = from->pixels + ((from_row + row) * from->float_stride);
        float* __restrict dest_buffer = to->pixels + ((to_row + row) * to->float_stride);

        uint32_t ndx;

        // if both have alpha, process it
        if (from_step == 4 && to_step == 4)
        {
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
        else if (from_step == 3 && to_step == 3)
        {
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
        else{
            const uint32_t min_channels = MIN(from_step, to_step);
            float* avg = (float*)_malloca(min_channels * sizeof(float));
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
            _freea(avg);
        }
    }
}





