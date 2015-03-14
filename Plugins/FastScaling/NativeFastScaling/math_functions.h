/*
 * Copyright (c) Imazen LLC.
 * No part of this project, including this file, may be copied, modified,
 * propagated, or distributed except as permitted in COPYRIGHT.txt.
 * Licensed under the GNU Affero General Public License, Version 3.0.
 * Commercial licenses available at http://imageresizing.net/
 */
#pragma once
#ifdef _MSC_VER
#pragma unmanaged
#endif

#include <stdint.h>
#include <math.h>
#include <limits.h>



#define IR_PI  3.1415926535897932384626433832795
#define IR_SINC(value) (value == 0 ? 1 : sin(value * IR_PI) / (value * IR_PI))

#define IR_GUASSIAN(x, stdDev) (exp((-x * x) / (2 * stdDev * stdDev)) / (sqrt(2 * IR_PI) * stdDev))

#ifndef MIN
#   define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#   define MIN3(a,b,c) ((a)<(b)?(MIN(a,c)):(MIN(b,c)))
#ifndef MAX
#   define MAX(a,b) ((a)<(b)?(b):(a))
#endif
#ifndef MAX3
#   define MAX3(a,b,c) ((a)<(b)?(MAX(b,c)):(MAX(a,c)))
#endif
#ifndef NULL
#   define NULL 0
#endif

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))



static inline 
uint8_t 
uchar_clamp_ff(float clr) {
    uint16_t result;

    result = (uint16_t)(int16_t)(clr + 0.5);

    if (result > 255) {
        result = (clr < 0) ? 0 : 255;
    }

    return (uint8_t)result;
}

static inline
uint16_t
clamp_01_to_04096(float clr) {
    const uint16_t result = (uint16_t)lround(clr * 4096.0f);

    if (result > 4096) {
        return clr < 0 ? 0 : 4096;
    }
    return result;
}



static inline
int overflow2(int a, int b)
{
    if (a < 1 || b < 1) {
        return 1;
    }
    if (a > INT_MAX / b) {
        return 1;
    }
    return 0;
}

static inline
int intlog2(unsigned int val) {
    int ret = -1;
    while (val != 0) {
        val >>= 1;
        ret++;
    }
    return ret;
}

static inline int isPowerOfTwo(unsigned int x)
{
    return ((x != 0) && !(x & (x - 1)));
}
