cmake_minimum_required(VERSION 3.20)

project(
  bull
  VERSION 1.0
  LANGUAGES CXX
)

add_library(
  bull 
  STATIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include/core/shared_ptr.hh
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include/util/raii_guard.hh
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include/util/noncopyable.hh
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel/src/core/shared_ptr.cc
)

target_include_directories(
  bull
  SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kernel/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/src
)

target_compile_features(bull PRIVATE cxx_std_23)
