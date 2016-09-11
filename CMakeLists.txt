project(CxxProfiler)

cmake_minimum_required(VERSION 3.2)

if(WIN32)
  # link automatically to qtmain.lib on Windows
  cmake_policy(SET CMP0020 NEW)
endif()

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)

set_property(GLOBAL PROPERTY USE_FOLDERS OFF)

add_definitions("-D_UNICODE -DUNICODE -D_SCL_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4200") # nonstandard extension used : zero-sized array in struct/union
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4204") # nonstandard extension used : non-constant aggregate initializer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /GF /Gy /GS- /GR- /GL")

set(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL:NO")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:ICF /OPT:REF")

macro(use_pch HEADER SOURCE FILES)
  foreach(FILE ${FILES})
    set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "/Yu${HEADER} /FI${HEADER}")
  endforeach()
  set_source_files_properties(${SOURCE} PROPERTIES COMPILE_FLAGS "/Yc${HEADER}")
endmacro(use_pch)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CxxProfiler_BINARY_DIR}/Build")

add_subdirectory(CxxProfiler)
