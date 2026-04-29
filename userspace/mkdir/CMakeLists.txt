add_executable(mkdir)

target_compile_options(mkdir PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(mkdir PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(mkdir PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(mkdir
        PRIVATE
        main.c)
set_target_properties(mkdir PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
