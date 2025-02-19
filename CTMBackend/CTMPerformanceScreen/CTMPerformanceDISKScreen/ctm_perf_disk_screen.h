#ifndef CTM_PERFORMANCE_DISK_SCREEN_HPP
#define CTM_PERFORMANCE_DISK_SCREEN_HPP

//Windows stuff
#include <Pdh.h>
//My stuff
#include "../ctm_perf_graph.h"
#include "../ctm_perf_common.h"
#include "../../ctm_base_state.h"
#include "../../ctm_logger.h"
#include "../../CTMGlobalManagers/ctm_critical_resource_guard.h"

//Struct used to store very basic information about each drive
struct DiskDriveInfo
{
    char        letterAssigned;                                 // ----|
    bool        isSystemDrive; //Drive in which main OS resides // ----|--> Total of 8 bytes with padding
    const char* driveType;
    char        fileSystemName[8] = { 0 }; //Even tho the API to get this name supports 'MAX_PATH' bytes, i am not going to alloc that much
    double      diskTotalSize;
    double      diskAvailSize;
};

class CTMPerformanceDISKScreen : public CTMBasePerformanceScreen, protected CTMPerformanceUsageGraph<2, double>
{
public:
    CTMPerformanceDISKScreen();
    ~CTMPerformanceDISKScreen() override;

protected:
    void OnRender() override;
    void OnUpdate() override;

private: //Constructor and destructor functions
    bool CTMConstructorInitDrives();
    bool CTMConstructorInitPDH();
    void CTMDestructorCleanupPDH();

private: //Render functions
    void RenderViewForDiskDrive();
    void RenderDiskDriveTableEntries(DiskDriveInfo&);
    void RenderDiskDriveTableEntry(const char*, const char*);
    void RenderDiskDriveTableEntry(const char*, double);

private: //Helper functions
    //std::pair<read usage, write usage>
    std::pair<double, double> GetDriveUsageAtIdx(std::size_t);

private: //Resource guard
    CTMCriticalResourceGuard& resourceGuard = CTMCriticalResourceGuard::GetInstance();
    const char* pdhCleanupFunctionName = "CTMPerformanceDISKScreen::CleanupPDH";

private: //Collapsable header variables
    bool isStatisticsHeaderExpanded = false;

private: //PDH variables
    PDH_HQUERY hQuery;
    std::vector<PDH_HCOUNTER> readCounterVector;
    std::vector<PDH_HCOUNTER> writeCounterVector;

private: //Misc enums
    enum class CTMPlotTypeIndex { DiskRead, DiskWrite };
    //Value used for displaying graph y limits. Dynamically changes its unit
    float displayMaxYLimit = 0;
    //Graph colors :)
    static constexpr ImVec4 graphColors[2] = { {0.2f, 0.8f, 0.4f, 1.0f}, {0.1f, 0.6f, 0.3f, 1.0f} };

private: //Disk variables
    std::vector<DiskDriveInfo> diskDriveVector; //Equivalent to metricsVector
    //Used for displaying button / metrics text basically
    char         diskDriveLabel[9]        = " : Drive";
    std::uint8_t currentViewingDriveIndex = 0;
};

#endif