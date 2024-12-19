#ifndef CTM_BASE_STATE_HPP
#define CTM_BASE_STATE_HPP

#include <chrono>
// #include <iostream>

enum class CTMScreenState
{
    Processes,
    Performance,
    Apps,
    Services,
    Settings,
    None
};

class CTMBaseState
{
public:
    CTMBaseState() : lastUpdateTime(std::chrono::steady_clock::now()) {}
    virtual ~CTMBaseState() = default;

public:
    void Render()
    {
        if(!isInitialized)
        {
            ImGui::Text("Failed to render content, initialization of current screen failed.");
            return;
        }

        auto  now     = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - lastUpdateTime).count();

        //Update only if 1 second has passed
        if (elapsed >= 1.0f)
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

#endif