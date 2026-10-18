cmake_minimum_required(VERSION 3.24)
project(SPVersionCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sp_core STATIC
    ../GeneralsMD/Code/GameEngine/Source/SporePrototype/SporeVoxelWorld.cpp
    ../GeneralsMD/Code/GameEngine/Source/SporePrototype/SporeCreature.cpp
    ../GeneralsMD/Code/GameEngine/Source/GameLogic/Spore/SporeEvolution.cpp
)

target_include_directories(sp_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../GeneralsMD/Code/GameEngine/Include
)

if(MSVC)
    target_compile_options(sp_core PRIVATE /W4 /permissive-)
endif()

add_executable(sp_smoke smoke_main.cpp)
target_link_libraries(sp_smoke PRIVATE sp_core)
target_include_directories(sp_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../GeneralsMD/Code/GameEngine/Include
)

if(WIN32)
    add_executable(sp_demo WIN32 demo_softbody.cpp)
    target_link_libraries(sp_demo PRIVATE sp_core user32 gdi32)
    target_include_directories(sp_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../GeneralsMD/Code/GameEngine/Include
    )
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
