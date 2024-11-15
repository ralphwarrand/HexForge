cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0069 NEW)
cmake_policy(SET CMP0115 NEW)
project(stb_image)

add_library(stb_image)
target_sources(stb_image PRIVATE "src/stb_image.cpp")
target_include_directories(stb_image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")



