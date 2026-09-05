# char_clap_stage_web_ui(<target>
#     SOURCE <directory containing index.html and main.js>
#     COMPOST_ROOT <compost checkout>
#     COMPONENTS <compost component names, e.g. knob scope popup>)
#
# Copies the plug-in's web UI plus the requested compost components (and the
# shared compost helpers they import) into
# ${CMAKE_CURRENT_BINARY_DIR}/<target>-resources/ui, creates a
# <target>-resources custom target, and sets <target>_RESOURCE_DIRECTORY in the
# caller's scope for make_clapfirst_plugins(RESOURCE_DIRECTORY ...).
function(char_clap_stage_web_ui target)
    cmake_parse_arguments(UI "" "SOURCE;COMPOST_ROOT" "COMPONENTS" ${ARGN})
    if(NOT UI_SOURCE OR NOT UI_COMPOST_ROOT)
        message(FATAL_ERROR "char_clap_stage_web_ui: SOURCE and COMPOST_ROOT are required")
    endif()
    if(NOT EXISTS "${UI_COMPOST_ROOT}/src/utils.js")
        message(FATAL_ERROR "char_clap_stage_web_ui: no compost checkout at ${UI_COMPOST_ROOT}")
    endif()

    set(output "${CMAKE_CURRENT_BINARY_DIR}/${target}-resources")
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}-resources.stamp")
    set(component_commands)
    set(component_files)
    foreach(component IN LISTS UI_COMPONENTS)
        set(file "${UI_COMPOST_ROOT}/src/components/compost-${component}.js")
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "char_clap_stage_web_ui: unknown compost component '${component}' (${file})")
        endif()
        list(APPEND component_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${file}" "${output}/ui/compost/components/compost-${component}.js")
        list(APPEND component_files "${file}")
    endforeach()

    file(GLOB internal_files CONFIGURE_DEPENDS "${UI_COMPOST_ROOT}/src/internal/*.js")

    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output}/ui/compost/components"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${UI_SOURCE}" "${output}/ui"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${output}/ui/.DS_Store"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${UI_COMPOST_ROOT}/src/parameter-scale.js" "${output}/ui/compost/parameter-scale.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${UI_COMPOST_ROOT}/src/utils.js" "${output}/ui/compost/utils.js"
        # compost components import shared helpers from src/internal; stage the
        # whole directory so a compost refactor cannot silently break the UI
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${UI_COMPOST_ROOT}/src/internal" "${output}/ui/compost/internal"
        ${component_commands}
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS
            "${UI_SOURCE}/index.html"
            "${UI_SOURCE}/main.js"
            "${UI_COMPOST_ROOT}/src/parameter-scale.js"
            "${UI_COMPOST_ROOT}/src/utils.js"
            ${internal_files}
            ${component_files}
        VERBATIM)
    add_custom_target(${target}-resources DEPENDS "${stamp}")
    set(${target}_RESOURCE_DIRECTORY "${output}" PARENT_SCOPE)
endfunction()

# Adds the macOS adapter to an existing target. The project must enable OBJCXX.
function(char_clap_add_native_webview target choc_root)
    if(NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^wasm")
        return()
    endif()
    if(NOT EXISTS "${choc_root}/choc/gui/choc_WebView.h")
        message(FATAL_ERROR "char_clap_add_native_webview needs a CHOC checkout")
    endif()
    set(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/NativeWebView.mm")
    target_sources(${target} PRIVATE "${source}")
    set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(${target} PRIVATE "${choc_root}")
    target_compile_definitions(${target} PUBLIC CHAR_CLAP_HAS_NATIVE_WEBVIEW=1)
    target_link_libraries(${target} PRIVATE "-framework WebKit" "-framework AppKit")
endfunction()
