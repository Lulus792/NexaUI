# Keep the runtime link boundary explicit. Add an OS/driver library here only
# after documenting why it belongs to the self-contained core.
function(nexaui_audit_core_target target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Dependency audit: missing target ${target}")
    endif()

    # shell32 supplies the Win32 Shell's filesystem-drop transfer functions.
    # imm32 positions the OS-managed composition and candidate windows.
    # libX11 is the Linux system protocol client for the native X11 adapter.
    # libwayland-client and libxkbcommon handle the Linux compositor protocol
    # and the compositor-supplied XKB keymap; UI input routing stays in NexaUI.
    set(_allowed_links user32 shell32 imm32 m Threads::Threads X11::X11
        PkgConfig::WAYLAND_CLIENT PkgConfig::XKBCOMMON)
    # UIA, COM marshaling, BSTR/SAFEARRAY storage and SDK interface IDs are
    # Windows accessibility services, not rendering/layout/widget dependencies.
    if(WIN32)
        list(APPEND _allowed_links ole32 oleaut32 uiautomationcore uuid)
    endif()
    # AppKit supplies native macOS windows, input and pasteboard services.
    # objc is its OS runtime, needed when a plain C executable links the static
    # adapter. Neither entry is permitted on other target platforms.
    if(APPLE)
        list(APPEND _allowed_links "-framework AppKit" objc)
    endif()
    foreach(_property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_links "${target}" "${_property}")
        if(_links)
            foreach(_link IN LISTS _links)
                string(REGEX REPLACE "^\\$<LINK_ONLY:([^>]+)>$" "\\1"
                    _plain_link "${_link}")
                if(NOT _plain_link IN_LIST _allowed_links)
                    message(FATAL_ERROR
                        "Dependency audit: ${target} has unapproved ${_property} entry '${_link}'")
                endif()
            endforeach()
        endif()
    endforeach()

    foreach(_property LINK_OPTIONS INTERFACE_LINK_OPTIONS LINK_FLAGS
            LINK_DIRECTORIES INTERFACE_LINK_DIRECTORIES)
        get_target_property(_entries "${target}" "${_property}")
        if(_entries)
            message(FATAL_ERROR
                "Dependency audit: ${target} has unreviewed ${_property} entry '${_entries}'")
        endif()
    endforeach()

    # This catches the banned header-only libraries named in MASTER.md as well
    # as an accidental include of a linked framework without its link target.
    set(_forbidden_roots
        sdl sdl2 sdl3 glfw qt gtk imgui nuklear skia ft2build freetype
        harfbuzz hb pango cairo bgfx sokol raylib stb_truetype yoga nanovg
        qwidget qapplication qobject qstring)
    get_target_property(_sources "${target}" SOURCES)
    if(EXISTS "${PROJECT_SOURCE_DIR}/include/nexaui/nexaui.h")
        list(APPEND _sources "${PROJECT_SOURCE_DIR}/include/nexaui/nexaui.h")
    endif()
    foreach(_source IN LISTS _sources)
        if(_source MATCHES "^\\$<")
            message(FATAL_ERROR
                "Dependency audit: review generated source expression '${_source}'")
        endif()
        get_filename_component(_path "${_source}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${_path}")
            get_source_file_property(_generated "${_path}" GENERATED
                TARGET_DIRECTORY "${target}")
            if(_generated)
                continue() # The reviewed protocol scanner output appears at build time.
            endif()
            message(FATAL_ERROR "Dependency audit: missing source '${_path}'")
        endif()
        file(STRINGS "${_path}" _includes REGEX "^[ \t]*#[ \t]*(include|import)[ \t]*")
        foreach(_include IN LISTS _includes)
            string(REGEX REPLACE
                "^[ \t]*#[ \t]*(include|import)[ \t]*[<\"]([^>\"]+)[>\"].*$"
                "\\2" _header "${_include}")
            string(TOLOWER "${_header}" _header)
            string(REGEX MATCH "^[^/.]+" _root "${_header}")
            if(_root IN_LIST _forbidden_roots)
                message(FATAL_ERROR
                    "Dependency audit: ${_path} includes forbidden header '${_header}'")
            endif()
        endforeach()
    endforeach()
endfunction()
