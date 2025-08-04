add_library(third_party_imgui STATIC
    "${THIRD_PARTY_ROOT}/imgui/imgui.cpp"
    "${THIRD_PARTY_ROOT}/imgui/imgui_draw.cpp"
    "${THIRD_PARTY_ROOT}/imgui/imgui_tables.cpp"
    "${THIRD_PARTY_ROOT}/imgui/imgui_widgets.cpp"

    "${THIRD_PARTY_ROOT}/imgui/backends/imgui_impl_glfw.cpp"
    "${THIRD_PARTY_ROOT}/imgui/backends/imgui_impl_opengl3.cpp"
)

add_library(
    ThirdParty::imgui
    ALIAS third_party_imgui
)

target_include_directories(third_party_imgui
    SYSTEM PUBLIC
        "${THIRD_PARTY_ROOT}/imgui"
        "${THIRD_PARTY_ROOT}/imgui/backends"
)

target_link_libraries(third_party_imgui
    PUBLIC
        ThirdParty::glfw
)

set_target_properties(third_party_imgui PROPERTIES
    FOLDER "ThirdParty"
)

third_party_disable_warnings(third_party_imgui)
