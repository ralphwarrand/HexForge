# modified version of: https://github.com/meemknight/cmakeSetup

cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

# Set this to true to ship the game - enables production build configuration
option(PRODUCTION_BUILD "Make this a production build" OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # Link time optimization

if(MSVC)
	add_compile_options(/arch:AVX2) # SIMD optimizations for MSVC
endif()

project(Hexahedron)

# GLFW and other dependencies setup
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glfw-3.3.2)
add_subdirectory(thirdparty/glad)
add_subdirectory(thirdparty/stb_image)
add_subdirectory(thirdparty/stb_truetype)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/imgui-docking)
add_subdirectory(thirdparty/date)
add_subdirectory(thirdparty/fmt)

# Include all source files for the project
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable("${CMAKE_PROJECT_NAME}" src/main.cpp)
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/date/src/tz.cpp)
target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE USE_OS_TZDB=0)

set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC GLFW_INCLUDE_NONE=1)

if(PRODUCTION_BUILD)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1)
else()
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0)
endif()

target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})

if(MSVC)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)
	set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

# Link other libraries
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE glm glfw glad stb_image stb_truetype imgui date fmt-header-only)

# macOS-specific setup for linking libcurl
if(APPLE)
	# Find and link libcurl
	find_package(CURL REQUIRED)
	target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE CURL::libcurl)
endif()