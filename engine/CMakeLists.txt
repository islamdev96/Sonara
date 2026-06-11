cmake_minimum_required(VERSION 3.20)
project(Sonara LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Option 1: Portable DSP tests (build anywhere, incl. Linux CI).
#   cmake -B build -DWAB_BUILD_TESTS=ON && cmake --build build
# =============================================================================
option(WAB_BUILD_TESTS "Build the portable DSP unit tests" OFF)
if(WAB_BUILD_TESTS)
    add_executable(dsp_test test/dsp_test.cpp)
    enable_testing()
    add_test(NAME dsp_test COMMAND dsp_test)
endif()

# =============================================================================
# Option 2: The Windows System Effect APO (the self-contained engine).
#   Requires MSVC + the Windows Driver Kit (WDK).
#   cmake -B build -A x64 && cmake --build build --config Release
# =============================================================================
if(WIN32 AND MSVC)
    # The WDK/SDK ships audiobaseprocessingobject.lib + audioenginebaseapo.h.
    find_path(WDK_APO_INCLUDE audioenginebaseapo.h
        HINTS "$ENV{WDKContentRoot}/Include"
        PATHS "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
              "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um"
        PATH_SUFFIXES "um" "shared")
    find_library(WDK_APO_LIB NAMES audiobaseprocessingobject AudioBaseProcessingObjectV140 BaseAudioProcessingObject
        HINTS "$ENV{WDKContentRoot}/Lib"
        PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
              "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
        PATH_SUFFIXES "um" "lib")
    find_library(WDK_MEDIA_TYPE_LIB NAMES audiomediatypecrt
        HINTS "$ENV{WDKContentRoot}/Lib"
        PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
              "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
        PATH_SUFFIXES "um" "lib")

    add_library(BoosterAPO SHARED
        src/dllmain.cpp
        src/apo/BoosterAPO.cpp
        src/apo/ClassFactory.cpp
        src/BoosterAPO.def
    )
    target_compile_definitions(BoosterAPO PRIVATE UNICODE _UNICODE NOMINMAX)
    set_target_properties(BoosterAPO PROPERTIES
        OUTPUT_NAME "BoosterAPO")

    if(WDK_APO_INCLUDE)
        target_include_directories(BoosterAPO PRIVATE ${WDK_APO_INCLUDE})
    else()
        message(WARNING "WDK audioenginebaseapo.h not found; install the WDK.")
    endif()

    target_link_libraries(BoosterAPO PRIVATE
        ole32 oleaut32 uuid propsys avrt ksuser
        $<$<BOOL:${WDK_APO_LIB}>:${WDK_APO_LIB}>
        $<$<BOOL:${WDK_MEDIA_TYPE_LIB}>:${WDK_MEDIA_TYPE_LIB}>)
endif()
