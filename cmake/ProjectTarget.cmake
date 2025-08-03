# ============================================================================
# Project Target Setup
# ============================================================================

function(project_setup_target target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "project_setup_target: target '${target}' does not exist."
        )
    endif()

    target_link_libraries(${target}
        PRIVATE
            Project::Warnings
            Project::Options
    )

    project_enable_clang_tidy(${target})
endfunction()
