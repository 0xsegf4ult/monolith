#include <lib/types.hpp>
#include <lib/kstd.hpp>

// FIXME: should be securely randomized by bootloader, still useful for finding bugs
extern "C" uintptr_t __stack_chk_guard = 0xdeab12b6d60c6703;

extern "C" void __stack_chk_fail()
{
	panic("stack smashing detected");
}
