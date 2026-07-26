#pragma once

#include <types.h>

struct task;
int elf_exec(int execfd, struct task* task, virtaddr_t* out_entry);

typedef virtaddr_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint8_t Elf64_UnsignedChar;

enum Elf64_EIdent
{
	EI_MAG0,
	EI_MAG1,
	EI_MAG2,
	EI_MAG3,
	EI_CLASS,
	EI_DATA,
	EI_VERSION,
	EI_OSABI,
	EI_ABIVERSION,
	EI_PAD,
	EI_NIDENT = 16
};

constexpr Elf64_UnsignedChar ELF_MAG0 = 0x7f;
constexpr Elf64_UnsignedChar ELF_MAG1 = 'E';
constexpr Elf64_UnsignedChar ELF_MAG2 = 'L';
constexpr Elf64_UnsignedChar ELF_MAG3 = 'F';
constexpr Elf64_UnsignedChar ELF_CLASS64 = 2;
constexpr Elf64_UnsignedChar ELF_DATA2LSB = 1;
constexpr Elf64_Half EM_X86_64 = 62;
constexpr Elf64_Word EV_CURRENT = 1;

enum Elf64_EType
{
	ET_NONE,
	ET_REL,
	ET_EXEC,
	ET_DYN,
	ET_CORE,
	ET_LOOS = 0xFE00,
	ET_HIOS = 0xFEFF,
	ET_LOPROC = 0xFF00,
	ET_HIPROC = 0xFFFF
};

typedef struct 
{
	Elf64_UnsignedChar e_ident[EI_NIDENT];
	Elf64_Half e_type;
	Elf64_Half e_machine;
        Elf64_Word e_version;
        Elf64_Addr e_entry;
        Elf64_Off e_phoff;
        Elf64_Off e_shoff;
        Elf64_Word e_flags;
        Elf64_Half e_ehsize;
        Elf64_Half e_phentsize;
        Elf64_Half e_phnum;
        Elf64_Half e_shentsize;
        Elf64_Half e_shnum;
        Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct 
{
        Elf64_Word sh_name;
        Elf64_Word sh_type;
        Elf64_Xword sh_flags;
        Elf64_Addr sh_addr;
        Elf64_Off sh_offset;
        Elf64_Xword sh_size;
        Elf64_Word sh_link;
        Elf64_Word sh_info;
        Elf64_Xword sh_addralign;
        Elf64_Xword sh_entsize;
} Elf64_Shdr;

enum Elf_AType
{
        AT_NULL,
        AT_IGNORE,
        AT_EXECFD,
        AT_PHDR,
        AT_PHENT,
        AT_PHNUM,
        AT_PAGESZ,
        AT_BASE,
	AT_FLAGS,
	AT_ENTRY,
        AT_NOTELF,
        AT_UID,
        AT_EUID,
        AT_GID,
        AT_EGID
};

enum Elf_PType
{
        PT_NULL,
        PT_LOAD,
        PT_DYNAMIC,
        PT_INTERP,
        PT_NOTE,
        PT_SHLIB,
        PT_PHDR,
        PT_TLS
};

enum Elf_PFlags
{
        PF_NONE = 0,
        PF_X = 1,
        PF_W = 2,
        PF_R = 4,
        PF_MASKPROC = 0xf0000000
};

typedef struct 
{
        Elf64_Word p_type;
        Elf64_Word p_flags;
        Elf64_Off p_offset;
        Elf64_Addr p_vaddr;
        Elf64_Addr p_paddr;
        Elf64_Xword p_filesz;
        Elf64_Xword p_memsz;
        Elf64_Xword p_align;
} Elf64_Phdr;

typedef struct
{
        Elf64_Word st_name;
        Elf64_UnsignedChar st_info;
        Elf64_UnsignedChar st_other;
        Elf64_Half st_shndx;
        Elf64_Addr st_value;
        Elf64_Xword st_size;
} Elf64_Sym;

typedef struct 
{
        Elf64_Addr r_offset;
        Elf64_Xword r_info;
} Elf64_Rel;

typedef struct 
{
        Elf64_Addr r_offset;
        Elf64_Xword r_info;
        Elf64_Sxword r_addend;
} Elf64_Rela;

typedef struct 
{
        Elf64_Sxword d_tag;
        union
        {
                Elf64_Xword d_val;
                Elf64_Addr d_ptr;
        } d_un;
} Elf64_Dyn;
