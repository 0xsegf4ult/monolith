#include <lib/elf.hpp>
#include <lib/types.hpp>

bool elf_validate(Elf64_Ehdr* ehdr)
{
	if(!ehdr)
		return false;

	if(ehdr->e_ident[EI_MAG0] != ELF_MAG0)
		return false;
	if(ehdr->e_ident[EI_MAG1] != ELF_MAG1)
		return false;
	if(ehdr->e_ident[EI_MAG2] != ELF_MAG2)
		return false;
	if(ehdr->e_ident[EI_MAG3] != ELF_MAG3)
		return false;

	if(ehdr->e_ident[EI_CLASS] != ELF_CLASS64)
		return false;
	if(ehdr->e_ident[EI_DATA] != ELF_DATA2LSB)
		return false;
	if(ehdr->e_machine != EM_X86_64)
		return false;
	if(ehdr->e_ident[EI_VERSION] != EV_CURRENT)
		return false;
	if(ehdr->e_type != ET_EXEC)
		return false;

	return true;
}
