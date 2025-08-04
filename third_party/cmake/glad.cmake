add_library(third_party_glad STATIC
    "${THIRD_PARTY_ROOT}/glad/src/gl.c"
)

add_library(
    ThirdParty::glad
    ALIAS third_party_glad
)

target_include_directories(third_party_glad
    SYSTEM PUBLIC
        "${THIRD_PARTY_ROOT}/glad/include"
)

set_target_properties(third_party_glad PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    FOLDER "ThirdParty"
)

third_party_disable_warnings(third_party_glad)
