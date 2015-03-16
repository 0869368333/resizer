/*
 * Copyright (c) Imazen LLC.
 * No part of this project, including this file, may be copied, modified,
 * propagated, or distributed except as permitted in COPYRIGHT.txt.
 * Licensed under the GNU Affero General Public License, Version 3.0.
 * Commercial licenses available at http://imageresizing.net/
 */

#include "fastscaling.h"
#pragma once

#ifdef _MSC_VER
#pragma managed
#endif


using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;
using namespace ImageResizer::Resizing;
using namespace System::Diagnostics;
using namespace System::Collections::Specialized;
using namespace System::Runtime::InteropServices;

namespace ImageResizer{
    namespace Plugins{
        namespace FastScaling {

            public ref class ManagedRenderer
            {

                float* CopyFloatArray(array<float, 1>^ a){
                    if (a == nullptr) return NULL;

                    float * copy = (float *)ir_malloc(sizeof(float) * a->Length);
                    if (copy == NULL) throw gcnew OutOfMemoryException();
                    for (int i = 0; i < a->Length; i++)
                        copy[i] = a[i];
                    return copy;
                }

                void CopyBasics(RenderOptions^ from, RenderDetails* to){
                    if (from->ColorMatrix != nullptr)
                    {
                        for (int i = 0; i < 5; i++)
                            for (int j = 0; j < 5; j++)
                                to->color_matrix_data[i * 5 + j] = (float)((array<float,1>^)from->ColorMatrix->GetValue(i))->GetValue(j);

                        to->apply_color_matrix = true;
                    }
                    to->post_transpose = from->RequiresTransposeStep;
                    to->post_flip_x = from->RequiresHorizontalFlipStep;
                    to->post_flip_y = from->RequiresVerticalFlipStep;
                    to->halve_only_when_common_factor = from->HalveOnlyWhenPerfect;
                    to->sharpen_percent_goal = from->SharpeningPercentGoal;
                    to->kernel_a_min = from->ConvolutionA_MinChangeThreshold;
                    to->kernel_a_max = from->ConvolutionA_MaxChangeThreshold;
                    to->kernel_b_min = from->ConvolutionB_MinChangeThreshold;
                    to->kernel_b_max = from->ConvolutionB_MaxChangeThreshold;
                    to->kernel_a = CopyFloatArray(from->ConvolutionA);
                    to->kernel_b = CopyFloatArray(from->ConvolutionB);
                    to->interpolate_last_percent = from->InterpolateLastPercent;


                    if (to->interpolation != nullptr){
                        free(to->interpolation);
                        to->interpolation = nullptr;
                    }

                    to->interpolation = create_interpolation(from->Filter);
                    if (to->interpolation == nullptr) throw gcnew ArgumentOutOfRangeException("Invalid filter value");
                    to->interpolation->blur *= from->SamplingBlurFactor;
                    if (from->SamplingWindowOverride != 0) {
                        to->interpolation->window = from->SamplingWindowOverride;
                    }
                    to->minimum_sample_window_to_interposharpen = from->MinSamplingWindowToIntegrateSharpening;


                }


                ~ManagedRenderer(){
                    if (p != nullptr) p->Start("Renderer: dispose", false);
                    destroy_renderer(r);

                    if (wbSource != nullptr){
                        delete wbSource;
                        wbSource = nullptr;
                    }
                    if (wbCanvas != nullptr){
                        delete wbCanvas;
                        wbCanvas = nullptr;
                    }

                    if (p != nullptr) p->Stop("Renderer: dispose", true, false);
                }
                Renderer* r;
                RenderOptions^ originalOptions;
                WrappedBitmap^ wbSource;
                WrappedBitmap^ wbCanvas;
                IProfiler^ p;

            public:


                ManagedRenderer(BitmapOptions^ editInPlace, RenderOptions^ opts, IProfiler^ p){
                    this->p = p;
                    originalOptions = opts;


                    if (opts->RequiresTransposeStep) throw gcnew ArgumentException("Cannot transpose image in place.");


                    RenderDetails* details = create_render_details();
                    CopyBasics(opts, details);
                    p->Start("SysDrawingToBgra", false);
                    wbSource = gcnew WrappedBitmap(editInPlace);
                    p->Stop("SysDrawingToBgra", true, false);

                }

                ManagedRenderer(BitmapOptions^ source, BitmapOptions^ canvas, RenderOptions^ opts, IProfiler^ p){

                    this->p = p;
                    originalOptions = opts;
                    RenderDetails* details = create_render_details();
                    CopyBasics(opts, details);
                    p->Start("SysDrawingToBgra", false);
                    wbSource = gcnew WrappedBitmap(source);
                    wbCanvas = gcnew WrappedBitmap(canvas);
                    p->Stop("SysDrawingToBgra", true, false);

                    r = create_renderer(wbSource->bgra,wbCanvas->bgra, details);
                }


                void Render(){
                    perform_render(r);
                }

            };

        }
    }
}
