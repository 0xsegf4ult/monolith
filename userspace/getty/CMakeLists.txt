add_executable(getty)

target_compile_options(getty PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(getty PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(getty PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(getty
        PRIVATE
        main.c)
set_target_properties(getty PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
