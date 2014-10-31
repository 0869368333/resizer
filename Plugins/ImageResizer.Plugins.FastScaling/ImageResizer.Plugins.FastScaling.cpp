// This is the main DLL file.

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "ImageResizer.Plugins.FastScaling.h"

#pragma unmanaged

typedef struct BitmapBgraStruct{
    int w;
    int h;
    int stride;
    unsigned char *pixels;
    unsigned int *pixelInts;
    int hasAlpha;
} BitmapBgra;

typedef BitmapBgra *BitmapBgraPtr;

typedef struct BitmapPlanarStruct{
    int w;
    int h;
    int channels;
    float **planes;
}BitmapPlanar;

typedef BitmapPlanar *BitmapPlanarPtr;


#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#define MIN3(a,b,c) ((a)<(b)?(MIN(a,c)):(MIN(b,c)))
#ifndef MAX
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif
#define MAX3(a,b,c) ((a)<(b)?(MAX(b,c)):(MAX(a,c)))

#define NULL 0

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define DEFAULT_BOX_RADIUS					0.5

typedef struct
{
	float *Weights;  /* Normalized weights of neighboring pixels */
	int Left, Right;   /* Bounds of source pixels window */
} ContributionType;  /* Contirbution information for a single pixel */

typedef struct
{
	ContributionType *ContribRow; /* Row (or column) of contribution weights */
	unsigned int WindowSize,      /* Filter window size (of affecting source pixels) */
		LineLength;      /* Length of line (no. or rows / cols) */
} LineContribType;

#define gdTrueColor(r, g, b) (((r) << 16) + \
			      ((g) << 8) +  \
			      (b))

#define gdTrueColorAlpha(r, g, b, a) (((a) << 24) + \
				      ((r) << 16) + \
				      ((g) << 8) +  \
				      (b))

/* Convert a float to an unsigned char, rounding to the nearest
* integer and clamping the result between 0 and max.  The absolute
* value of clr must be less than the maximum value of an unsigned
* short. */
static inline unsigned char
uchar_clamp(float clr, unsigned char max) {
	unsigned short result;

	//assert(fabs(clr) <= SHRT_MAX);

	/* Casting a negative float to an unsigned short is undefined.
	* However, casting a float to a signed truncates toward zero and
	* casting a negative signed value to an unsigned of the same size
	* results in a bit-identical value (assuming twos-complement
	* arithmetic).	 This is what we want: all legal negative values
	* for clr will be greater than 255. */

	/* Convert and clamp. */
	result = (unsigned short)(short)(clr + 0.5);
	if (result > max) {
		result = (clr < 0) ? 0 : max;
	}/* if */

	return result;
}/* uchar_clamp*/




/* Interpolation function ptr */
typedef double(*interpolation_method)(double);

static int overflow2(int a, int b)
{
	if (a <= 0 || b <= 0) {
		return 1;
	}
	if (a > INT_MAX / b) {
		return 1;
	}
	return 0;
}


static BitmapBgraPtr CreateBitmapBgraPtr(int sx, int sy, int zeroed)
{
	int i;
    BitmapBgraPtr im;

    if (overflow2(sx, sy) || overflow2(sizeof(int *), sy) || overflow2(sizeof(int), sx)) {
		return NULL;
	}


    im = (BitmapBgra *)malloc(sizeof(BitmapBgra));
	if (!im) {
		return 0;
	}
    memset(im, 0, sizeof(BitmapBgra));
    im->w = sx;
    im->h = sy;
    im->stride = sx * 4;
	
    if (zeroed){
        im->pixels = (unsigned char *)calloc(sy * im->stride, sizeof(unsigned char));
    }
    else{
        im->pixels = (unsigned char *)malloc(sy * im->stride);
    }
    im->pixelInts = (unsigned int *)im->pixels;

    if (!im->pixels) {
        free(im);
        return 0;
    }
	return im;
}


static void DestroyBitmapBgra(BitmapBgraPtr im)
{
    int i;
    if (im->pixels) {
		free(im->pixels);
    }
    free(im);
}

/**
* Bicubic interpolation kernel (a=-1):
\verbatim
/
| 1-2|t|**2+|t|**3          , if |t| < 1
h(t) = | 4-8|t|+5|t|**2-|t|**3     , if 1<=|t|<2
| 0                         , otherwise
\
\endverbatim
* ***bd*** 2.2004
*/
static inline double filter_bicubic(const double t)
{
    const double abs_t = (double)fabs(t);
    const double abs_t_sq = abs_t * abs_t;
    if (abs_t<1) return 1 - 2 * abs_t_sq + abs_t_sq*abs_t;
    if (abs_t<2) return 4 - 8 * abs_t + 5 * abs_t_sq - abs_t_sq*abs_t;
    return 0;
}


static inline LineContribType *  ContributionsAlloc(unsigned int line_length, unsigned int windows_size)
{
	unsigned int u = 0;
	LineContribType *res;

	res = (LineContribType *)malloc(sizeof(LineContribType));
	if (!res) {
		return NULL;
	}
	res->WindowSize = windows_size;
	res->LineLength = line_length;
	res->ContribRow = (ContributionType *)malloc(line_length * sizeof(ContributionType));


    float *allWeights = (float *)malloc(windows_size * line_length * sizeof(float));
    
    for (int i = 0; i < line_length; i++)
        res->ContribRow[i].Weights = allWeights + (i * windows_size);

	return res;
}

static inline void ContributionsFree(LineContribType * p)
{
    free(p->ContribRow[0].Weights);
	free(p->ContribRow);
	free(p);
}

static inline LineContribType *ContributionsCalc(unsigned int line_size, unsigned int src_size, double scale_d, const interpolation_method pFilter)
{
	double width_d;
	double scale_f_d = 1.0;
	const double filter_width_d = DEFAULT_BOX_RADIUS;
	int windows_size;
	unsigned int u;
	LineContribType *res;

	if (scale_d < 1.0) {
		width_d = filter_width_d / scale_d;
		scale_f_d = scale_d;
	}
	else {
		width_d = filter_width_d;
	}

	windows_size = 2 * (int)ceil(width_d) + 1;
	res = ContributionsAlloc(line_size, windows_size);

	for (u = 0; u < line_size; u++) {
		const double dCenter = (double)u / scale_d;
		/* get the significant edge points affecting the pixel */
		register int iLeft = MAX(0, (int)floor(dCenter - width_d));
		int iRight = MIN((int)ceil(dCenter + width_d), (int)src_size - 1);
		double dTotalWeight = 0.0;
		int iSrc;

		/* Cut edge points to fit in filter window in case of spill-off */
		if (iRight - iLeft + 1 > windows_size)  {
			if (iLeft < ((int)src_size - 1 / 2))  {
				iLeft++;
			}
			else {
				iRight--;
			}
		}

		res->ContribRow[u].Left = iLeft;
		res->ContribRow[u].Right = iRight;

        for (iSrc = iLeft; iSrc <= iRight; iSrc++) {
            dTotalWeight += (res->ContribRow[u].Weights[iSrc - iLeft] = scale_f_d * (*pFilter)(scale_f_d * (dCenter - (double)iSrc)));
        }

        if (dTotalWeight < 0.0) {
            ContributionsFree(res);
            return NULL;
        }

        if (dTotalWeight > 0.0) {
            for (iSrc = iLeft; iSrc <= iRight; iSrc++) {
                res->ContribRow[u].Weights[iSrc - iLeft] /= dTotalWeight;
            }
        }
        
	}
	return res;
}




//#define ScaleAlpha

static inline void
ScaleBgraFloat(float *source_buffer, unsigned int source_buffer_count, unsigned int source_buffer_len,
float *dest_buffer, unsigned int dest_buffer_count, unsigned int dest_buffer_len, ContributionType * weights){

    unsigned int ndx;
    for (ndx = 0; ndx < dest_buffer_count; ndx++) {
        float r = 0, g = 0, b = 0, a = 0;
        const int left = weights[ndx].Left;
        const int right = weights[ndx].Right;

        const float * weightArray = weights[ndx].Weights;
        int i;

        /* Accumulate each channel */
        for (i = left; i <= right; i++) {
            const float weight = weightArray[i - left];

            b += weight * source_buffer[i * 4];
            g += weight * source_buffer[i * 4 + 1];
            r += weight * source_buffer[i * 4 + 2];
#ifdef ScaleAlpha
            a += weight * source_buffer[i * 4 + 3];
#endif
        }

        dest_buffer[ndx * 4] = b;
        dest_buffer[ndx * 4 + 1] = g;
        dest_buffer[ndx * 4 + 2] = r;
#ifdef ScaleAlpha
        dest_buffer[ndx * 4 + 3] = a;
#endif   
    }

}


static inline void ScaleXAndPivotRow(unsigned char * source_row, unsigned int source_pixel_count, ContributionType * weights, BitmapBgraPtr dest, unsigned int dest_column_index, float *source_buffer, unsigned int source_buffer_len, float *dest_buffer, unsigned int dest_buffer_len, float *lut){
    unsigned int bix;
   //This copy seems responsible for about 8% of runtime
   for (bix = 0; bix < source_buffer_len; bix++){
        source_buffer[bix] = lut[source_row[bix]];
    }
    
   //Actual scaling seems responsible for about 40% of execution time
   ScaleBgraFloat(source_buffer, source_pixel_count, source_buffer_len, dest_buffer, dest->h, dest_buffer_len, weights);


   //This copy seems responsible for about 12% of runtime
    for (bix = 0; bix < dest->h; bix++){

#ifdef ScaleAlpha
        dest->pixelInts[bix * dest->w + dest_column_index] = gdTrueColorAlpha(
            uchar_clamp(dest_buffer[bix * 4], 0xFF),
            uchar_clamp(dest_buffer[bix * 4 + 1], 0xFF),
            uchar_clamp(dest_buffer[bix * 4 + 2], 0xFF),
            uchar_clamp(dest_buffer[bix * 4 + 3], 0xFF)); /* alpha is 0..255 */
#endif
#ifndef ScaleAlpha
        dest->pixelInts[bix * dest->w + dest_column_index] = gdTrueColorAlpha(
            uchar_clamp(dest_buffer[bix * 4], 0xFF),
            uchar_clamp(dest_buffer[bix * 4 + 1], 0xFF),
            uchar_clamp(dest_buffer[bix * 4 + 2], 0xFF),0xFF); 
#endif
    }
}


static inline void ScaleXAndPivotRows(BitmapBgraPtr source_bitmap, unsigned int start_row, unsigned int row_count,  ContributionType * weights, BitmapBgraPtr dest, float *source_buffers, unsigned int source_buffer_len, float *dest_buffers, unsigned int dest_buffer_len, float *lut){

    unsigned char * scan_start = source_bitmap->pixels + start_row * source_bitmap->stride;


    unsigned int bix, bufferSet;
    
    for (bix = 0; bix < source_buffer_len * row_count; bix++){
        source_buffers[bix] = lut[scan_start[bix]];
    }
    
    //Actual scaling seems responsible for about 40% of execution time
    for (bufferSet = 0; bufferSet < row_count; bufferSet++){
        ScaleBgraFloat(source_buffers + (source_buffer_len * bufferSet), source_bitmap->w, source_buffer_len, dest_buffers + (dest_buffer_len * bufferSet), dest->h, dest_buffer_len, weights);
    }
    
    for (bix = 0; bix < dest->h; bix++){
        for (bufferSet = 0; bufferSet < row_count; bufferSet++){
    #ifdef ScaleAlpha
            dest->pixelInts[bix * dest->w + start_row + bufferSet] = gdTrueColorAlpha(
                uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4], 0xFF),
                uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4 + 1], 0xFF),
                uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4 + 2], 0xFF), 
                uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4 + 3], 0xFF));
    #endif
    #ifndef ScaleAlpha
            dest->pixelInts[bix * dest->w + start_row + bufferSet] = gdTrueColorAlpha(
                  uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4], 0xFF),
                  uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4 + 1], 0xFF),
                  uchar_clamp(dest_buffers[bufferSet * dest_buffer_len + bix * 4 + 2], 0xFF), 0xFF);
    #endif
        }
    }
}


static inline int ScaleXAndPivot(const BitmapBgraPtr pSrc,
    const BitmapBgraPtr pDst, float *lut)
{
    unsigned int line_ndx;
    LineContribType * contrib;

    contrib = ContributionsCalc(pDst->h, pSrc->w,
                        (double)pDst->h / (double)pSrc->w, filter_bicubic);
    if (contrib == NULL) {
        return 0;
    }

    int buffer = 1; //using buffer=5 seems about 6% better than most other non-zero values. 


    unsigned int source_buffer_len = pSrc->stride;
    float *sourceBuffers = (float *)malloc(sizeof(float) * source_buffer_len * buffer);

    unsigned int dest_buffer_len = pSrc->h * 4;
    float *destBuffers = (float *)malloc(sizeof(float) * dest_buffer_len * buffer);


    if (buffer == 1){
        for (line_ndx = 0; line_ndx < pSrc->h; line_ndx++) {

            ScaleXAndPivotRow(pSrc->pixels + line_ndx * pSrc->stride, pSrc->w, contrib->ContribRow, pDst, line_ndx,
                sourceBuffers, source_buffer_len, destBuffers, dest_buffer_len, lut);
        }
    }
    else{
        /* Scale each line */
        for (line_ndx = 0; line_ndx < pSrc->h; line_ndx += buffer) {

            ScaleXAndPivotRows(pSrc, line_ndx, MIN(pSrc->h - line_ndx, buffer), contrib->ContribRow, pDst,
                 sourceBuffers, source_buffer_len, destBuffers, dest_buffer_len, lut);
        }
    }

    free(sourceBuffers);
    free(destBuffers);
    
    ContributionsFree(contrib);

    return 1;
}/* _gdScalePass*/


static void unpack24bitRow(int width, void * sourceLine, unsigned int * destArray){
	for (int i = 0; i < width; i++){
        destArray[i] = (*(unsigned int *)((unsigned long  long)sourceLine + (i * 3))) | 0xFF000000;
	}
}




#pragma managed


using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;
using namespace ImageResizer::Resizing;
using namespace System::Diagnostics;
namespace ImageResizer{
	namespace Plugins{
		namespace FastScaling {

			public ref class BitmapScaler
			{
			public:
				void ScaleBitmap(Bitmap^ source, Bitmap^ dest, Rectangle crop, Rectangle target, IProfiler^ p){
					BitmapBgraPtr bbSource;
                    BitmapBgraPtr bbResult;
					try{
                        p->Start("FromGDI+",false);
                        bbSource = FromGDI(source, crop);
                        p->Stop("FromGDI+", true, false);
                        p->Start("Scale", false);
                        bbResult = Scale(bbSource, target.Width, target.Height, p);
                        p->Stop("Scale", true, false);
                        p->Start("ToGDI+", false);
                        ToGDI(bbResult, dest, target);
                        p->Stop("ToGDI+", true, false);
                        p->Start("GdDispose", false);
					}finally{
                        if (bbSource != 0) {
                            DestroyBitmapBgra(bbSource);
                            bbSource = 0;
						}
                        if (bbResult != 0){
                            DestroyBitmapBgra(bbResult);
                            bbResult = 0;
						}
                        p->Stop("GdDispose", true, false);

					}
				}
				 

			private:

                BitmapBgraPtr Scale(BitmapBgraPtr source, int width, int height, IProfiler^ p){

                    BitmapBgraPtr tmp_im = NULL;
                    BitmapBgraPtr dst = NULL;


                    float lut[256];
                    for (int n = 0; n < 256; n++) lut[n] = (float)n;

                    p->Start("create temp image(sy x dx)", false);
                    /* Scale horizontally  */
                    tmp_im = CreateBitmapBgraPtr(source->h, width,false);
                   
                    if (tmp_im == NULL) {
                        return NULL;
                    }
                    try{
                        p->Stop("create temp image(sy x dx)", true, false);

                        p->Start("scale and pivot to temp", false);
                        ScaleXAndPivot(source, tmp_im, lut);
                        p->Stop("scale and pivot to temp", true, false);

                        p->Start("create image(dx x dy)", false);
                        /* Otherwise, we need to scale vertically. */
                        dst = CreateBitmapBgraPtr(width, height,false);
                        p->Stop("create image(dx x dy)", true, false);
                        if (dst == NULL) {
                            return NULL;
                        }

                        p->Start("scale and pivot to final", false);
                        ScaleXAndPivot(tmp_im, dst, lut);
                        p->Stop("scale and pivot to final", true, false);
                    }
                    finally{
                        p->Start("destroy temp image", false);
                        DestroyBitmapBgra(tmp_im);
                        p->Stop("destroy temp image", true, false);
                    }
                    return dst;
				}

                void ToGDI(BitmapBgraPtr source, Bitmap^ target, Rectangle targetArea){
					if (target->PixelFormat != PixelFormat::Format32bppArgb){
						throw gcnew ArgumentOutOfRangeException("target", "Invalid pixel format " + target->PixelFormat.ToString());
					}
                    BitmapData ^targetData;
					try{
						targetData = target->LockBits(targetArea, ImageLockMode::ReadOnly, target->PixelFormat);
						int sy = source->h;
						int sx = source->w;
						int i;
                        IntPtr^ scan0intptr = targetData->Scan0;
						void *scan0 = scan0intptr->ToPointer();
						for (i = 0; (i < sy); i++) {
                            void * linePtr = (void *)((unsigned long  long)scan0 + (targetData->Stride * i) + (targetArea.Left * 4));
							memcpy(linePtr, &source->pixels[i * source->stride], sx * 4);
						}
					}
					finally{
                        target->UnlockBits(targetData);
					}
				}

                BitmapBgraPtr FromGDI(Bitmap^ source, Rectangle from){
					int i;
					int j;
					bool hasAlpha = source->PixelFormat == PixelFormat::Format32bppArgb;
					if (source->PixelFormat != PixelFormat::Format32bppArgb && source->PixelFormat != PixelFormat::Format24bppRgb){
						throw gcnew ArgumentOutOfRangeException("source", "Invalid pixel format " + source->PixelFormat.ToString());
					}
					if (from.X < 0 || from.Y < 0 || from.Right > source->Width || from.Bottom > source->Height) {
						throw gcnew ArgumentOutOfRangeException("from");
					}
					int sx = from.Width;
					int sy = from.Height;

					int mask = ((INT_MAX >> 8) << 8);
					BitmapBgraPtr im = CreateBitmapBgraPtr(sx, sy,false);
					
					BitmapData ^sourceData;
					try{
						sourceData = source->LockBits(from, ImageLockMode::ReadWrite, source->PixelFormat);

						for (i = 0; (i < sy); i++) {
							IntPtr^ scan0intptr = sourceData->Scan0;

							void *scan0 = scan0intptr->ToPointer();
							void *linePtr = (void *)((unsigned long long)scan0 + (unsigned long  long)((sourceData->Stride * i) + (from.Left * (hasAlpha ? 4 : 3))));
							if (hasAlpha){
								memcpy(&im->pixels[i * im->stride], linePtr, sx * 4);
							}
							else{
								unpack24bitRow(sx, linePtr, (unsigned int*)&im->pixels[i * im->stride]);
							}
						}
					}
					finally{
						source->UnlockBits(sourceData);
					}
					im->w = sx;
					im->h = sy;

                    im->hasAlpha = hasAlpha;
					return im;
				}
			};


			public ref class FastScalingPlugin : public ImageResizer::Resizing::BuilderExtension, IPlugin
			{
			protected:
				virtual RequestedAction InternalGraphicsDrawImage(ImageState^ s, Bitmap^ dest, Bitmap^ source, array<PointF>^ targetArea, RectangleF sourceArea, ImageAttributes^ imageAttributes) override{
				/*	System::Collections::Specialized::NameValueCollection^ query = safe_cast<System::Collections::Specialized::NameValueCollection^>(s->settings);

                
					String^ fastScale = query->Get("fastscale");
					String^ sTrue = "true";
					if (fastScale != sTrue){
						return RequestedAction::None;
					}*/

                    
					RectangleF targetBox = ImageResizer::Util::PolygonMath::GetBoundingBox(targetArea);
					if (targetBox.Location != targetArea[0] || targetBox.Width != (targetArea[1].X - targetArea[0].X)){
						return RequestedAction::None;
					}
					BitmapScaler ^scaler = gcnew BitmapScaler();
                    scaler->ScaleBitmap(source, dest, Util::PolygonMath::ToRectangle(sourceArea), Util::PolygonMath::ToRectangle(targetBox), s->Job->Profiler);
					return RequestedAction::Cancel;
					
				}
			public: 
				virtual ImageResizer::Plugins::IPlugin^ Install(ImageResizer::Configuration::Config^ c) override{
					c->Plugins->add_plugin(this);
					return this;
				}
				virtual bool Uninstall(ImageResizer::Configuration::Config^ c) override{
					c->Plugins->remove_plugin(this);
					return true;
				}
			};
			
		}
	}
}