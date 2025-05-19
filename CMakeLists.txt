cmake_minimum_required(VERSION 3.20) # Using a newer version for better cross-platform support
cmake_policy(SET CMP0115 NEW)
cmake_policy(SET CMP0069 NEW)
if(POLICY CMP0175)
	cmake_policy(SET CMP0175 OLD)
endif()

# Project settings
project(Hexahedron)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # Link-time optimization

# Enable SIMD optimizations for MSVC
if(MSVC)
	add_compile_options(/arch:AVX2)
endif()

# Production build flag
option(PRODUCTION_BUILD "Enable production build settings" OFF)

# MSVC runtime library settings
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# GLFW and other dependencies setup
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Add third-party libraries
add_subdirectory(thirdparty/glfw-3.3.2)
add_subdirectory(thirdparty/glad)
add_subdirectory(thirdparty/stb_image)
add_subdirectory(thirdparty/stb_truetype)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/imgui-docking)
add_subdirectory(thirdparty/date)
add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/assimp)

# Include source files
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Main executable
add_executable("${CMAKE_PROJECT_NAME}" src/main.cpp)
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/date/src/tz.cpp)
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})

# Include directories
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_include_directories("${CMAKE_PROJECT_NAME}" PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/entt)

# Compilation definitions
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC GLFW_INCLUDE_NONE=1 ENTT_INCLUDE_NATVIS)

if(PRODUCTION_BUILD)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1)
else()
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0)
endif()

# Platform-specific configurations
if(MSVC)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)
	set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

if(LINUX OR APPLE)
	find_package(CURL REQUIRED)
	target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE CURL::libcurl)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE USE_OS_TZDB=1)
else()
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE USE_OS_TZDB=0)
endif()

# Link libraries
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE glm glfw glad stb_image stb_truetype imgui date fmt-header-only assimp)
