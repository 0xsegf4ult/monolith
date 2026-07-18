#pragma once

#include <sys/mutex.h>
#include <libk/list.h>
#include <types.h>

struct page_table;
struct file_descriptor;

enum vm_flags_t : uint32_t
{
	VM_FLAG_NONE		= 0,
	VM_FLAG_ALLOCATE	= 1,
	VM_FLAG_COW		= 2,
	VM_FLAG_FILE		= 4,
	VM_FLAG_DEVICE		= 8,
	VM_FLAG_OWNER 		= 16
};

enum vm_prot_t : uint32_t
{
	PROT_NONE		= 0,
	PROT_READ		= 1,
	PROT_WRITE		= 2,
	PROT_EXEC		= 4,
	PROT_USER		= 8,
	PROT_UNCACHED		= 16,
	PROT_WRITECOMBINE	= 32
};

enum pf_flag_t : uint32_t
{
	VM_FAULT_PRESENT	= 1,
	VM_FAULT_WRITE		= 2,
	VM_FAULT_USER		= 4,
	VM_FAULT_FETCH		= 8
};

struct vm_space;
struct vm_object;

struct vm_object_ops
{
	bool (*fault)(struct vm_object* obj, virtaddr_t addr, uint32_t fault_flags);
};

struct vm_object
{
	virtaddr_t base;
	size_t length;
	uint32_t prot;
	uint32_t flags;
	struct file_descriptor* file;
	off_t offset;
	struct vm_object_ops* vm_ops;
	struct vm_space* space;
	list_node_t list_node;
};

struct vm_space
{
	list_head_t objects;
	virtaddr_t start_address;
	virtaddr_t end_address;

	struct page_table* mmu_root;
	mutex_t lock;

	size_t mapped_anon;
	size_t mapped_file;
	size_t resident_anon;
	size_t resident_file;
};
