extern "C" int _start()
{
	asm volatile("movl $6, %eax; int $0x80");

	for(;;)
		asm volatile("pause");
}
