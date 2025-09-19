cmake_minimum_required(VERSION 3.20)
project(SandboxGame LANGUAGES CXX)

# --- Executable Target -----------------------------------------------------
# This defines our actual game/example executable.
# It should have its own main.cpp.
add_executable(SandboxGame main.cpp)

# --- Link Against The Engine -----------------------------------------------
# This is the crucial step. We link the executable against our engine library.
# CMake automatically handles include directories and other dependencies
# that we defined as PUBLIC in the engine's CMakeLists.txt.
target_link_libraries(SandboxGame PRIVATE HexForgeEngine)

# --- Resource Handling -----------------------------------------------------
# This command ensures the 'resources' folder is copied next to the
# executable after a successful build, so the game can find its assets.
add_custom_command(TARGET SandboxGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:SandboxGame>/resources
        COMMENT "Copying resources to build directory"
)

# --- Compile Definitions for this specific example -------------------------
if(PRODUCTION_BUILD)
    target_compile_definitions(SandboxGame PRIVATE PRODUCTION_BUILD=1 RESOURCES_PATH="./resources/")
else()
    # In non-production builds, point to the source resource folder directly.
    target_compile_definitions(SandboxGame PRIVATE PRODUCTION_BUILD=0 RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
endif()

# Platform-specific settings for the final executable
if(MSVC AND PRODUCTION_BUILD)
    set_target_properties(SandboxGame PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()
