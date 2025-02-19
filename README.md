# Custom Task Manager (CTM)

### I am making this application to sort of try to mimic Windows 'Task Manager'. It uses Dear ImGui and ImPlot to render all the content of CTM (Both the Client and the Non-Client Region).

# !!! Important !!!
- I compiled this application on MSVC and MinGW and it worked fine for me. I cannot guarantee that this will compile on other compilers properly but give it a shot.
- If you are trying to run this app and you have an Antivirus which checks the app before running it, chances are, this may bug out a little bit before it runs fine. Atleast it bugged out on my Avast Antivirus.
- Alot of inconsistent code as i'm rushing to complete this project now (sry i have other works as well rn). So if you feel like this code is bad (which it is), please contribute and fix the code... thank you in advance if you do fix the code :)

## Features
- **Single Instance App**: Only one instance of application is allowed at a time throughout system. If you try to open a new instance while the main instance is hung, then the main instance will be terminated and a new instance will be opened.
- **Monitoring Processes**: Provides info such as ProcessID, CPU usage, Memory Usage, Network Usage and File Usage. It can also terminate processes excluding processes protected by OS.
- **Hardware Statistics**: Provides statistics about hardware like CPU, Memory, etc. More to be added in future.
- **Basic Settings Menu**: A menu where you can tinker with how window looks, default page, etc. More settings to be added in future.

## Requirements
- C++17 or later _(for the build system)_
- DirectX 11
- CMake
- ImGui & ImPlot _(This project uses DirectX + Win32, so feel free to change `ImGUIBackend` and `ImGUI` folder to match your requirements)_

## Third-Party Libraries
This project uses the following third-party libraries:

- [Dear ImGui](https://github.com/ocornut/imgui), licensed under the [MIT License](ImGUI/LICENSE.txt).
- [ImPlot](https://github.com/epezent/implot), licensed under the [MIT License](ImPlot/LICENSE).

## How to use?
- Clone the repository to your local machine.
- Ensure you have **CMake** and a C++ compiler (e.g., `MSVC`) installed on your system.
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
- Run the executable _(.exe file should be in root directory if you are using MinGW, else it will be in Debug or Release directory for MSVC)_
