#ifndef CTM_PERFORMANCE_USAGE_GRAPH_HPP
#define CTM_PERFORMANCE_USAGE_GRAPH_HPP

//Using std::max instead
#undef max

//My stuff
#include "../ctm_logger.h"
//ImGui and Implot stuff
#include "../../ImGUI/imgui.h"
#include "../../ImPlot/implot.h"
//Stdlib stuff
#include <vector>
#include <algorithm>

//'using' makes my life alot easi-
template<typename T>
using PlotBufferVector = std::vector<T>; //NOTE: Might change this to 'std::array' in future

//Custom struct to hold x and y values of type 'T'
template<typename T>
struct CTMPoint
{
public:
    CTMPoint()         : x(0), y(0) {}
    CTMPoint(T x, T y) : x(x), y(y) {}

public:
    T x, y;
};

//Fixed size buffer which will keep scrolling (aka if we reach the end, then we start from the beginning overrding the previous values)
//Copied it from ImPlot -> implot_demo.cpp
template<typename T>
struct CTMScrollingBuffer
{
//Making sections of code is always good no matter how explicit you gotta be
public:
    //Have a value initially, or u would be accessing value out of bounds, INITIALLY ofc
    PlotBufferVector<CTMPoint<T>> Data = { {0, 0} };
    std::size_t                   MaxSize;
    std::size_t                   Offset;

public:
    CTMScrollingBuffer(std::size_t maxSizeIn = 61)
    {
        MaxSize = maxSizeIn;
        Offset  = 0;
        Data.reserve(MaxSize);
    }

    void AddPoint(T x, T y)
    {
        if(Data.size() < MaxSize)
            Data.emplace_back(x, y);
        else
        {
            Data[Offset] = CTMPoint<T>(x, y);
            Offset       = (Offset + 1) % MaxSize;
        }
    }

    T GetMaxYValue()
    {
        return std::max_element(Data.begin(), Data.end(),
                    [](CTMPoint<T>& left, CTMPoint<T>& right){ return left.y < right.y; })->y;
    }
};

//Its simply a class to be inherited by 'screen' classes (aka (pages/screens) like Cpu Usage, etc)
//It contains function and variables to plot graph which is common to all screens
//Templating it as it maybe used for plotting multiple plots on a single graph.
template<std::size_t NumOfPlots, typename PlotType>
class CTMPerformanceUsageGraph
{
    //Right now i only allow either 1 or 2 plots at max
    static_assert(NumOfPlots == 1 || NumOfPlots == 2, "Max number of plots supported at this time are 1 or 2 only.");
    //Simple static assert to check if PlotType is floating point compatible. Other types are not allowed
    static_assert(std::is_floating_point_v<PlotType>, "'CTMPerformanceUsageGraph' requires 'PlotType' to be floating point.");

protected: //Used in render function
    //Letting minPlotLimit and maxPlotLimit be double as PlotType is of no use here
    void PlotUsageGraph(const char* plotLabel, const ImVec2& plotSize, double minPlotLimit, double maxPlotLimit, const ImVec4& plotColor)
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
    void PlotMultiUsageGraph(const char* plotLabel, const char* plotShadedLabel, const char* plotLineLabel,
                            double minPlotLimit, double maxPlotLimit, const ImVec2& plotSize, const ImVec4 plotColors[NumOfPlots])
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

protected: //Used in update function
    //Plotting for both multiple and single plots
    void PlotYAxis(PlotType y)                            { PlotPointAtIndex(0, y);         }
    void PlotYAxisAtIndex(std::size_t index, PlotType y)  { PlotPointAtIndex(index, y);     }

    //X-Axis will always be fixed to updating per second, unless in future i change my mind
    void UpdateXAxis()                                    { xAxisValue += xAxisValueOffset; }
    //Update y-axis value dynamically if the user wants to do it. Optional ofc
    void UpdateYAxisToMaxValue()
    {
        //This is for the case when we start supporting multiple buffers
        PlotType temp = 0;
        for (std::size_t i = 0; i < NumOfPlots; i++)
            temp = std::max(temp, plotBuffers[i].GetMaxYValue());
        
        yAxisMaxValue = temp;
    }

protected: //Common function
    double GetYAxisMaxValue() { return yAxisMaxValue; }

private: //Helper functions
    void PlotPointAtIndex(std::size_t index, PlotType y)
    {
        if(index >= NumOfPlots)
            CTM_LOG_ERROR("Trying to plot beyond the bounds of plots. Total plots: ", NumOfPlots, ", your plot index: ", index);
        else
            plotBuffers[index].AddPoint(xAxisValue, y);
    }

private: //I don't want these variables to accidentally get modified in any way other than the method specified by functions
    //These are hardcoded as all the screens will be having same duration of graph and buffer size
    constexpr static PlotType    graphDuration    = 60;
    constexpr static PlotType    xAxisValueOffset = 1;
    PlotType                     xAxisValue       = 0;
    CTMScrollingBuffer<PlotType> plotBuffers[NumOfPlots]{ static_cast<std::size_t>(graphDuration + 1) };
    
    //Used specifically when we plot dynamically changing y axis values
    PlotType yAxisMaxValue    = 0;
};

#endif