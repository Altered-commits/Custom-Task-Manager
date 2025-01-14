#include "ctm_perf_net_screen.h"

//Equivalent to OnInit function
CTMPerformanceNETScreen::CTMPerformanceNETScreen()
{
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceNETScreen::~CTMPerformanceNETScreen()
{
    SetInitialized(false);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceNETScreen::OnRender()
{
    ImGui::Text("EEEEEEEEEEEEEEEEEEEEEE");
}

void CTMPerformanceNETScreen::OnUpdate()
{

}
