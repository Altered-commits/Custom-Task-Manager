#ifndef CTM_PERFORMANCE_USAGE_GRAPH_HPP
#define CTM_PERFORMANCE_USAGE_GRAPH_HPP

//ImGui and Implot stuff
#include "../../ImGUI/imgui.h"
#include "../../ImPlot/implot.h"
//Stdlib stuff
#include <vector>

//Used for plotting stuff while moving the graph (like animating the graph left to right / right to left, whatever)
//Copied it from ImPlot -> implot_demo.cpp
struct ScrollingBuffer
{
    std::vector<ImVec2> Data = { {0, 0} }; //Have a value initially, or u would be accessing value out of bounds INITIALLY ofc
    int                 MaxSize;
    int                 Offset;

    ScrollingBuffer(int max_size = 2000)
    {
        MaxSize = max_size;
        Offset  = 0;
        Data.reserve(MaxSize);
    }

    void AddPoint(float x, float y)
    {
        if (Data.size() < MaxSize)
            Data.emplace_back(x, y);
        else
        {
            Data[Offset] = ImVec2(x, y);
            Offset       = (Offset + 1) % MaxSize;
        }
    }
};

//Its simply a struct to be inherited by 'screen' classes (aka (pages/screens) like Cpu Usage, etc)
//It contains function and variables to plot graph which is common to all screens
struct CTMPerformanceUsageGraph
{
protected: //Explicitly typing it makes my OCD go away
    void PlotUsageGraph(const char* plotLabel, const ImVec2& plotSize, float minPlotLimit, float maxPlotLimit, const ImVec4& plotColor)
    {
        if(ImPlot::BeginPlot(plotLabel, plotSize, ImPlotFlags_NoInputs))
        {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);

            ImPlot::SetupAxisLimits(ImAxis_X1, xAxisValue - graphDuration, xAxisValue, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, minPlotLimit, maxPlotLimit, ImPlotCond_Always);

            ImPlot::SetNextFillStyle(plotColor, 0.5f);
            ImPlot::SetNextLineStyle(plotColor, 0.5f);

            ImPlot::PlotShaded("", &plotBuffer.Data[0].x, &plotBuffer.Data[0].y, plotBuffer.Data.size(), 0, 0, plotBuffer.Offset, 2 * sizeof(float));
            ImPlot::PlotLine("", &plotBuffer.Data[0].x, &plotBuffer.Data[0].y, plotBuffer.Data.size(), 0, plotBuffer.Offset, 2 * sizeof(float));  
            
            ImPlot::EndPlot();
        }
    }
    
    void  UpdateXAxis()               { xAxisValue += xAxisValueOffset; }
    float GetCurrentXAxisValue()      { return xAxisValue; }
    void  PlotPoint(float x, float y) { plotBuffer.AddPoint(x, y); }

private: //I don't want these variables to accidentally get modified in any way other than the method specified by functions
    //These are hardcoded as all the screens will be having same duration of graph and buffer size
    constexpr static int graphDuration    = 60;
    float                xAxisValue       = 0.0f;
    float                xAxisValueOffset = 1.0f;
    ScrollingBuffer      plotBuffer{graphDuration + 1};
};

#endif