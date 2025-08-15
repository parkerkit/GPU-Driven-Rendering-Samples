# ============================================================================
# Library: framework
# ============================================================================

set(lib_name "framework")

add_library(${lib_name}
    STATIC
        "inc/aabb.h"
        "inc/application.h"
        "inc/buffer.h"
        "inc/camera.h"
        "inc/camera_controller.h"
        "inc/drawable.h"
        "inc/framework.h"
        "inc/gpu_timer.h"
        "inc/image.h"
        "inc/primitive.h"
        "inc/shader.h"
        "inc/shapes.h"
        "inc/texture.h"
        "inc/vertex_array.h"
        "src/aabb.cpp"
        "src/application.cpp"
        "src/buffer.cpp"
        "src/camera.cpp"
        "src/camera_controller.cpp"
        "src/drawable.cpp"
        "src/gpu_timer.cpp"
        "src/image.cpp"
        "src/shader.cpp"
        "src/shapes.cpp"
        "src/texture.cpp"
        "src/vertex_array.cpp"
)

target_include_directories(${lib_name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")

target_link_libraries(${lib_name}
     PUBLIC
        ThirdParty::glfw
        ThirdParty::glad
        ThirdParty::imgui
        ThirdParty::glm
        ThirdParty::stb_image
        ThirdParty::meshoptimizer
)

project_setup_target(${lib_name})

# ============================================================================
# Executable: framework_test
# ============================================================================

set(exe_name "framework_test")

add_executable(${exe_name} "test/main.cpp")

target_link_libraries(${exe_name} PRIVATE ${lib_name})

project_setup_target(${exe_name})

file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/*"
)

add_custom_target(framework_test_copy_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders"
        "$<TARGET_FILE_DIR:${exe_name}>/shaders"
    DEPENDS ${SHADER_FILES}
    COMMENT "Copying changed shaders..."
)

add_dependencies(${exe_name} framework_test_copy_shaders)
