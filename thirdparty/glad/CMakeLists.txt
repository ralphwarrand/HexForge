cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0069 NEW)

project(glad)

add_library(glad)
target_sources(glad PRIVATE src/glad.c)
target_include_directories(glad PUBLIC include)