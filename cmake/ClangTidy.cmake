# ============================================================================
# clang-tidy
# ============================================================================

option(
    ENABLE_CLANG_TIDY
    "Enable clang-tidy for project targets"
    OFF
)

if(ENABLE_CLANG_TIDY)
    find_program(
        CLANG_TIDY_EXE
        NAMES clang-tidy
    )

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found in PATH."
        )
    endif()
endif()


function(project_enable_clang_tidy target)
    if(NOT ENABLE_CLANG_TIDY)
        return()
    endif()

    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "project_enable_clang_tidy: "
            "target '${target}' does not exist."
        )
    endif()

    set_property(
        TARGET ${target}
        PROPERTY CXX_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
    )
endfunction()
