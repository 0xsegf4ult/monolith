#pragma once

namespace vfs
{

struct ventry_t;	

enum LOOKUP_FLAGS
{
        LOOKUP_PARENT = 1
};

int lookup_at(ventry_t* parent, const char* path, ventry_t** result, int flags = 0);
int lookup(const char* path, ventry_t** result, int flags = 0);

}
