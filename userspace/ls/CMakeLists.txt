add_executable(ls)

target_compile_options(ls PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_options(ls PRIVATE -fuse-ld=lld -no-pie -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_libraries(ls PRIVATE crt0 c)
target_sources(ls
        PRIVATE
        main.c)
set_target_properties(ls PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
