cmake_minimum_required(VERSION 3.16)

# ─────────────────────────────────────────────
# Project configuration
# ─────────────────────────────────────────────

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")

set(CMAKE_OSX_DEPLOYMENT_TARGET
    "15.6"
    CACHE STRING "Minimum macOS version"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CODESIGN_IDENTITY
    "-"
    CACHE STRING "Codesign identity"
)

project(pluginplayground
    LANGUAGES C CXX OBJC
)


# ─────────────────────────────────────────────
# Swingset
# ─────────────────────────────────────────────

add_library(Swingset STATIC
    Swingset/PPOptions.m
    Swingset/SOPPObservableDictionary.m
)

target_include_directories(Swingset PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Swingset PUBLIC
    "-framework Foundation"
)


# ─────────────────────────────────────────────
# Core library
# ─────────────────────────────────────────────

add_library(syphon STATIC
    syphon/target.c
    syphon/stepping.c
    syphon/breakpoint.c
    syphon/mach_exc.c
    syphon/process.c
    syphon/exe.c
    syphon/PPOptionsLoader.m
)

target_include_directories(syphon PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/syphon"
)

target_link_libraries(syphon PUBLIC
    Swingset
    "-framework CoreFoundation"
    "-framework Security"
    "-framework Foundation"
)


# ─────────────────────────────────────────────
# Code signing
# ─────────────────────────────────────────────

function(syphon_codesign target)

    add_custom_command(
        TARGET ${target}
        POST_BUILD

        COMMAND codesign
            -f
            -s "${CODESIGN_IDENTITY}"
            --entitlements
                "${CMAKE_SOURCE_DIR}/Master.entitlements"
            "$<TARGET_FILE:${target}>"

        COMMENT "Codesigning ${target}"
    )

endfunction()


# ─────────────────────────────────────────────
# Fangs
# ─────────────────────────────────────────────

add_executable(fangs
    syphon/main.m
)

target_link_libraries(fangs PRIVATE
    syphon
)

syphon_codesign(fangs)


# ─────────────────────────────────────────────
# Privilege grant tool
# ─────────────────────────────────────────────

add_executable(grant
    grant/amfid_handler.m
    grant/main.m
)

target_include_directories(grant PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/grant"
)

target_link_libraries(grant PRIVATE
    "-framework Foundation"
)


# ─────────────────────────────────────────────
# Configurator
# ─────────────────────────────────────────────

option(BUILD_CONFIGURATOR
    "Build the Configurator App"
    ON
)

if(BUILD_CONFIGURATOR)

    # ─────────────────────────────────────────
    # XIB sources
    # ─────────────────────────────────────────

    set(CONFIGURATOR_XIBS
        configurator/MainMenu.xib
        configurator/Panels/PPListEditorPanel.xib
        configurator/Panels/PPTweakEditorPanel.xib
        configurator/Views/PPMainViewView.xib
    )


    # ─────────────────────────────────────────
    # Configurator application
    # ─────────────────────────────────────────

    add_executable(configurator MACOSX_BUNDLE

        configurator/main.m
        configurator/AppDelegate.m

        configurator/Services/PPDaemonStatus.m
        configurator/Services/PPTweaksCollection.m

        configurator/Controllers/PPListEditorController.m
        configurator/Controllers/PPMainViewController.m
        configurator/Controllers/PPTweakEditorController.m

        configurator/Drill.png
        configurator/AppIcon.icns
    )


    # ─────────────────────────────────────────
    # Static resources
    # ─────────────────────────────────────────

    set_source_files_properties(
        configurator/Drill.png
        configurator/AppIcon.icns

        PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )


    # ─────────────────────────────────────────
    # Link libraries
    # ─────────────────────────────────────────

    target_link_libraries(configurator PRIVATE
        Swingset
        "-framework AppKit"
    )


    # ─────────────────────────────────────────
    # Bundle properties
    # ─────────────────────────────────────────

    set_target_properties(configurator PROPERTIES

        MACOSX_DEPLOYMENT_TARGET
            "15.6"

        MACOSX_BUNDLE_GUI_IDENTIFIER
            "com.pluginplayground.configurator"

        MACOSX_BUNDLE_SHORT_VERSION_STRING
            "1.1"

        MACOSX_BUNDLE_BUNDLE_VERSION
            "1.1.0"

        MACOSX_BUNDLE_BUNDLE_NAME
            "Plugin Playground Configurator"

        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/configurator/Info.plist"

        BUILD_RPATH
            "@executable_path/."

        INSTALL_RPATH
            "@executable_path/."
    )


    # ─────────────────────────────────────────
    # Compile XIBs -> NIBs
    # ─────────────────────────────────────────

    set(XCODE_DEVELOPER_DIR
    	"/Applications/Xcode.app/Contents/Developer"
    )

    set(IBTOOL
    	"/Applications/Xcode.app/Contents/Developer/usr/bin/ibtool"
    )

    set(CONFIGURATOR_NIBS)

    foreach(XIB ${CONFIGURATOR_XIBS})

        get_filename_component(
            XIB_NAME
            "${XIB}"
            NAME_WE
        )

        set(NIB_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/configurator_nibs/${XIB_NAME}.nib"
        )

        add_custom_command(
            OUTPUT
                "${NIB_OUTPUT}"

            COMMAND
                ${CMAKE_COMMAND}
                -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/configurator_nibs"

	    COMMAND
    		"${IBTOOL}"
    		--compile
    		"${NIB_OUTPUT}"
    		"${CMAKE_CURRENT_SOURCE_DIR}/${XIB}"

            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/${XIB}"

            COMMENT
                "Compiling ${XIB} -> ${XIB_NAME}.nib"
        )

        list(APPEND CONFIGURATOR_NIBS
            "${NIB_OUTPUT}"
        )

    endforeach()


    # ─────────────────────────────────────────
    # Copy compiled NIBs into the app bundle
    # ─────────────────────────────────────────

    add_custom_command(
        TARGET configurator
        POST_BUILD

        COMMAND
            ${CMAKE_COMMAND}
            -E make_directory
            "$<TARGET_BUNDLE_DIR:configurator>/Contents/Resources"

        COMMENT
            "Preparing Configurator resources"
    )


    foreach(NIB ${CONFIGURATOR_NIBS})

        get_filename_component(
            NIB_NAME
            "${NIB}"
            NAME
        )

        add_custom_command(
            TARGET configurator
            POST_BUILD

            COMMAND
                ${CMAKE_COMMAND}
                -E copy_if_different
                "${NIB}"
                "$<TARGET_BUNDLE_DIR:configurator>/Contents/Resources/${NIB_NAME}"

            COMMENT
                "Installing ${NIB_NAME}"
        )

    endforeach()


    # ─────────────────────────────────────────
    # Make Configurator depend on the NIBs
    # ─────────────────────────────────────────

    add_custom_target(configurator_nibs
        DEPENDS ${CONFIGURATOR_NIBS}
    )

    add_dependencies(configurator
        configurator_nibs
    )


    # ─────────────────────────────────────────
    # Install
    # ─────────────────────────────────────────

    install(TARGETS configurator
        BUNDLE DESTINATION Applications
    )

endif()


# ─────────────────────────────────────────────
# Install command-line tools
# ─────────────────────────────────────────────

install(TARGETS fangs grant
    RUNTIME DESTINATION bin
)