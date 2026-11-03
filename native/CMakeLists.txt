cmake_minimum_required(VERSION 3.22)
project(snakemod LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(NearBranch ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG 5dfc8546954ce3b3198132ab13fddb89ee92cdd7
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(dobby)

add_library(snakemod SHARED mod.cpp)
target_compile_options(snakemod PRIVATE -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(snakemod PRIVATE dobby_static log android dl)
set_target_properties(snakemod PROPERTIES OUTPUT_NAME "snakemod")
