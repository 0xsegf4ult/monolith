add_executable(tee)

target_compile_options(tee PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(tee PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(tee PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(tee
        PRIVATE
        main.c)
set_target_properties(tee PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
