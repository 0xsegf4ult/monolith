#pragma once

struct file_ops
struct task;

void procfs_init();
void procfs_mkdir(const char* path);
void procfs_create(const char* path, struct file_ops* fops, void* priv_data); 
void procfs_remove(const char* path);
void procfs_register_process(struct task* proc);
void procfs_unregister_process(struct task* proc);
