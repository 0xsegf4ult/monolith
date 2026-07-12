#pragma once

namespace vfs
{
struct fs_file_ops;
}

struct task_t;

void procfs_init();
void procfs_mkdir(const char* path);
void procfs_create(const char* path, vfs::fs_file_ops* fops, void* priv_data = nullptr); 
void procfs_remove(const char* path);
void procfs_register_process(task_t* proc);
void procfs_unregister_process(task_t* proc);
