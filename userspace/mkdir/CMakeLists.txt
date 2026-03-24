add_executable(mkdir)

target_compile_options(mkdir PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_options(mkdir PRIVATE -fuse-ld=lld -no-pie -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_libraries(mkdir PRIVATE crt0 c)
target_sources(mkdir
        PRIVATE
        main.c)
set_target_properties(mkdir PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
