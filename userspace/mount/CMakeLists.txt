add_executable(mount)

target_compile_options(mount PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(mount PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(mount PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(mount
        PRIVATE
        main.c)
set_target_properties(mount PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
