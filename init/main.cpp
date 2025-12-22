extern "C" int _start()
{
	const char* init_str = "Message from userspace!";

	asm volatile("movl $6, %%esi; movq %0, %%rdi; int $0x80" : :"g"(init_str));

	for(;;)
		asm volatile("pause");
}
