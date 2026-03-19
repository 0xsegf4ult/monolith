add_executable(sh)

target_compile_options(sh PRIVATE -target x86_64-none-elf -ffreestanding -fno-rtti -fno-exceptions -mno-red-zone -mgeneral-regs-only -nostdlib -nostdlib++ -nodefaultlibs)
target_link_options(sh PRIVATE -fuse-ld=lld -nostdlib -nostdlib++ -nodefaultlibs -no-pie)
target_include_directories(sh PRIVATE ${CMAKE_SOURCE_DIR}/buildroot/usr/include)
target_sources(sh
        PRIVATE
        main.cpp)
set_target_properties(sh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/bin")
