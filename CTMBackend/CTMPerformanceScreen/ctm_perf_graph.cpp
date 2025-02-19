#include "ctm_perf_graph.h"

//----------------------------------------PLOTTING FUNCTIONS----------------------------------------
template<std::size_t NumOfPlots, typename PlotType>
void CTMPerformanceUsageGraph<NumOfPlots, PlotType>::PlotUsageGraph(const char* plotLabel, double minPlotLimit, double maxPlotLimit,
    const ImVec2& plotSize, const ImVec4& plotColor)
{
    if(ImPlot::BeginPlot(plotLabel, plotSize, ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);

        ImPlot::SetupAxisLimits(ImAxis_X1, xAxisValue - graphDuration, xAxisValue, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, minPlotLimit, maxPlotLimit, ImPlotCond_Always);

        ImPlot::SetNextFillStyle(plotColor, 0.5f);
        ImPlot::SetNextLineStyle(plotColor, 0.5f);

        ImPlot::PlotShaded("", &plotBuffers[0].Data[0].x, &plotBuffers[0].Data[0].y, plotBuffers[0].Data.size(),
                            0, 0, plotBuffers[0].Offset, 2 * sizeof(PlotType));
        ImPlot::PlotLine("", &plotBuffers[0].Data[0].x, &plotBuffers[0].Data[0].y, plotBuffers[0].Data.size(),
                            0, plotBuffers[0].Offset, 2 * sizeof(PlotType));
        
        ImPlot::EndPlot();
    }
}

template<std::size_t NumOfPlots, typename PlotType>
void CTMPerformanceUsageGraph<NumOfPlots, PlotType>::PlotMultiUsageGraph(const char* plotLabel, const char* plotShadedLabel,
    const char* plotLineLabel, double minPlotLimit, double maxPlotLimit, const ImVec2& plotSize, const ImVec4 plotColors[NumOfPlots])
{   
    if(ImPlot::BeginPlot(plotLabel, plotSize, ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);

        ImPlot::SetupAxisLimits(ImAxis_X1, xAxisValue - graphDuration, xAxisValue, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, minPlotLimit, maxPlotLimit, ImPlotCond_Always);

        //Rn at max there will be two plots only, so make one line and one shaded
        //1) 0th index is shaded
        ImPlot::SetNextFillStyle(plotColors[0], 0.5f);
        ImPlot::SetNextLineStyle(plotColors[0], 0.5f);
        ImPlot::PlotShaded(plotShadedLabel, &plotBuffers[0].Data[0].x, &plotBuffers[0].Data[0].y, plotBuffers[0].Data.size(),
                            0, 0, plotBuffers[0].Offset, 2 * sizeof(PlotType));
        ImPlot::PlotLine(plotShadedLabel, &plotBuffers[0].Data[0].x, &plotBuffers[0].Data[0].y, plotBuffers[0].Data.size(),
                            0, plotBuffers[0].Offset, 2 * sizeof(PlotType));

        //2) 1st index is line
        ImPlot::SetNextLineStyle(plotColors[1], 0.5f);
        ImPlot::PlotLine(plotLineLabel, &plotBuffers[1].Data[0].x, &plotBuffers[1].Data[0].y, plotBuffers[1].Data.size(),
                            0, plotBuffers[1].Offset, 2 * sizeof(PlotType));
        
        ImPlot::EndPlot();
    }
}

//Because of how templated functions work around files (aka they don't work at all)
//We need to pre initiate the template building process (if it makes any sense)
//Aka we need to pre declare templates for the compiler to build a copy of those stuff so they can be used in other files
template class CTMPerformanceUsageGraph<1, double>;
template class CTMPerformanceUsageGraph<2, double>;