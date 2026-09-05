cmake_minimum_required(VERSION 3.21)

project(char-clap-utils VERSION 0.2.0 LANGUAGES CXX)

add_library(char-clap-utils INTERFACE)
add_library(char-clap-utils::char-clap-utils ALIAS char-clap-utils)

target_compile_features(char-clap-utils INTERFACE cxx_std_17)
target_include_directories(char-clap-utils INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# Optional macOS WKWebView adapter for char_clap::WebUI, so a clap.webview
# plug-in also shows its UI in hosts that only offer a Cocoa window.
# Needs a CHOC checkout (choc/gui/choc_WebView.h).
set(CHAR_CLAP_UTILS_CHOC_ROOT "" CACHE PATH "CHOC checkout used by char-clap-utils::webview")
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^wasm")
    set(_char_clap_webview_default ON)
else()
    set(_char_clap_webview_default OFF)
endif()
option(CHAR_CLAP_UTILS_NATIVE_WEBVIEW "Build char-clap-utils::webview (macOS)" ${_char_clap_webview_default})

if(CHAR_CLAP_UTILS_NATIVE_WEBVIEW)
    if(NOT EXISTS "${CHAR_CLAP_UTILS_CHOC_ROOT}/choc/gui/choc_WebView.h")
        message(FATAL_ERROR
            "char-clap-utils: CHAR_CLAP_UTILS_NATIVE_WEBVIEW needs CHAR_CLAP_UTILS_CHOC_ROOT "
            "pointing at a CHOC checkout (got '${CHAR_CLAP_UTILS_CHOC_ROOT}'). "
            "Set CHAR_CLAP_UTILS_NATIVE_WEBVIEW=OFF to skip it.")
    endif()
    enable_language(OBJCXX)
    add_library(char-clap-utils-webview STATIC src/NativeWebView.mm)
    add_library(char-clap-utils::webview ALIAS char-clap-utils-webview)
    target_link_libraries(char-clap-utils-webview PUBLIC char-clap-utils)
    target_include_directories(char-clap-utils-webview PRIVATE "${CHAR_CLAP_UTILS_CHOC_ROOT}")
    target_compile_definitions(char-clap-utils-webview PUBLIC CHAR_CLAP_HAS_NATIVE_WEBVIEW=1)
    target_compile_options(char-clap-utils-webview PRIVATE -fobjc-arc)
    target_link_libraries(char-clap-utils-webview PUBLIC "-framework WebKit" "-framework AppKit")
    set_target_properties(char-clap-utils-webview PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

option(CHAR_CLAP_UTILS_BUILD_TESTS "Build char-clap-utils tests" ${PROJECT_IS_TOP_LEVEL})

if(CHAR_CLAP_UTILS_BUILD_TESTS)
    enable_testing()

    find_path(CHAR_CLAP_UTILS_CLAP_INCLUDE_DIR clap/clap.h
        HINTS
            "${CHAR_CLAP_UTILS_CLAP_ROOT}"
            "$ENV{CLAP_SDK_ROOT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../clap"
        PATH_SUFFIXES include)

    if(NOT CHAR_CLAP_UTILS_CLAP_INCLUDE_DIR)
        message(FATAL_ERROR
            "CLAP headers not found. Set CHAR_CLAP_UTILS_CLAP_ROOT to the CLAP SDK root.")
    endif()

    find_package(Threads REQUIRED)
    add_executable(char-clap-utils-tests tests/char_clap_utils_tests.cpp)
    target_link_libraries(char-clap-utils-tests PRIVATE char-clap-utils Threads::Threads)
    target_include_directories(char-clap-utils-tests PRIVATE
        "${CHAR_CLAP_UTILS_CLAP_INCLUDE_DIR}")
    # Keep assertions enabled in Release builds too.
    target_compile_options(char-clap-utils-tests PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
    add_test(NAME char-clap-utils COMMAND char-clap-utils-tests)
    set_tests_properties(char-clap-utils PROPERTIES TIMEOUT 30)
endif()
