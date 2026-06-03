cmake_minimum_required(VERSION 3.16)
project(pluginplayground C CXX OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CODESIGN_IDENTITY "-" CACHE STRING "Codesign identity (default: ad-hoc)")
set(INSTALL_PREFIX "/opt/pluginplayground" CACHE PATH "Installation prefix")

function(syphon_codesign target)
  add_custom_command(TARGET ${target} POST_BUILD
        COMMAND codesign -f -s "${CODESIGN_IDENTITY}"
                --entitlements "${CMAKE_SOURCE_DIR}/Master.entitlements"
                --timestamp=none
                $<TARGET_FILE:${target}>
        COMMENT "codesigning ${target} (identity: ${CODESIGN_IDENTITY})"
    )
endfunction()

# ── Core library ──────────────────────────────────────────────
add_library(syphon STATIC
    syphon/target.c
    syphon/stepping.c
    syphon/breakpoint.c
    syphon/mach_exc.c
    syphon/process.c
    syphon/exe.c
)

target_include_directories(syphon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/syphon>
    $<INSTALL_INTERFACE:include/syphon>
)
target_link_libraries(syphon PUBLIC
    "-framework CoreFoundation"
    "-framework Security"
)

# ── Tracers (add new executables here) ────────────────────────
add_executable(fangs syphon/main.c)
target_link_libraries(fangs PRIVATE syphon)
syphon_codesign(fangs)

# ── Privilege grant tool (amfid hook + fangs launcher) ────────
add_executable(grant
    grant/amfid_handler.m
    grant/main.m
)
target_include_directories(grant PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/grant")
target_link_libraries(grant PRIVATE
    "-framework Foundation"
)

# ── Configurator ─────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG        v1.16.1
)
set(SLINT_STYLE "native" CACHE STRING "Slint widget style")
set(SLINT_FEATURE_STYLE_NATIVE ON CACHE BOOL "Enable native widget style")
set(SLINT_FETCH_CONTENT ON CACHE BOOL "" FORCE)

# Persist Rust cargo artifacts outside .build so clean CMake builds reuse them
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.build-cargo")
if(EXISTS "${CMAKE_BINARY_DIR}/cargo")
    if(NOT IS_SYMLINK "${CMAKE_BINARY_DIR}/cargo")
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/cargo")
        file(CREATE_LINK "${CMAKE_SOURCE_DIR}/.build-cargo" "${CMAKE_BINARY_DIR}/cargo" SYMBOLIC)
    endif()
else()
    file(CREATE_LINK "${CMAKE_SOURCE_DIR}/.build-cargo" "${CMAKE_BINARY_DIR}/cargo" SYMBOLIC)
endif()
FetchContent_MakeAvailable(Slint)

add_executable(configurator
    configurator/main.cpp
    configurator/controller.cpp
    configurator/options.cpp
    configurator/tweaks.cpp
    configurator/configurator.slint
)
slint_target_sources(configurator configurator/configurator.slint)
set_target_properties(configurator PROPERTIES
    CXX_STANDARD 17
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.pluginplayground.configurator"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_BUNDLE_NAME "Plugin Playground Configurator"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/configurator/Info.plist.in"
    BUILD_RPATH "@executable_path/."
    INSTALL_RPATH "@executable_path/."
)
target_link_libraries(configurator PRIVATE Slint::Slint "-framework CoreFoundation")

add_custom_command(TARGET configurator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/_deps/slint-build/api/cpp/libslint_cpp.dylib"
        "$<TARGET_FILE_DIR:configurator>"
    COMMENT "Copying libslint_cpp.dylib into configurator.app bundle"
)

# ── Install ──────────────────────────────────────────────────
install(TARGETS fangs grant RUNTIME DESTINATION bin)
install(TARGETS configurator
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION Applications
)
