#ifndef CTM_PERFORMANCE_USAGE_GRAPH_HPP
#define CTM_PERFORMANCE_USAGE_GRAPH_HPP

//Using std::max instead
#undef max

//My stuff
#include "../CTMPureHeaderFiles/ctm_logger.h"
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
    //Also this is the only place where you will find variables with Pascal case and not Camel case
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

    void ResetBuffer()
    {
        Data.clear();
        Offset = 0;
        //An initial value is always needed so it doesn't throw an access out of bounds error
        Data.emplace_back(0, 0);
    }

    T GetMaxYValue()
    {
        return std::max_element(Data.begin(), Data.end(),
                    [](CTMPoint<T>& left, CTMPoint<T>& right){ return left.y < right.y; })->y;
    }
};

//Its simply a class to be inherited by 'screen' classes (aka (pages/screens) like Cpu Usage, etc)
//It contains function and variables to plot graph which is common to all screens
//No error handling in most cases as i am assuming i'm not dumb ( which i am ) enough to access arrays out of bounds
//Templating it as it maybe used for plotting multiple lines on a single graph. Template param PlotType added to switch between float and double
template<std::size_t NumOfPlots, typename PlotType>
class CTMPerformanceUsageGraph
{
    //Right now i only allow either 1 or 2 plots at max
    static_assert(NumOfPlots == 1 || NumOfPlots == 2, "Max number of plots supported at this time are 1 or 2 only.");
    //Simple static assert to check if PlotType is floating point compatible. Other types are not allowed
    static_assert(std::is_floating_point_v<PlotType>, "'CTMPerformanceUsageGraph' requires 'PlotType' to be floating point.");

protected: //Used in render function
    //For plotting single line in single graph
    void PlotUsageGraph(const char*, double, double, const ImVec2&, const ImVec4&);
    //For plotting multiple lines in single graph
    void PlotMultiUsageGraph(const char*, const char*, const char*, double, double, const ImVec2&, const ImVec4[NumOfPlots]);

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
        for(std::size_t i = 0; i < NumOfPlots; i++)
            temp = std::max(temp, plotBuffers[i].GetMaxYValue());
        
        yAxisMaxValue = temp;
    }

protected: //Common function
    double GetYAxisMaxValue() { return yAxisMaxValue; }
    void   ResetGraph()
    {
        for(std::size_t i = 0; i < NumOfPlots; i++)
            plotBuffers[i].ResetBuffer();
        xAxisValue    = 0;
        yAxisMaxValue = 0;
    }

private: //Helper functions
    void PlotPointAtIndex(std::size_t index, PlotType y) { plotBuffers[index].AddPoint(xAxisValue, y); }

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