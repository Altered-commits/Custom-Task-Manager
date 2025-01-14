#ifndef CTM_BASE_STATE_HPP
#define CTM_BASE_STATE_HPP

//Stdlib stuff
#include <chrono>

enum class CTMScreenState : std::uint8_t
{
    Processes,
    Performance,
    Apps,
    Services,
    Settings,
    PageCount, //Personal use
    None
};

enum class CTMPerformanceScreenState : std::uint8_t
{
    CpuInfo,
    MemoryInfo,
    NetInfo,
    PageCount,
    None
};

class CTMBaseScreen
{
public:
    CTMBaseScreen() : lastUpdateTime(std::chrono::steady_clock::now()) {}
    virtual ~CTMBaseScreen() = default;

public:
    void Render()
    {
        if(!isInitialized)
        {
            ImGui::Text("Failed to initialize, rendering of current screen failed.");
            return;
        }

        auto  now     = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - lastUpdateTime).count();

        //Update only if 1 second has passed
        if(elapsed >= 1.0f)
        {
            lastUpdateTime = now;
            OnUpdate();
        }

        OnRender();
    }

protected:
    virtual void OnRender() = 0;
    virtual void OnUpdate() = 0;
    
    //Derived classes tell if they are initialized or not (helps base state to decide when to call render and when not to)
    void SetInitialized(bool init) { isInitialized = init; }

private:
    bool isInitialized = false;
    std::chrono::steady_clock::time_point lastUpdateTime;
};

//For screens under 'Performance' screen
class CTMBasePerformanceScreen
{
public:
    CTMBasePerformanceScreen()          = default;
    virtual ~CTMBasePerformanceScreen() = default;

public: //To be called
    void Render()
    {
        //Failed to initialize stuff in derived class, do not render anything
        if(!isInitialized)
            ImGui::Text("Failed to initialize, rendering of current screen failed.");
        //Else just render
        else
            OnRender();
    }

    void Update()
    {
        //Just call OnUpdate. This function (will be/should be) called inside the OnUpdate function of class derived from 'CTMBaseScreen'
        //That will ensure this function is called every second and not every frame (I mean nothing wrong with calling it every frame... NO)
        //Also the compiler will probably inline this so yeah
        if(isInitialized)
            OnUpdate();
    }

protected: //To be overriden
    virtual void OnRender() = 0;
    virtual void OnUpdate() = 0;

protected: //Decided to keep it seperate from above
    void SetInitialized(bool init) { isInitialized = init; }

private:
    bool isInitialized = false;
};

#endif