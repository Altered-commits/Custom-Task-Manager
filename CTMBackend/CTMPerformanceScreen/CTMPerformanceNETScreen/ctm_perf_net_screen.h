#ifndef CTM_PERFORMANCE_NET_SCREEN_HPP
#define CTM_PERFORMANCE_NET_SCREEN_HPP

//My stuff
#include "../ctm_perf_graph.h"
#include "../../ctm_constants.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"

class CTMPerformanceNETScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph
{
public:
    CTMPerformanceNETScreen();
    ~CTMPerformanceNETScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;
};

#endif