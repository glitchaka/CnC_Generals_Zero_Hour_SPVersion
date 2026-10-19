cmake_minimum_required(VERSION 3.24)
project(SPVersionCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sp_smoke smoke_main.cpp)

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

    add_executable(sp_demo WIN32 demo_d3d11_human.cpp)
    target_include_directories(sp_demo PRIVATE ${CGLTF_DIR})
    target_link_libraries(sp_demo PRIVATE user32 d3d11 dxgi d3dcompiler windowscodecs ole32)
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
