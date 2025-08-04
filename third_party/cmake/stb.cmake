add_library(third_party_stb_image STATIC
    "${THIRD_PARTY_ROOT}/src/stb_image.cpp"
)

add_library(
    ThirdParty::stb_image
    ALIAS third_party_stb_image
)

target_include_directories(third_party_stb_image
    SYSTEM PUBLIC
        "${THIRD_PARTY_ROOT}/stb"
)

set_target_properties(third_party_stb_image PROPERTIES
    FOLDER "ThirdParty"
)

third_party_disable_warnings(third_party_stb_image)


add_library(third_party_stb_image_write STATIC
    "${THIRD_PARTY_ROOT}/src/stb_image_write.cpp"
)

add_library(
    ThirdParty::stb_image_write
    ALIAS third_party_stb_image_write
)

target_include_directories(third_party_stb_image_write
    SYSTEM PUBLIC
        "${THIRD_PARTY_ROOT}/stb"
)

set_target_properties(third_party_stb_image_write PROPERTIES
    FOLDER "ThirdParty"
)

third_party_disable_warnings(third_party_stb_image_write)
