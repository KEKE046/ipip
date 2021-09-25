#include<implot.h>
#include<implot_internal.h>

#ifndef IMPLOT_NO_FORCE_INLINE
    #ifdef _MSC_VER
        #define IMPLOT_INLINE __forceinline
    #elif defined(__GNUC__)
        #define IMPLOT_INLINE inline __attribute__((__always_inline__))
    #elif defined(__CLANG__)
        #if __has_attribute(__always_inline__)
            #define IMPLOT_INLINE inline __attribute__((__always_inline__))
        #else
            #define IMPLOT_INLINE inline
        #endif
    #else
        #define IMPLOT_INLINE inline
    #endif
#else
    #define IMPLOT_INLINE inline
#endif

namespace ImPlot
{

    struct TransformerLin {
        TransformerLin(double pixMin, double pltMin, double,       double m, double    ) : PixMin(pixMin), PltMin(pltMin), M(m) { }
        template <typename T> IMPLOT_INLINE float operator()(T p) const { return (float)(PixMin + M * (p - PltMin)); }
        double PixMin, PltMin, M;
    };

    struct TransformerLog {
        TransformerLog(double pixMin, double pltMin, double pltMax, double m, double den) : Den(den), PltMin(pltMin), PltMax(pltMax), PixMin(pixMin), M(m) { }
        template <typename T> IMPLOT_INLINE float operator()(T p) const {
            p = p <= 0.0 ? IMPLOT_LOG_ZERO : p;
            double t = ImLog10(p / PltMin) / Den;
            p = ImLerp(PltMin, PltMax, (float)t);
            return (float)(PixMin + M * (p - PltMin));
        }
        double Den, PltMin, PltMax, PixMin, M;
    };

    template <typename TransformerX, typename TransformerY>
    struct TransformerXY {
        TransformerXY() :
            YAxis(GetCurrentYAxis()),
            Tx(GImPlot->PixelRange[YAxis].Min.x, GImPlot->CurrentPlot->XAxis.Range.Min,        GImPlot->CurrentPlot->XAxis.Range.Max,        GImPlot->Mx,        GImPlot->LogDenX),
            Ty(GImPlot->PixelRange[YAxis].Min.y, GImPlot->CurrentPlot->YAxis[YAxis].Range.Min, GImPlot->CurrentPlot->YAxis[YAxis].Range.Max, GImPlot->My[YAxis], GImPlot->LogDenY[YAxis])
        { }
        template <typename P> IMPLOT_INLINE ImVec2 operator()(const P& plt) const {
            ImVec2 out;
            out.x = Tx(plt.x);
            out.y = Ty(plt.y);
            return out;
        }
        int YAxis;
        TransformerX Tx;
        TransformerY Ty;
    };

    typedef TransformerXY<TransformerLin,TransformerLin> TransformerLinLin;
    typedef TransformerXY<TransformerLin,TransformerLog> TransformerLinLog;
    typedef TransformerXY<TransformerLog,TransformerLin> TransformerLogLin;
    typedef TransformerXY<TransformerLog,TransformerLog> TransformerLogLog;

    struct RectInfo {
        ImPlotPoint Min, Max;
        ImU32 Color;
    };
    
    template <typename T>
    struct MaxIdx_ { static const unsigned int Value; };
    template <> const unsigned int MaxIdx_<unsigned short>::Value = 65535;
    template <> const unsigned int MaxIdx_<unsigned int>::Value   = 4294967295;

    template <typename TGetter, typename TTransformer>
    struct RectRenderer {
        IMPLOT_INLINE RectRenderer(const TGetter& getter, const TTransformer& transformer) :
            Getter(getter),
            Transformer(transformer),
            Prims(Getter.Count)
        {}
        IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
            RectInfo rect = Getter(prim);
            ImVec2 P1 = Transformer(rect.Min);
            ImVec2 P2 = Transformer(rect.Max);

            if ((rect.Color & IM_COL32_A_MASK) == 0 || !cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2))))
                return false;

            DrawList._VtxWritePtr[0].pos   = P1;
            DrawList._VtxWritePtr[0].uv    = uv;
            DrawList._VtxWritePtr[0].col   = rect.Color;
            DrawList._VtxWritePtr[1].pos.x = P1.x;
            DrawList._VtxWritePtr[1].pos.y = P2.y;
            DrawList._VtxWritePtr[1].uv    = uv;
            DrawList._VtxWritePtr[1].col   = rect.Color;
            DrawList._VtxWritePtr[2].pos   = P2;
            DrawList._VtxWritePtr[2].uv    = uv;
            DrawList._VtxWritePtr[2].col   = rect.Color;
            DrawList._VtxWritePtr[3].pos.x = P2.x;
            DrawList._VtxWritePtr[3].pos.y = P1.y;
            DrawList._VtxWritePtr[3].uv    = uv;
            DrawList._VtxWritePtr[3].col   = rect.Color;
            DrawList._VtxWritePtr += 4;
            DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
            DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
            DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
            DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
            DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
            DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
            DrawList._IdxWritePtr   += 6;
            DrawList._VtxCurrentIdx += 4;
            return true;
        }
        const TGetter& Getter;
        const TTransformer& Transformer;
        const int Prims;
        static const int IdxConsumed = 6;
        static const int VtxConsumed = 4;
    };


    template <typename Renderer>
    IMPLOT_INLINE void RenderPrimitives(const Renderer& renderer, ImDrawList& DrawList, const ImRect& cull_rect) {
        unsigned int prims        = renderer.Prims;
        unsigned int prims_culled = 0;
        unsigned int idx          = 0;
        const ImVec2 uv = DrawList._Data->TexUvWhitePixel;
        while (prims) {
            // find how many can be reserved up to end of current draw command's limit
            unsigned int cnt = ImMin(prims, (MaxIdx_<ImDrawIdx>::Value - DrawList._VtxCurrentIdx) / Renderer::VtxConsumed);
            // make sure at least this many elements can be rendered to avoid situations where at the end of buffer this slow path is not taken all the time
            if (cnt >= ImMin(64u, prims)) {
                if (prims_culled >= cnt)
                    prims_culled -= cnt; // reuse previous reservation
                else {
                    DrawList.PrimReserve((cnt - prims_culled) * Renderer::IdxConsumed, (cnt - prims_culled) * Renderer::VtxConsumed); // add more elements to previous reservation
                    prims_culled = 0;
                }
            }
            else
            {
                if (prims_culled > 0) {
                    DrawList.PrimUnreserve(prims_culled * Renderer::IdxConsumed, prims_culled * Renderer::VtxConsumed);
                    prims_culled = 0;
                }
                cnt = ImMin(prims, (MaxIdx_<ImDrawIdx>::Value - 0/*DrawList._VtxCurrentIdx*/) / Renderer::VtxConsumed);
                DrawList.PrimReserve(cnt * Renderer::IdxConsumed, cnt * Renderer::VtxConsumed); // reserve new draw command
            }
            prims -= cnt;
            for (unsigned int ie = idx + cnt; idx != ie; ++idx) {
                if (!renderer(DrawList, cull_rect, uv, idx))
                    prims_culled++;
            }
        }
        if (prims_culled > 0)
            DrawList.PrimUnreserve(prims_culled * Renderer::IdxConsumed, prims_culled * Renderer::VtxConsumed);
    }

    template <typename T>
    struct GetterHeatmapTranspose {
        GetterHeatmapTranspose(const T* values, int rows, int cols, double scale_min, double scale_max, double width, double height, double xref, double yref, double ydir) :
            Values(values),
            Count(rows*cols),
            Rows(rows),
            Cols(cols),
            ScaleMin(scale_min),
            ScaleMax(scale_max),
            Width(width),
            Height(height),
            XRef(xref),
            YRef(yref),
            YDir(ydir),
            HalfSize(Width*0.5, Height*0.5)
        { }

        template <typename I> IMPLOT_INLINE RectInfo operator()(I idx) const {
            double val = (double)Values[idx];
            const int r = idx % Rows;
            const int c = idx / Rows;
            const ImPlotPoint p(XRef + HalfSize.x + c*Width, YRef + YDir * (HalfSize.y + r*Height));
            RectInfo rect;
            rect.Min.x = p.x - HalfSize.x;
            rect.Min.y = p.y - HalfSize.y;
            rect.Max.x = p.x + HalfSize.x;
            rect.Max.y = p.y + HalfSize.y;
            const float t = ImClamp((float)ImRemap01(val, ScaleMin, ScaleMax),0.0f,1.0f);
            rect.Color = GImPlot->ColormapData.LerpTable(GImPlot->Style.Colormap, t);
            return rect;
        }
        const T* const Values;
        const int Count, Rows, Cols;
        const double ScaleMin, ScaleMax, Width, Height, XRef, YRef, YDir;
        const ImPlotPoint HalfSize;
    };

    template <typename T, typename Transformer>
    void RenderHeatmapTranspose(Transformer transformer, ImDrawList& DrawList, const T* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max, bool reverse_y) {
        ImPlotContext& gp = *GImPlot;
        if (scale_min == 0 && scale_max == 0) {
            T temp_min, temp_max;
            ImMinMaxArray(values,rows*cols,&temp_min,&temp_max);
            scale_min = (double)temp_min;
            scale_max = (double)temp_max;
        }
        if (scale_min == scale_max) {
            ImVec2 a = transformer(bounds_min);
            ImVec2 b = transformer(bounds_max);
            ImU32  col = GetColormapColorU32(0,gp.Style.Colormap);
            DrawList.AddRectFilled(a, b, col);
            return;
        }
        const double yref = reverse_y ? bounds_max.y : bounds_min.y;
        const double ydir = reverse_y ? -1 : 1;
        GetterHeatmapTranspose<T> getter(values, rows, cols, scale_min, scale_max, (bounds_max.x - bounds_min.x) / cols, (bounds_max.y - bounds_min.y) / rows, bounds_min.x, yref, ydir);
        switch (GetCurrentScale()) {
            case ImPlotScale_LinLin: RenderPrimitives(RectRenderer<GetterHeatmapTranspose<T>, TransformerLinLin>(getter, TransformerLinLin()), DrawList, gp.CurrentPlot->PlotRect); break;
            case ImPlotScale_LogLin: RenderPrimitives(RectRenderer<GetterHeatmapTranspose<T>, TransformerLogLin>(getter, TransformerLogLin()), DrawList, gp.CurrentPlot->PlotRect); break;;
            case ImPlotScale_LinLog: RenderPrimitives(RectRenderer<GetterHeatmapTranspose<T>, TransformerLinLog>(getter, TransformerLinLog()), DrawList, gp.CurrentPlot->PlotRect); break;;
            case ImPlotScale_LogLog: RenderPrimitives(RectRenderer<GetterHeatmapTranspose<T>, TransformerLogLog>(getter, TransformerLogLog()), DrawList, gp.CurrentPlot->PlotRect); break;;
        }
        if (fmt != NULL) {
            const double w = (bounds_max.x - bounds_min.x) / cols;
            const double h = (bounds_max.y - bounds_min.y) / rows;
            const ImPlotPoint half_size(w*0.5,h*0.5);
            int i = 0;
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    ImPlotPoint p;
                    p.x = bounds_min.x + 0.5*w + c*w;
                    p.y = yref + ydir * (0.5*h + r*h);
                    ImVec2 px = transformer(p);
                    char buff[32];
                    sprintf(buff, fmt, values[i]);
                    ImVec2 size = ImGui::CalcTextSize(buff);
                    double t = ImClamp(ImRemap01((double)values[i], scale_min, scale_max),0.0,1.0);
                    ImVec4 color = SampleColormap((float)t);
                    ImU32 col = CalcTextColor(color);
                    DrawList.AddText(px - size * 0.5f, col, buff);
                    i++;
                }
            }
        }
    }

    template <typename T>
    void PlotHeatmapTranspose(const char* label_id, const T* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max) {
        if (BeginItem(label_id)) {
            if (FitThisFrame()) {
                FitPoint(bounds_min);
                FitPoint(bounds_max);
            }
            ImDrawList& DrawList = *GetPlotDrawList();
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderHeatmapTranspose(TransformerLinLin(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
                case ImPlotScale_LogLin: RenderHeatmapTranspose(TransformerLogLin(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
                case ImPlotScale_LinLog: RenderHeatmapTranspose(TransformerLinLog(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
                case ImPlotScale_LogLog: RenderHeatmapTranspose(TransformerLogLog(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
            }
            EndItem();
        }
    }

    template IMPLOT_API void PlotHeatmapTranspose<ImS8>(const char* label_id, const ImS8* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImU8>(const char* label_id, const ImU8* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImS16>(const char* label_id, const ImS16* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImU16>(const char* label_id, const ImU16* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImS32>(const char* label_id, const ImS32* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImU32>(const char* label_id, const ImU32* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImS64>(const char* label_id, const ImS64* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<ImU64>(const char* label_id, const ImU64* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<float>(const char* label_id, const float* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
    template IMPLOT_API void PlotHeatmapTranspose<double>(const char* label_id, const double* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);

} // namespace ImPlot
