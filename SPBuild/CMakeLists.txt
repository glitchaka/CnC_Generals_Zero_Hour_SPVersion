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

    # glTF stores matrices column-major and uses column vectors. DirectXMath here
    # uses row vectors, so the semantic matrix must be transposed when loaded.
    # The old helper rebuilt the glTF matrix without that transpose, leaving
    # translation in the fourth column. XMVector3TransformCoord then divided by
    # a position-dependent W, which is what sent individual skinned vertices to
    # enormous/infinite coordinates and produced the stretched triangles.
    set(SP_DEMO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/demo_d3d11_human.cpp")
    set(SP_DEMO_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/demo_d3d11_human_fixed.cpp")
    file(READ "${SP_DEMO_SOURCE}" SP_DEMO_CODE)
    set(SP_GLTF_MATRIX_BROKEN "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[4],m[8],m[12], m[1],m[5],m[9],m[13], m[2],m[6],m[10],m[14], m[3],m[7],m[11],m[15]); }")
    set(SP_GLTF_MATRIX_FIXED  "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7], m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]); }")
    string(REPLACE "${SP_GLTF_MATRIX_BROKEN}" "${SP_GLTF_MATRIX_FIXED}" SP_DEMO_CODE_FIXED "${SP_DEMO_CODE}")
    if(SP_DEMO_CODE_FIXED STREQUAL SP_DEMO_CODE)
        message(FATAL_ERROR "No se encontro la conversion glTF defectuosa; no se compilara silenciosamente una version sin el arreglo de skinning.")
    endif()
    file(WRITE "${SP_DEMO_GENERATED}" "${SP_DEMO_CODE_FIXED}")

    add_executable(sp_demo WIN32 "${SP_DEMO_GENERATED}")
    target_include_directories(sp_demo PRIVATE ${CGLTF_DIR})
    target_link_libraries(sp_demo PRIVATE user32 d3d11 dxgi d3dcompiler windowscodecs ole32)
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
