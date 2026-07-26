cmake_minimum_required(VERSION 3.20)
project(HexForgeEngine LANGUAGES CXX)

# --- Engine Library Target -------------------------------------------------

# Find all .cpp files within the 'src' directory.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Define the library and add all its source files at once.
# This includes the engine's own sources and the ImGui implementation files.
# We build the paths manually from CMAKE_BINARY_DIR as this is more robust
# than relying on variables or target properties from other subdirectories.
add_library(HexForgeEngine STATIC
		${ENGINE_SOURCES}
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui.cpp"
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_draw.cpp"
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_tables.cpp"
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_widgets.cpp"
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_glfw.cpp"
		"${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_opengl3.cpp"
)


# --- Include Directories ---------------------------------------------------
# PUBLIC: Consumers of this library need this path to find <HexForge/...> headers.
# PRIVATE: The engine's own source files need these paths for internal includes.
target_include_directories(HexForgeEngine
		PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		# Explicitly add the ImGui source directory to ensure it's found by the PCH.
		# FetchContent places dependencies in the binary directory under _deps.
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/imgui-src>
		# Explicitly add the EnTT include directory. NOTE: EnTT headers are in its 'src' directory.
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/entt-src/src>
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/HexForge>
)

# --- Precompiled Header ----------------------------------------------------
# The path needs to be relative to this CMakeLists.txt file.
target_precompile_headers(HexForgeEngine
		PRIVATE
		"include/HexForge/pch.h"
)

# --- Dependencies ----------------------------------------------------------
# The engine needs to link against the third-party libraries.
# These are available because the top-level CMakeLists.txt added them.
target_link_libraries(HexForgeEngine
		PUBLIC
		# Libraries whose headers are included by our public engine headers
		# must be linked publicly to propagate their include directories.
		glm
		glfw
		glad
		PRIVATE
		# We no longer link the 'imgui' or 'entt' targets because their sources are
		# either compiled directly or are header-only. Their include paths are
		# handled by the target_include_directories call above.
		stb_image
		stb_truetype
		assimp
)
# Link CURL on Linux/Apple
if(UNIX)
	find_package(CURL REQUIRED)
	target_link_libraries(HexForgeEngine PRIVATE CURL::libcurl)
endif()


# --- Compile Definitions ---------------------------------------------------
target_compile_definitions(HexForgeEngine
		PUBLIC
		GLFW_INCLUDE_NONE=1
		ENTT_INCLUDE_NATVIS
		IMGUI_DEFINE_MATH_OPERATORS
		IMGUI_IMPL_OPENGL_LOADER_CUSTOM
		$<$<CONFIG:Debug>:RESOURCES_PATH="${CMAKE_SOURCE_DIR}/examples/sandbox/resources/">
		$<$<CONFIG:Release>:RESOURCES_PATH="./resources/">
)

if(MSVC)
	target_compile_definitions(HexForgeEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

