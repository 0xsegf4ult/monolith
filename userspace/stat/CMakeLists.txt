add_executable(stat)

target_compile_options(stat PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(stat PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(stat PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(stat
        PRIVATE
        main.c)
set_target_properties(stat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
