add_executable(pwd)

target_compile_options(pwd PRIVATE ${USERSPACE_COMPILE_OPTIONS})
target_link_options(pwd PRIVATE ${USERSPACE_LINK_OPTIONS})
target_link_libraries(pwd PRIVATE ${USERSPACE_LINK_LIBRARIES})
target_sources(pwd
        PRIVATE
        main.c)
set_target_properties(pwd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
