add_executable(touch)

target_compile_options(touch PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(touch PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(touch PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(touch
        PRIVATE
        main.c)
set_target_properties(touch PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
