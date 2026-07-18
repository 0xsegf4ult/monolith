#include <types.h>
#include <panic.h>

// FIXME: should be securely randomized by bootloader, still useful for finding bugs
uintptr_t __stack_chk_guard = 0xdeab12b6d60c6703;

void __stack_chk_fail()
{
	panic("stack smashing detected");
}
