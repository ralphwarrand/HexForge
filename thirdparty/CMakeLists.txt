# This file manages the automatic download and configuration of all third-party libraries.
# It uses CMake's FetchContent module to clone them from their Git repositories.

# --- GLM (OpenGL Mathematics) -----------------------------------------------
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8 # A stable and widely used version
)
FetchContent_MakeAvailable(glm)

# --- GLFW (Windowing and Input) ---------------------------------------------
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
# Disable building unnecessary parts of GLFW for our use case.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- GLAD (OpenGL Loading Library) - LOCAL ----------------------------------
# We are now using a local, pre-generated version of GLAD.
# This command tells CMake to look for a CMakeLists.txt inside the 'glad' directory.
add_subdirectory(glad)

# --- ImGui (Immediate Mode GUI) ---------------------------------------------
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking # Use the latest from the docking branch for advanced features
)
FetchContent_MakeAvailable(imgui)

# --- EnTT (Entity Component System) -----------------------------------------
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.11.1
)
# Using MakeAvailable is the modern way. EnTT's CMake script will correctly
# create an INTERFACE target named 'entt' that we can link against.
FetchContent_MakeAvailable(entt)


# --- STB (Single-File Public Domain Libraries) - UPDATED --------------------
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master # STB is stable on master
)
# MakeAvailable will download and unpack the source. Since stb has no
# CMakeLists.txt, no target will be created automatically.
FetchContent_MakeAvailable(stb)

# We manually create INTERFACE libraries for the specific headers we need.
# The ${stb_SOURCE_DIR} variable is set by FetchContent.
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

add_library(stb_truetype INTERFACE)
target_include_directories(stb_truetype INTERFACE ${stb_SOURCE_DIR})


# --- Assimp (Asset Importer Library) ----------------------------------------
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.2.5
)
# Disable building tests and other unnecessary parts for our use case.
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)
