set(KERNEL_CFLAGS -ffreestanding -nostdlib -nodefaultlibs)
set(KERNEL_LDFLAGS -nostdlib -nodefaultlibs)
set(KERNEL_SOURCES
	libk/string.c
       	libk/vsprintf.c
	klog.c
	panic.c
)

add_subdirectory("arch/${BUILD_ARCH}")

add_executable(kernel)
target_compile_options(kernel PRIVATE ${KERNEL_CFLAGS})
target_link_options(kernel PRIVATE ${KERNEL_LDFLAGS})
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include "${CMAKE_CURRENT_LIST_DIR}/include/arch/${BUILD_ARCH}")
target_sources(kernel PRIVATE ${KERNEL_SOURCES})
set_target_properties(kernel PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sysroot/boot")
