set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)

add_subdirectory(
    "${THIRD_PARTY_ROOT}/glfw"
    "${CMAKE_CURRENT_BINARY_DIR}/glfw"
)

add_library(
    ThirdParty::glfw
    ALIAS glfw
)

set_target_properties(glfw PROPERTIES
    FOLDER "ThirdParty"
)

target_include_directories(glfw
    SYSTEM INTERFACE
        "${THIRD_PARTY_ROOT}/glfw/include"
)

third_party_disable_warnings(glfw)
