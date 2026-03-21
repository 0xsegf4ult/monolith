add_executable(sh)

target_compile_options(sh PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_options(sh PRIVATE -fuse-ld=lld -no-pie -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_libraries(sh PRIVATE crt0 c)
target_sources(sh
        PRIVATE
        main.c)
set_target_properties(sh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
