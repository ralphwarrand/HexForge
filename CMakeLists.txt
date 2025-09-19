cmake_minimum_required(VERSION 3.20)
project(HexForgeProject)

# --- Include FetchContent Module ---------------------------------------------
# This line is necessary to enable the automatic dependency downloading.
include(FetchContent)

# --- Global Settings --------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Link-Time Optimization for release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Set the default build output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Options ----------------------------------------------------------------
option(PRODUCTION_BUILD "Enable production build settings" OFF)

# --- Third-Party Dependencies ---------------------------------------------
# This will now automatically download and configure all libraries.
add_subdirectory(thirdparty)

# --- Project Structure ------------------------------------------------------
# Add the engine as the primary library.
add_subdirectory(engine)

# Add the examples directory.
add_subdirectory(examples)
