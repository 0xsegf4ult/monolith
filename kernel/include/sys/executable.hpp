#pragma once

namespace vfs
{
struct ventry_t;
}
struct thread_t;
int load_executable(const char* path, thread_t* thr, vfs::ventry_t* exec_dir); 
