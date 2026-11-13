
cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(SKIP_BUILDING_CODEGEN off)

project(GDMegaOverlay LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

file(GLOB SOURCES
  src/*.cpp
  src/*.h
  src/*.c
  src/GUI/*.cpp
  src/GUI/*.h
  src/Hacks/*.cpp
  src/hacks/*.cpp
  src/JsonPatches/*.cpp
  src/Macrobot/*.cpp
  src/DiscordRPCManager/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

# ImGui math operators: define before including any ImGui headers
target_compile_definitions(${PROJECT_NAME} PRIVATE
  IMGUI_DEFINE_MATH_OPERATORS
)

if (NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
  message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src/)

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Updated ImGui dependency for Geode 5
set(IMGUI_VERSION "v1.92.3")
CPMAddPackage("gh:matcool/gd-imgui-cocos#geode")

target_link_libraries(${PROJECT_NAME} imgui-cocos)

# Android video renderer uses the NDK media codec/muxer (AMediaCodec, AMediaMuxer).
if(ANDROID)
  target_link_libraries(${PROJECT_NAME} mediandk)
endif()

setup_geode_mod(${PROJECT_NAME})

include(cmake/git.cmake)

message("Getting modules\n")

include(cmake/nhjson.cmake)

if(NOT ANDROID)
  include(cmake/discord-rpc.cmake)
endif()

include(cmake/gdr.cmake)

set(BUILD_PROGRAMS OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)

include(cmake/portable-file-dialogs.cmake)
include(cmake/subprocess.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${nhjson_SOURCE_DIR}/single_include
  ${gdr_SOURCE_DIR}/include/gdr
  ${portable-file-dialogs_SOURCE_DIR}
  ${subprocess_SOURCE_DIR}/include
)

if(NOT ANDROID)
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${discord-rpc_SOURCE_DIR}/include
  )
  target_link_libraries(${PROJECT_NAME} discord-rpc)
endif()
