# Custom Task Manager (CTM)

### I am making this application to sort of try to mimic Windows 'Task Manager'. It uses ImGui to render all the content of CTM.

## Features
- Single instance app: Only one instance of application is allowed at a time throughout system.
- Monitoring processes: Provides info such as ProcessID, CPU usage, Memory Usage, and Disk Usage.

## Requirements
- C++17 or later (for the build system)
- CMake
- ImGui (This project uses DirectX + Win32, so feel free to change `ImGUIBackend` and `ImGUI` folder to match your requirements)

## How to use?
- Clone the repository to your local machine.
- Ensure you have **CMake** and a C++ compiler (e.g., `g++`) installed on your system. (I'm not really sure if it works with Visual Studio)
- Build the project:
   - Navigate to the project directory.
   - Use the provided `CMakeLists.txt` to generate build files:
     ```bash
     cmake -S . -B build
     ```
   - Compile the application:
     ```bash
     cmake --build build
     ```
- Run the executable (.exe file should be in root directory)
