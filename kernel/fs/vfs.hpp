#pragma once

#include <lib/types.hpp>

namespace vfs
{

enum class vnode_type
{
	directory,
	file
};

struct vnode_t;
struct ventry_t;
struct file_descriptor_t;

typedef ventry_t* (*fs_lookup_t)(ventry_t*, const char*);
typedef int (*fs_mkdir_t)(ventry_t*, const char*);
typedef int (*fs_create_t)(ventry_t*, const char*);
typedef int (*fs_open_t)(vnode_t*, int);
typedef int (*fs_close_t)(int);
typedef size_t (*fs_read_t)(file_descriptor_t*, byte*, size_t);
typedef size_t (*fs_write_t)(file_descriptor_t*, const byte*, size_t);

struct vfilesystem_t
{
	fs_lookup_t lookup;
	fs_create_t create;
	fs_mkdir_t mkdir;
	fs_open_t open;	
	fs_close_t close;
	fs_read_t read;
	fs_write_t write;
	void* data;	
};

struct ventry_t
{
	char name[64];
	vnode_t* node;
	ventry_t* parent;
	ventry_t* children;
	ventry_t* sibling;
};

struct vnode_t
{
	vnode_type type;
	size_t size;
	void* data;
	vfilesystem_t* fs;
};

struct mountpoint_t
{
	
};

struct file_descriptor_t
{
	size_t read_pos;
	size_t write_pos;
	vnode_t* inode;
	int fs_id;
};

struct context_t
{
	ventry_t* root_node;	
	file_descriptor_t open_files[64];
};

void init();
int create(const char* path);
int mkdir(const char* path);

struct lookup_result
{
	ventry_t* result;
	ventry_t* parent;
	const char* basename;
};

lookup_result lookup(ventry_t* parent, const char* path);

int open(const char* path, int flags = 0);
int close(int fd);
size_t read(int fd, byte* buffer, size_t len);
size_t write(int fd, const byte* buffer, size_t len);
size_t seek(int fd, size_t offset, int flags = 0);

}
