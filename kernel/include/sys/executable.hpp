#pragma once

namespace vfs
{
struct ventry_t;
}
struct task_t;
int load_executable(const char* path, task_t* task, vfs::ventry_t* exec_dir); 
