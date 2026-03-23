add_executable(init)

target_compile_options(init PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib -nodefaultlibs)
target_link_options(init PRIVATE -fuse-ld=lld -nostdlib -nodefaultlibs -no-pie)
target_link_libraries(init PRIVATE crt0 c)
target_sources(init
        PRIVATE
        main.c)
set_target_properties(init PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
