add_executable(init)

target_compile_options(init PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(init PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(init PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(init
        PRIVATE
        main.c)
set_target_properties(init PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
