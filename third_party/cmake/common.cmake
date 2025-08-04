# ============================================================================
# Third-Party Common Utilities
# ============================================================================

function(third_party_disable_warnings target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "third_party_disable_warnings: target '${target}' does not exist"
        )
    endif()

    # 第三方库的警告不作为错误。
    set_property(
        TARGET ${target}
        PROPERTY COMPILE_WARNING_AS_ERROR OFF
    )

    if(MSVC)
        # 某些第三方库（例如 GLFW）会主动添加 /W3。
        #
        # 如果直接再添加 /w：
        #
        #   /W3 /w
        #
        # MSVC 会产生：
        #
        #   D9025: overriding '/W3' with '/w'
        #
        # 因此先移除 Target 自己已有的 Warning Level 选项，
        # 再统一使用 /w。

        get_target_property(
            third_party_compile_options
            ${target}
            COMPILE_OPTIONS
        )

        if(third_party_compile_options)
            list(
                FILTER third_party_compile_options
                EXCLUDE
                REGEX "^/W[0-4]$"
            )

            list(
                FILTER third_party_compile_options
                EXCLUDE
                REGEX "^/Wall$"
            )

            set_property(
                TARGET ${target}
                PROPERTY COMPILE_OPTIONS
                ${third_party_compile_options}
            )
        endif()

        target_compile_options(
            ${target}
            PRIVATE
                /w
        )
    elseif(
        CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
        OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
    )
        target_compile_options(
            ${target}
            PRIVATE
                -w
        )
    endif()
endfunction()
