extern "C" int _start()
{
	const char* init_str = "Message from userspace!";

	asm volatile("movq $6, %%rax; movq %0, %%rdi; int $0x80" : :"g"(init_str));


	const char* devname = "/dev/tty0";
	
	asm volatile("movq $0, %%rax; movq %0, %%rdi; movq $0, %%rsi; int $0x80" : : "g"(devname));
	
	int fd = -1;
	asm volatile("movl %%eax, %0" : "=g"(fd));

	asm volatile("movq $2, %%rax; movq %0, %%rdi; movq $2, %%rsi; movq $8, %%rdx; int $0x80" : : "g"(fd));

	for(;;)
		asm volatile("pause");
}
