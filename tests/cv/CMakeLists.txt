cmake_minimum_required(VERSION 3.20)
project(CatEngineCVTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Bcore/src/main/cpp/catEngine/cv
)

add_executable(test_board_detection
    test_board_detection.cpp
    ../../Bcore/src/main/cpp/catEngine/cv/BoardDetectionEngine.cpp
)

if (MSVC)
    target_compile_options(test_board_detection PRIVATE /W3 /EHsc)
else()
    target_compile_options(test_board_detection PRIVATE -Wall -Wextra)
endif()

