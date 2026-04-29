add_executable(ls)

target_compile_options(ls PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(ls PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(ls PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(ls
        PRIVATE
        main.c)
set_target_properties(ls PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
