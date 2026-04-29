add_executable(cat)

target_compile_options(cat PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(cat PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(cat PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(cat
        PRIVATE
        main.c)
set_target_properties(cat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
