add_library(c)

target_compile_options(c PRIVATE -target x86_64-none-elf -ffreestanding -mno-red-zone -mgeneral-regs-only -nostdlib -nodefaultlibs -fPIC)
target_link_options(c PRIVATE -fuse-ld=lld -nostdlib -nodefaultlibs)
target_include_directories(c PUBLIC ${CMAKE_SOURCE_DIR}/libc/include)
target_sources(c PRIVATE
	exit.c
	printf.c
	string.c)
