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
    set(CGLTF_DIR "${CMAKE_BINARY_DIR}/_deps/cgltf")
    set(CGLTF_HEADER "${CGLTF_DIR}/cgltf.h")
    file(MAKE_DIRECTORY "${CGLTF_DIR}")
    if(NOT EXISTS "${CGLTF_HEADER}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/jkuhlmann/cgltf/85cd62382dfea638278962690cf515023f33ed00/cgltf.h"
            "${CGLTF_HEADER}"
            STATUS CGLTF_STATUS
            TLS_VERIFY ON)
        list(GET CGLTF_STATUS 0 CGLTF_CODE)
        if(NOT CGLTF_CODE EQUAL 0)
            message(FATAL_ERROR "No se pudo descargar cgltf.h: ${CGLTF_STATUS}")
        endif()
    endif()

    add_executable(sp_demo WIN32 demo_d3d11_characters.cpp)
    target_link_libraries(sp_demo PRIVATE sp_core user32 d3d11 dxgi d3dcompiler windowscodecs ole32)
    target_include_directories(sp_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../GeneralsMD/Code/GameEngine/Include
        ${CGLTF_DIR}
    )
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
