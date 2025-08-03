# ============================================================================
# Project Warnings
# ============================================================================

add_library(project_warnings INTERFACE)
add_library(Project::Warnings ALIAS project_warnings)

if(MSVC)
    target_compile_options(project_warnings
        INTERFACE
            /W4
    )

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(project_warnings
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()
