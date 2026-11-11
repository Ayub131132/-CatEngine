cmake_minimum_required(VERSION 3.20)
project(CatEnginePhysicsTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Bcore/src/main/cpp/catEngine/v2
)

add_executable(test_physics_engine
    test_physics_engine.cpp
    ../../Bcore/src/main/cpp/catEngine/v2/PhysicsEngineV2.cpp
)

if (MSVC)
    target_compile_options(test_physics_engine PRIVATE /W3 /EHsc)
else()
    target_compile_options(test_physics_engine PRIVATE -Wall -Wextra)
endif()

