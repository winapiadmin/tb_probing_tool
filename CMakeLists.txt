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
add_library(syzygy_probe "syzygy/tbprobe.cpp")
target_include_directories(syzygy_probe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${chesslib_SOURCE_DIR})
target_link_libraries(syzygy_probe PUBLIC chesslib)
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
    add_executable(tester syzygy/main.cpp)

    target_link_libraries(tester PRIVATE syzygy_probe doctest::doctest)

    add_test(NAME tester COMMAND tester)
    if (UNIX AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(SANITIZERS "" CACHE STRING "sanitizers such as undefined,address")
    
        if (NOT "${SANITIZERS}" STREQUAL "")
            add_compile_options(-fsanitize=${SANITIZERS} -fno-omit-frame-pointer)
            add_link_options(-fsanitize=${SANITIZERS})
        endif()
    endif()

endif()

