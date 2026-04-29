add_executable(sh)

target_compile_options(sh PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(sh PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(sh PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(sh
        PRIVATE
        main.c)
set_target_properties(sh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
