cmake_minimum_required(VERSION 3.24)

project(siege VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIEGE_FETCH_SDL3 "Download SDL3 when a suitable installation is not found" ON)

find_package(SDL3 3.2 CONFIG QUIET)

if(NOT SDL3_FOUND)
    if(NOT SIEGE_FETCH_SDL3)
        message(FATAL_ERROR "SDL3 was not found and SIEGE_FETCH_SDL3 is OFF")
    endif()

    include(FetchContent)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_executable(siege
    src/main.cpp
    src/core/game.cpp
    src/core/perception.cpp
    src/core/targeting.cpp
    src/core/simulation.cpp
    src/client/frame_animation.cpp
    src/world/world.cpp
    src/world/unit.cpp
    src/world/zone.cpp
    src/client/debug_renderer.cpp
    src/client/renderer.cpp
    src/client/texture_cache.cpp
    src/client/world_transform.cpp
)

target_include_directories(siege PRIVATE src)
target_link_libraries(siege PRIVATE SDL3::SDL3)

add_custom_command(
    TARGET siege POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:siege>/assets"
    COMMENT "Copying local runtime assets"
)

if(MSVC)
    target_compile_options(siege PRIVATE /W4 /permissive-)
else()
    target_compile_options(siege PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(siege_core_tests
        tests/core_tests.cpp
    src/core/perception.cpp
    src/core/targeting.cpp
        src/core/simulation.cpp
        src/world/unit.cpp
        src/world/world.cpp
        src/world/zone.cpp
    )
    target_include_directories(siege_core_tests PRIVATE src)
    if(MSVC)
        target_compile_options(siege_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(siege_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME siege_core_tests COMMAND siege_core_tests)
endif()
