foreach(example_name IN ITEMS texture buffer)
    set(target_name "bindless_${example_name}")

    add_executable(${target_name} "${example_name}_main.cpp")
    target_link_libraries(${target_name} PRIVATE framework)
    project_setup_target(${target_name})

    set(vertex_shader "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${example_name}_bindless.vert")
    set(fragment_shader "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${example_name}_bindless.frag")
    set(copy_shaders_target_name "${target_name}_copy_shaders")
    add_custom_target(${copy_shaders_target_name}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target_name}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${vertex_shader}" "${fragment_shader}"
            "$<TARGET_FILE_DIR:${target_name}>/shaders"
        DEPENDS "${vertex_shader}" "${fragment_shader}"
        COMMENT "Copying ${target_name} shaders..."
    )
    add_dependencies(${target_name} ${copy_shaders_target_name})
endforeach()
