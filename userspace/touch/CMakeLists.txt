add_executable(touch)

target_compile_options(touch PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_options(touch PRIVATE -fuse-ld=lld -no-pie -nostdlib --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_libraries(touch PRIVATE crt0 c)
target_sources(touch
        PRIVATE
        main.c)
set_target_properties(touch PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
