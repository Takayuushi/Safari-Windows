cmake_minimum_required(VERSION 3.24)
project(Chronos LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "Chronos for Windows targets Windows x64 only.")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(WEBKIT_SDK_DIR "" CACHE PATH "Extracted WebKit SDK directory containing include/, lib/, runtime/")
option(CHRONOS_COPY_RUNTIME "Copy WebKit runtime DLLs next to Chronos.exe" ON)

if (NOT WEBKIT_SDK_DIR OR NOT IS_DIRECTORY "${WEBKIT_SDK_DIR}")
    message(FATAL_ERROR "Set WEBKIT_SDK_DIR to the extracted WebKit SDK directory.")
endif ()

set(_webkit_lib "${WEBKIT_SDK_DIR}/lib/WebKit2.lib")
if (NOT EXISTS "${_webkit_lib}")
    message(FATAL_ERROR "WebKit2.lib not found at: ${_webkit_lib}")
endif ()

if (NOT EXISTS "${WEBKIT_SDK_DIR}/include/WebKit/WKView.h")
    message(FATAL_ERROR "WebKit/WKView.h not found in include directory: ${WEBKIT_SDK_DIR}/include")
endif ()

add_executable(Chronos WIN32
    src/main.cpp
    src/browser.cpp
    src/downloads.cpp
    src/util.cpp
    src/webkit_host.cpp
)

target_include_directories(Chronos PRIVATE
    src
    "${WEBKIT_SDK_DIR}/include"
)

target_compile_definitions(Chronos PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(Chronos PRIVATE
    "${_webkit_lib}"
    user32
    gdi32
    gdiplus
    dwmapi
    uxtheme
    shell32
    comdlg32
    advapi32
    ole32
    mfplat
    mf
    mfuuid
)

if (MSVC)
    target_compile_options(Chronos PRIVATE /W4 /permissive- /utf-8)
endif ()

if (CHRONOS_COPY_RUNTIME)
    add_custom_command(TARGET Chronos POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEBKIT_SDK_DIR}/runtime" "$<TARGET_FILE_DIR:Chronos>"
        COMMENT "Copying WebKit runtime next to Chronos.exe"
    )
endif ()
