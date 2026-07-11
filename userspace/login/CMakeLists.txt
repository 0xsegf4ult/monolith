add_executable(login)

target_compile_options(login PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(login PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(login PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(login
        PRIVATE
        main.c)
set_target_properties(login PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
