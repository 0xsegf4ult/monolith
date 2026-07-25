add_executable(ping)

target_compile_options(ping PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(ping PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(ping PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(ping
        PRIVATE
        main.c)
set_target_properties(ping PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sysroot/usr/bin")
