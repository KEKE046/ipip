#include<implot.h>

namespace ImPlot{
    template <typename T> IMPLOT_API void PlotHeatmapTranspose(const char* label_id, const T* values, int rows, int cols, double scale_min=0, double scale_max=0, const char* label_fmt="%.1f", const ImPlotPoint& bounds_min=ImPlotPoint(0,0), const ImPlotPoint& bounds_max=ImPlotPoint(1,1));
}