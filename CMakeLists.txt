cmake_minimum_required(VERSION 3.10)

project(cnes VERSION 0.1.0)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)

set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)

set(C6502_BUILD_TEST OFF CACHE BOOL "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory("c6502")

if(WIN32)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
else()
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)
find_package(SDL2 REQUIRED)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE C_FILES ${SRC_DIR}/*.c)
file(GLOB_RECURSE H_FILES ${INC_DIR}/*.h)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  if(MSVC_VERSION LESS 1911)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DEBUG:FULL /Od")
  else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DEBUG:FASTLINK /Od")
  endif()
else()
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -Winit-self -Wold-style-cast -Woverloaded-virtual -Wuninitialized \
                       -Wmissing-declarations -Winit-self -pedantic -pedantic-errors -std=c++11")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

add_executable(${PROJECT_NAME} ${C_FILES} ${H_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "${INC_DIR};${SDL2_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} "c6502;SDL2::Main")

set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 11)

if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:SDL2::Core>"
          "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()
