cmake_minimum_required(VERSION 3.21)

project(VibeLegion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VL_WITH_DIRECTML "Build against the DirectML flavour of ONNX Runtime (Radeon iGPU)" OFF)
option(VL_BUILD_TESTS "Build unit tests (portable core, runs on any OS)" ON)
set(VL_ORT_ROOT "${CMAKE_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH
    "Extracted ONNX Runtime release (include/ + lib/). Leave empty to use find_package (vcpkg).")

# ---------------------------------------------------------------- warnings / flags
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /MP /wd4100 /wd4505)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    # Zen 5 has AVX-512, but AVX2 keeps the binary portable; ONNX Runtime picks its
    # own kernels at run time anyway, our code is not the hot path.
    add_compile_options($<$<CONFIG:Release,RelWithDebInfo>:/O2> $<$<CONFIG:Release,RelWithDebInfo>:/arch:AVX2>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
    if(WIN32)
        add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    endif()
endif()

# ---------------------------------------------------------------- ONNX Runtime
add_library(vl_onnxruntime INTERFACE)
if(VL_ORT_ROOT AND EXISTS "${VL_ORT_ROOT}/include/onnxruntime_cxx_api.h")
    target_include_directories(vl_onnxruntime INTERFACE "${VL_ORT_ROOT}/include")
    if(WIN32)
        target_link_libraries(vl_onnxruntime INTERFACE "${VL_ORT_ROOT}/lib/onnxruntime.lib")
        set(VL_ORT_DLLS "${VL_ORT_ROOT}/lib/onnxruntime.dll")
        if(EXISTS "${VL_ORT_ROOT}/lib/onnxruntime_providers_shared.dll")
            list(APPEND VL_ORT_DLLS "${VL_ORT_ROOT}/lib/onnxruntime_providers_shared.dll")
        endif()
        if(EXISTS "${VL_ORT_ROOT}/lib/DirectML.dll")
            list(APPEND VL_ORT_DLLS "${VL_ORT_ROOT}/lib/DirectML.dll")
        endif()
    else()
        find_library(VL_ORT_LIB onnxruntime PATHS "${VL_ORT_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(vl_onnxruntime INTERFACE "${VL_ORT_LIB}")
        set_property(TARGET vl_onnxruntime PROPERTY INTERFACE_LINK_OPTIONS "-Wl,-rpath,${VL_ORT_ROOT}/lib")
    endif()
    message(STATUS "ONNX Runtime: ${VL_ORT_ROOT}")
else()
    find_package(onnxruntime CONFIG REQUIRED)  # vcpkg: `onnxruntime` port
    target_link_libraries(vl_onnxruntime INTERFACE onnxruntime::onnxruntime)
    message(STATUS "ONNX Runtime: find_package")
endif()

# ---------------------------------------------------------------- portable core
add_library(vl_core STATIC
    src/core/json.cpp
    src/core/image.cpp
    src/core/night_vision.cpp
    src/core/detection.cpp
    src/core/distance.cpp
    src/core/gestures.cpp
    src/core/face_db.cpp
    src/core/settings.cpp
)
target_include_directories(vl_core PUBLIC src)

add_library(vl_ml STATIC
    src/ml/ort_runtime.cpp
    src/ml/models.cpp
    src/vision/pipeline.cpp
)
target_link_libraries(vl_ml PUBLIC vl_core vl_onnxruntime)
if(VL_WITH_DIRECTML)
    target_compile_definitions(vl_ml PRIVATE VL_WITH_DIRECTML)
endif()
find_package(Threads REQUIRED)
target_link_libraries(vl_ml PUBLIC Threads::Threads)

# ---------------------------------------------------------------- Windows app
if(WIN32)
    add_executable(VibeLegion WIN32
        src/app/main.cpp
        src/app/app.cpp
        src/app/actions.cpp
        src/platform/win/camera_mf.cpp
        src/platform/win/system.cpp
        src/platform/win/tray.cpp
        src/ui/gfx.cpp
        src/ui/gui.cpp
        src/ui/theme.cpp
        src/ui/window_base.cpp
        src/ui/main_window.cpp
        src/ui/pages.cpp
        src/ui/flyout.cpp
        res/app.rc
    )
    target_link_libraries(VibeLegion PRIVATE vl_ml
        user32 shell32 gdi32 ole32 oleaut32 uuid advapi32 comctl32
        dwmapi uxtheme d2d1 dwrite windowscodecs
        mf mfplat mfreadwrite mfuuid strmiids
        crypt32 powrprof wtsapi32 psapi shlwapi version comdlg32)
    if(MSVC)
        target_link_options(VibeLegion PRIVATE /MANIFEST:NO)  # manifest comes from app.rc
    else()
        target_link_options(VibeLegion PRIVATE -municode -static)  # no MinGW runtime DLLs
    endif()

    # Ship ONNX Runtime DLLs and the models next to the exe.
    foreach(dll IN LISTS VL_ORT_DLLS)
        add_custom_command(TARGET VibeLegion POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:VibeLegion>")
    endforeach()
    add_custom_command(TARGET VibeLegion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VibeLegion>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/models" "$<TARGET_FILE_DIR:VibeLegion>/models")

    install(TARGETS VibeLegion RUNTIME DESTINATION .)
    install(FILES ${VL_ORT_DLLS} DESTINATION .)
    install(DIRECTORY models/ DESTINATION models FILES_MATCHING PATTERN "*.onnx")
endif()

# ---------------------------------------------------------------- tests / tools
if(VL_BUILD_TESTS)
    enable_testing()
    add_executable(vl_tests tests/test_core.cpp)
    target_link_libraries(vl_tests PRIVATE vl_core)
    add_test(NAME core COMMAND vl_tests)

    # Runs the real models on PPM images: `vl_selftest <models dir> <image.ppm>...`
    add_executable(vl_selftest tests/selftest_models.cpp)
    target_link_libraries(vl_selftest PRIVATE vl_ml)
endif()
