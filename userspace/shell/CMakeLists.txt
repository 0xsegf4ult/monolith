add_executable(sh)

target_compile_options(sh PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib -nodefaultlibs --sysroot=${CMAKE_SOURCE_DIR}/buildroot)
target_link_options(sh PRIVATE -fuse-ld=lld -nostdlib -nodefaultlibs -no-pie)
target_include_directories(sh PRIVATE ${CMAKE_SOURCE_DIR}/buildroot/usr/include)
target_sources(sh
        PRIVATE
        main.c)
set_target_properties(sh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
