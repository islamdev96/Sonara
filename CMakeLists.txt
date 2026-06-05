cmake_minimum_required(VERSION 3.20)
project(WinAudioBoosterPro)

set(CMAKE_CXX_STANDARD 20)

# Build a shared library (DLL)
add_library(BoosterAPO SHARED
    src/dllmain.cpp
    src/BoosterAPO.cpp
)

# Target the Windows API and link required COM and Audio libraries
target_link_libraries(BoosterAPO PRIVATE
    ole32.lib
    oleaut32.lib
    uuid.lib
    # Need to link against Baseaudioprocessingobject from WDK
)

# Output directory
set_target_properties(BoosterAPO PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
