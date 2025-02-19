#include "ctm_perf_disk_screen.h"

//Equivalent to OnInit function
CTMPerformanceDISKScreen::CTMPerformanceDISKScreen()
{
    //Nah no initialization
    if(!CTMConstructorInitDrives())
        return;

    //If we can't even get basic info, no point in initializing
    if(!CTMConstructorInitPDH())
        return;
    
    SetInitialized(true);
}

//Equivalent to OnClean function
CTMPerformanceDISKScreen::~CTMPerformanceDISKScreen()
{
    CTMDestructorCleanupPDH();
    SetInitialized(false);
}

//--------------------CONSTRUCTOR AND DESTRUCTOR FUNCTIONS--------------------
bool CTMPerformanceDISKScreen::CTMConstructorInitDrives()
{
    DWORD diskDriveMask = GetLogicalDrives();
    if(diskDriveMask == 0)
    {
        CTM_LOG_ERROR("Failed to get logical disk drives. Error code: ", GetLastError());
        return false;
    }

    //Get the system directory to check if the current drive we iterating thru is a system drive
    char systemDirectory[MAX_PATH] = { 0 };
    UINT status = GetSystemDirectoryA(systemDirectory, sizeof(systemDirectory));
    if(status == 0)
    {
        CTM_LOG_ERROR("Failed to get system directory. Error code: ", GetLastError());
        return false;
    }

    //Extract the system drive
    char systemDriveLetter = systemDirectory[0];

    //Temporary storage for storing full path of a drive
    char diskDriveFullPath[] = " :\\";

    //Store disk size information for each disk in this variable
    ULARGE_INTEGER freeAvailBytesToCaller = { 0 }, freeAvailBytes = { 0 }, totalBytes = { 0 };

    //For all the possible drives which are valid
    for(std::uint8_t i = 'A'; i <= 'Z'; i++)
    {
        //That letter is taken, we need this
        if(diskDriveMask & (1 << (i - 'A')))
        {
            //Create a entry in 'diskDriveVector'
            auto& diskDriveEntry = diskDriveVector.emplace_back();

            //1) Store the disk drive we need
            diskDriveEntry.letterAssigned = i;

            //2) Also store the drive type
            diskDriveFullPath[0] = i;
            switch(GetDriveTypeA(diskDriveFullPath))
            {
                case DRIVE_FIXED:
                    diskDriveEntry.driveType = "Fixed Drive";
                    break;
                case DRIVE_REMOVABLE:
                    diskDriveEntry.driveType = "Removable Drive";
                    break;
                case DRIVE_CDROM:
                    diskDriveEntry.driveType = "CD-ROM Drive";
                    break;
                case DRIVE_REMOTE:
                    diskDriveEntry.driveType = "Network Drive";
                    break;
                case DRIVE_RAMDISK:
                    diskDriveEntry.driveType = "RAM Disk";
                    break;
                default:
                    diskDriveEntry.driveType = "Unknown Type";
                    break;
            }

            //3) Store if its the drive where OS resides
            diskDriveEntry.isSystemDrive = (i == systemDriveLetter);

            //4) File system info and serial number
            if(!GetVolumeInformationA(diskDriveFullPath, nullptr, 0, nullptr, nullptr, nullptr,
                                diskDriveEntry.fileSystemName, sizeof(diskDriveEntry.fileSystemName)))
                CTM_LOG_WARNING("Failed to get file system info and serial number for drive: ", i, ". Error code: ", GetLastError());
            
            //5) Disk space information (free space available and total size of the disk)
            if(!GetDiskFreeSpaceExA(diskDriveFullPath, &freeAvailBytesToCaller, &totalBytes, &freeAvailBytes))
                CTM_LOG_WARNING("Failed to get disk space info for drive: ", i, ". Error code: ", GetLastError());
            //Succeeded, store the stuff in diskDriveEntry
            else
            {
                diskDriveEntry.diskTotalSize = (totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0));
                diskDriveEntry.diskAvailSize = (freeAvailBytes.QuadPart / (1024.0 * 1024.0 * 1024.0));
            }
        }
    }
    //We got all the drives
    return true;
}

bool CTMPerformanceDISKScreen::CTMConstructorInitPDH()
{
    if(PdhOpenQueryA(NULL, 0, &hQuery) != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to open pdh query.");
        return false;
    }

    //Register a resource guard just in case
    resourceGuard.RegisterCleanupFunction(pdhCleanupFunctionName, [this](){
        PdhCloseQuery(hQuery);
    });

    //This is reused for every single drive available (the ~ is replaced when running the loop)
    char pdhReadQuery[]  = "\\LogicalDisk(~:)\\Disk Read Bytes/sec";
    char pdhWriteQuery[] = "\\LogicalDisk(~:)\\Disk Write Bytes/sec";
    const std::uint8_t tildePos = 13;
    bool atleastOneSucceeded    = false;

    //Loop over all the drives present
    for(auto &&i : diskDriveVector)
    {
        //Override the query '~' with our string
        pdhReadQuery[tildePos]  = i.letterAssigned;
        pdhWriteQuery[tildePos] = i.letterAssigned;

        //Add the counter now
        PDH_HCOUNTER hReadCounter, hWriteCounter;
        PDH_STATUS readStatus  = PdhAddEnglishCounterA(hQuery, pdhReadQuery, 0, &hReadCounter);
        PDH_STATUS writeStatus = PdhAddEnglishCounterA(hQuery, pdhWriteQuery, 0, &hWriteCounter);

        if(readStatus == ERROR_SUCCESS && writeStatus == ERROR_SUCCESS)
        {
            atleastOneSucceeded = true;
            readCounterVector.emplace_back(hReadCounter);
            writeCounterVector.emplace_back(hWriteCounter);
        }
        else
            CTM_LOG_ERROR("Failed to add read/write counter for drive: ", i.letterAssigned, ". Error code: Read: ", readStatus, " Write: ", writeStatus);
    }

    //If none of the counters have been added, return out
    if(!atleastOneSucceeded)
    {
        CTM_LOG_ERROR("Failed to add atleast 1 counter.");
        PdhCloseQuery(hQuery);
        resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
        return false;
    }

    //Ok cool we added counters, now query it initially once, if we can't even query it once, no point in going forward
    PDH_STATUS status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect counter data initially. Error code: ", status);
        PdhCloseQuery(hQuery);
        resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
        return false;
    }

    return true;
}

void CTMPerformanceDISKScreen::CTMDestructorCleanupPDH()
{
    PdhCloseQuery(hQuery);
    resourceGuard.UnregisterCleanupFunction(pdhCleanupFunctionName);
}

//--------------------MAIN RENDER AND UPDATE FUNCTIONS--------------------
void CTMPerformanceDISKScreen::OnRender()
{
    //Buttons for viewing different disk drives
    for(std::uint8_t i = 0; i < diskDriveVector.size(); i++)
    {
        diskDriveLabel[0] = diskDriveVector[i].letterAssigned;
        //Every time we click on a disk to view, we check for one thing. That is-
        if(ImGui::Button(diskDriveLabel))
        {
            //-if we moved our view to another drive, we reset the graph (so they can now show stuff for other drives)
            if(currentViewingDriveIndex != i)
            {
                ResetGraph();
                currentViewingDriveIndex = i;
            }
        }
        
        //I want the buttons to be on same line with some spacing
        ImGui::SameLine(0.0, 15.0f);
    }
    
    //Because we are using 'SameLine' above, we need the other content to be from the new line
    //With some spacing vertically
    ImGui::NewLine();
    ImGui::Dummy({ 0.0f, 10.0f });

    //----------BELOW IS THE SAME AS EVERY OTHER SCREEN IMPL----------
    //Decode the max y limit into value and type
    float decodedMaxLimit = 0.0f; std::uint8_t decodedType = 0;
    CTMPerformanceCommon::DecodeDoubleWithUnits(displayMaxYLimit, decodedType, decodedMaxLimit);

    //Graph plotting
    ImGui::Text("%.2f %s", decodedMaxLimit, CTMPerformanceCommon::GetDataUnitAtIdx(decodedType));
    PlotMultiUsageGraph("Drive Usage (Over 60 sec)", "Read Usage", "Write Usage", 0, GetYAxisMaxValue(), {-1, 300}, graphColors);
    ImGui::TextUnformatted("0 KB");

    //Give some spacing vertically before displaying Drive Info
    ImGui::Dummy({-1.0f, 15.0f});

    //Add some padding to the frame (see the blank space around the text of the collapsable header)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));

    //Statistics
    isStatisticsHeaderExpanded = ImGui::CollapsingHeader("Selected Drive Statistics");
    if(isStatisticsHeaderExpanded)
        RenderViewForDiskDrive();

    ImGui::PopStyleVar();
}

void CTMPerformanceDISKScreen::OnUpdate()
{
    UpdateXAxis();

    auto&&[readUsageInKB, writeUsageInKB] = GetDriveUsageAtIdx(currentViewingDriveIndex);

    //Assign the metricsVector the above values with encoding only when statistics header is collapsed (user is actively watching the data)
    if(isStatisticsHeaderExpanded)
    {
        metricsVector[static_cast<std::size_t>(MetricsVectorIndex::ReadUsage)].second 
            = CTMPerformanceCommon::EncodeDoubleWithUnits(readUsageInKB);
        metricsVector[static_cast<std::size_t>(MetricsVectorIndex::WriteUsage)].second
            = CTMPerformanceCommon::EncodeDoubleWithUnits(writeUsageInKB);
    }

    //Plot the values as well
    PlotYAxisAtIndex(static_cast<std::size_t>(CTMPlotTypeIndex::DiskRead), readUsageInKB);
    PlotYAxisAtIndex(static_cast<std::size_t>(CTMPlotTypeIndex::DiskWrite), writeUsageInKB);

    //Also since we need dynamically changing y-axis limits, update y-axis at the backend side to the graphs max limit
    UpdateYAxisToMaxValue();

    //Get the max y-axis limit and just convert it to some good readable unit
    displayMaxYLimit = CTMPerformanceCommon::EncodeDoubleWithUnits(GetYAxisMaxValue());
}

//--------------------RENDER FUNCTIONS--------------------
void CTMPerformanceDISKScreen::RenderViewForDiskDrive()
{
    if(ImGui::BeginTable("DISKStatisticsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Metrics");
        ImGui::TableSetupColumn("Values");
        ImGui::TableHeadersRow();

        //Dynamically changing Disk Info From 'metricsVector'
        for(std::size_t i = 0; i < metricsVector.size(); i++)
        {
            auto&&[metric, value] = metricsVector[i];

            ImGui::TableNextRow();
            //Metrics column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(metric);
            //Value column
            ImGui::TableSetColumnIndex(1);
            std::visit([this](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;
            
                //Disk usage, 3 LSB will represent its type (KB, MB...) and rest represent the actual value
                if constexpr(std::is_same_v<T, float>)
                {
                    float decodedDiskData = 0.0f; std::uint8_t decodedType = 0;
                    CTMPerformanceCommon::DecodeDoubleWithUnits(arg, decodedType, decodedDiskData);
                    ImGui::Text("%.2f %s", decodedDiskData, CTMPerformanceCommon::GetDataUnitAtIdx(decodedType));
                }
            }, value);
        }

        //Static Disk Info
        auto& diskDriveInfo = diskDriveVector[currentViewingDriveIndex];
        RenderDiskDriveTableEntries(diskDriveInfo);

        ImGui::EndTable();
    }
}

void CTMPerformanceDISKScreen::RenderDiskDriveTableEntries(DiskDriveInfo& diskDriveInfo)
{
    //1) Drive letter
    diskDriveLabel[0] = diskDriveInfo.letterAssigned;
    RenderDiskDriveTableEntry("Drive Letter", diskDriveLabel);

    //2) Drive type
    RenderDiskDriveTableEntry("Drive Type", diskDriveInfo.driveType);

    //4) Drive File System type
    RenderDiskDriveTableEntry("File System", diskDriveInfo.fileSystemName);

    //5) Drive total size
    RenderDiskDriveTableEntry("Total Size", diskDriveInfo.diskTotalSize);

    //6) Drive avail size
    RenderDiskDriveTableEntry("Available Size", diskDriveInfo.diskAvailSize);

    //7) Drive used size
    RenderDiskDriveTableEntry("Used Size", diskDriveInfo.diskTotalSize - diskDriveInfo.diskAvailSize);

    //8) Drive contains OS?
    RenderDiskDriveTableEntry("OS in this drive?", diskDriveInfo.isSystemDrive ? "Yes" : "No");
}

void CTMPerformanceDISKScreen::RenderDiskDriveTableEntry(const char* metricsText, const char* valueText)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(metricsText);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", valueText);
}

void CTMPerformanceDISKScreen::RenderDiskDriveTableEntry(const char* metricsText, double valueText)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(metricsText);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2lf GB", valueText);
}

//--------------------HELPER FUNCTIONS--------------------
std::pair<double, double> CTMPerformanceDISKScreen::GetDriveUsageAtIdx(std::size_t driveIdx)
{
    /*
     * In the std::pair: First value is read usage and second value is write usage
     */

    //Before we go on trying to read the value, collect the value first
    PDH_STATUS status = PdhCollectQueryData(hQuery);
    if(status != ERROR_SUCCESS)
    {
        CTM_LOG_ERROR("Failed to collect data for Drive Usage. Error code: ", status);
        return {0.0, 0.0};
    }

    //Query PDH counters at that index which is specified by the 'driveIdx'
    PDH_HCOUNTER readCounter  = readCounterVector[driveIdx];
    PDH_HCOUNTER writeCounter = writeCounterVector[driveIdx];

    //To store the actual value itself
    PDH_FMT_COUNTERVALUE readValue, writeValue;
    DWORD dwType;

    if(PdhGetFormattedCounterValue(readCounter, PDH_FMT_LARGE, &dwType, &readValue) != ERROR_SUCCESS)
        CTM_LOG_WARNING("Failed to get read usage for drive index: ", driveIdx, ". Expect garbage / null value.");

    if(PdhGetFormattedCounterValue(writeCounter, PDH_FMT_LARGE, &dwType, &writeValue) != ERROR_SUCCESS)
        CTM_LOG_WARNING("Failed to get write usage for drive index: ", driveIdx, ". Expect garbage / null value.");
    
    //Finally, return the pair of read and write usage (the values will be at minimum in KB so yeah)
    return std::make_pair(readValue.largeValue / 1024.0, writeValue.largeValue / 1024.0);
}
