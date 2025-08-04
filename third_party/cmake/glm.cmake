add_library(third_party_glm INTERFACE)

add_library(
    ThirdParty::glm
    ALIAS third_party_glm
)

target_include_directories(third_party_glm
    SYSTEM INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/glm"
)
