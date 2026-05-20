cmake_minimum_required(VERSION 3.10)
project(tb_probing_tool)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)
include(FetchContent)

FetchContent_Declare(
    chesslib
    GIT_REPOSITORY https://github.com/winapiadmin/chesslib.git
    GIT_TAG main
)
if(DEFINED BUILD_TESTING)
    set(_OLD_BUILD_TESTING "${BUILD_TESTING}")
    set(_HAD_BUILD_TESTING TRUE)
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(chesslib)
if(_HAD_BUILD_TESTING)
    set(BUILD_TESTING "${_OLD_BUILD_TESTING}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_TESTING CACHE)
endif()
find_package(LibLZMA QUIET)

if(NOT LibLZMA_FOUND)
    message(STATUS "LibLZMA not found. Fetching from xz.git...")
    
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(XZ_TOOL_XZ OFF CACHE BOOL "" FORCE)
    set(XZ_TOOL_XZDEC OFF CACHE BOOL "" FORCE)
    set(XZ_TOOL_LZMADEC OFF CACHE BOOL "" FORCE)
    set(XZ_TOOL_LZMAINFO OFF CACHE BOOL "" FORCE)
    set(XZ_DOC OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        xz
        GIT_REPOSITORY https://git.tukaani.org/xz.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(xz)
    add_library(LibLZMA::LibLZMA ALIAS liblzma)
endif()
add_library(syzygy_probe "syzygy/tbprobe.cpp")
add_library(gaviota_probe "gaviota/tbprobe.cpp" "gaviota/tbprobe2.cpp")
target_include_directories(syzygy_probe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/syzygy ${chesslib_SOURCE_DIR})
target_link_libraries(syzygy_probe PUBLIC chesslib)
target_include_directories(gaviota_probe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gaviota ${chesslib_SOURCE_DIR})
target_link_libraries(gaviota_probe PUBLIC chesslib LibLZMA::LibLZMA)
if (NOT LibLZMA_FOUND)

target_include_directories(gaviota_probe PRIVATE
    ${xz_SOURCE_DIR}/src/liblzma/api
)
endif()
include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.5.2
    )
    FetchContent_MakeAvailable(doctest)

    # --- Test executable ---
    add_executable(test_syzygy syzygy/main.cpp)
    add_executable(test_gaviota gaviota/main.cpp)

    target_link_libraries(test_syzygy PRIVATE syzygy_probe doctest::doctest)
    target_link_libraries(test_gaviota PRIVATE gaviota_probe doctest::doctest)

    add_test(NAME test_syzygy COMMAND test_syzygy)
    add_test(NAME test_gaviota COMMAND test_gaviota)
    if (UNIX AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(SANITIZERS "" CACHE STRING "sanitizers such as undefined,address")
    
        if (NOT "${SANITIZERS}" STREQUAL "")
            add_compile_options(-fsanitize=${SANITIZERS} -fno-omit-frame-pointer)
            add_link_options(-fsanitize=${SANITIZERS})
        endif()
    endif()

endif()

