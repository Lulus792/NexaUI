foreach(_case clean link link_option header header_import appkit)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${NEXAUI_SOURCE_DIR}/tests/dependency_audit"
            -B "${NEXAUI_BINARY_DIR}/dependency-audit-${_case}"
            -G "${NEXAUI_GENERATOR}"
            -DNEXAUI_SOURCE_DIR=${NEXAUI_SOURCE_DIR}
            -DAUDIT_CASE=${_case}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    if(_case STREQUAL "clean" OR (_case STREQUAL "appkit" AND CMAKE_HOST_APPLE))
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR "Clean dependency fixture failed:\n${_stdout}\n${_stderr}")
        endif()
    else()
        if(_case STREQUAL "link")
            set(_expected "Dependency audit: probe has unapproved LINK_LIBRARIES entry 'SDL3'")
        elseif(_case STREQUAL "link_option")
            set(_expected "Dependency audit: probe has unreviewed LINK_OPTIONS entry '-lSDL3'")
        elseif(_case STREQUAL "appkit")
            set(_expected "unapproved LINK_LIBRARIES entry '-framework")
        else()
            set(_expected "forbidden header 'sdl3/sdl.h'")
        endif()
        if(_result EQUAL 0 OR NOT _stderr MATCHES "${_expected}")
            message(FATAL_ERROR
                "Forbidden ${_case} fixture was not rejected by the audit:\n${_stdout}\n${_stderr}")
        endif()
    endif()
endforeach()
