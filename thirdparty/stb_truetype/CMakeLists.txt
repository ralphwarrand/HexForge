cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0115 NEW)
cmake_policy(SET CMP0069 NEW)
project(stb_truetype)

add_library(stb_truetype)
target_sources(stb_truetype PRIVATE "src/stb_truetype.cpp")
target_include_directories(stb_truetype PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

