# ============================================================================
# Project Compiler Options
# ============================================================================

add_library(project_options INTERFACE)
add_library(Project::Options ALIAS project_options)

if(MSVC)
    target_compile_options(project_options
        INTERFACE
            /utf-8
            $<$<COMPILE_LANGUAGE:CXX>:/permissive->
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
            $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    )
endif()
