set(target_name "compute_culling")

add_executable(${target_name} "main.cpp")

target_link_libraries(${target_name} PRIVATE framework)

project_setup_target(${target_name})

file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*"
)

set(copy_shaders_target_name "${target_name}_copy_shaders")
add_custom_target(${copy_shaders_target_name}
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${target_name}>/shaders"
    DEPENDS ${SHADER_FILES}
    COMMENT "Copying changed shaders..."
)

add_dependencies(${target_name} ${copy_shaders_target_name})
