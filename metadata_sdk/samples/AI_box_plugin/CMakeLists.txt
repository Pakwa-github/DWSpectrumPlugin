## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)
project(AI_box_plugin)

set(metadataSdkDir "" CACHE PATH "Path to unpacked VMS Metadata SDK zip.")
if(metadataSdkDir STREQUAL "")
    set(metadataSdkDir ${CMAKE_CURRENT_LIST_DIR}/../..) #< Assume building samples inside the SDK.
    if(NOT EXISTS ${metadataSdkDir}/src/nx/sdk OR NOT EXISTS ${metadataSdkDir}/nx_kit/src/nx/kit)
        message(FATAL_ERROR "Define metadataSdkDir cache variable to point to the unzipped SDK.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    # In Linux, for the plugin .so library, set `rpath` to "$ORIGIN" and do not set `runpath`, thus
    # enabling the lookup of the dependencies in the plugin dir first.
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--disable-new-dtags")
endif()
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    add_compile_options("/Z7")
endif()

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

#--------------------------------------------------------------------------------------------------
# Define nx_kit lib, static.

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)

add_subdirectory(${metadataSdkDir}/nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

#--------------------------------------------------------------------------------------------------
# Define nx_sdk lib, static, depends on nx_kit.

set(SDK_SRC_DIR ${metadataSdkDir}/src)
file(GLOB_RECURSE SDK_SRC CONFIGURE_DEPENDS ${SDK_SRC_DIR}/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_include_directories(nx_sdk PUBLIC ${SDK_SRC_DIR})
target_link_libraries(nx_sdk PRIVATE nx_kit)

target_compile_definitions(nx_sdk PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}) #< for nxLibContext()

#--------------------------------------------------------------------------------------------------
# Define AI_box_plugin lib, dynamic, depends on nx_kit and nx_sdk.

set(AI_BOX_PLUGIN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE AI_BOX_PLUGIN_SRC CONFIGURE_DEPENDS ${AI_BOX_PLUGIN_SRC_DIR}/*)

add_library(AI_box_plugin SHARED ${AI_BOX_PLUGIN_SRC})
target_include_directories(AI_box_plugin PRIVATE ${AI_BOX_PLUGIN_SRC_DIR})
target_link_libraries(AI_box_plugin PRIVATE nx_kit nx_sdk)

target_compile_definitions(AI_box_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
)

# asio
target_include_directories(AI_box_plugin PRIVATE ${AI_BOX_PLUGIN_SRC_DIR}/third_party/asio/include)
target_compile_definitions(AI_box_plugin PRIVATE ASIO_STANDALONE)
target_compile_definitions(AI_box_plugin PRIVATE _WIN32_WINNT=0x0601)
target_compile_definitions(AI_box_plugin PRIVATE _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

if(NOT WIN32)
    target_link_libraries(AI_box_plugin PRIVATE pthread)
endif()

# Small test executable to validate Subscriber IPC connection.
add_executable(ipc_test
    ${AI_BOX_PLUGIN_SRC_DIR}/test_ipc/main.cpp
    ${AI_BOX_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/AIbox/net/subscriber.cpp
    ${AI_BOX_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/AIbox/net/tcp_client.cpp
)
target_include_directories(ipc_test PRIVATE ${AI_BOX_PLUGIN_SRC_DIR} ${AI_BOX_PLUGIN_SRC_DIR}/third_party/asio/include)
target_compile_definitions(ipc_test PRIVATE ASIO_STANDALONE _WIN32_WINNT=0x0601 _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
target_link_libraries(ipc_test PRIVATE nx_kit nx_sdk)
if(NOT WIN32)
    target_link_libraries(ipc_test PRIVATE pthread)
endif()

#--------------------------------------------------------------------------------------------------
# Copy object_streamer files.

file(COPY "object_streamer_files/" DESTINATION "object_streamer/" FILES_MATCHING PATTERN "*")
