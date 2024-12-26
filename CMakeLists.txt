# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(CTMApp)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output directory for the executable to the same as main.cpp
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Add ImGui and ImPlot source files
file(GLOB IMGUI_SOURCES "ImGUI/*.cpp")
file(GLOB IMGUI_BACKEND_SOURCES "ImGUIBackend/*.cpp")
file(GLOB IMPLOT_SOURCES "ImPlot/*.cpp")

# Add CTMBackend source files recursively
file(GLOB_RECURSE CTM_SOURCES "CTMBackend/*.cpp")

# Add executable target
add_executable(CTMApp main.cpp ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES} ${IMPLOT_SOURCES} ${CTM_SOURCES})

# Include directories for ImGui headers
target_include_directories(CTMApp PRIVATE ImGUI)

# Link required libraries
target_link_libraries(CTMApp PRIVATE dxgi d3d11 gdi32 d3dcompiler dwmapi tdh)
