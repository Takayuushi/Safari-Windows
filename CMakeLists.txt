cmake_minimum_required(VERSION 3.24)
project(Safari LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "Safari for Windows targets Windows x64 only.")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(WEBKIT_SDK_DIR "" CACHE PATH "Extracted WebKit SDK directory containing include/, lib/, runtime/")
option(SAFARI_COPY_RUNTIME "Copy WebKit runtime DLLs next to Safari.exe" ON)

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

add_executable(Safari WIN32
    src/main.cpp
    src/browser.cpp
    src/downloads.cpp
    src/util.cpp
    src/webkit_host.cpp
)

target_include_directories(Safari PRIVATE
    src
    "${WEBKIT_SDK_DIR}/include"
)

target_compile_definitions(Safari PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(Safari PRIVATE
    "${_webkit_lib}"
    user32
    gdi32
    gdiplus
    dwmapi
    uxtheme
    shell32
    comdlg32
    advapi32
)

if (MSVC)
    target_compile_options(Safari PRIVATE /W4 /permissive- /utf-8)
endif ()

if (SAFARI_COPY_RUNTIME)
    add_custom_command(TARGET Safari POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEBKIT_SDK_DIR}/runtime" "$<TARGET_FILE_DIR:Safari>"
        COMMENT "Copying WebKit runtime next to Safari.exe"
    )
endif ()
