add_executable(dhcpcl)

target_compile_options(dhcpcl PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(dhcpcl PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(dhcpcl PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(dhcpcl
        PRIVATE
        main.c)
set_target_properties(dhcpcl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sysroot/usr/bin")
