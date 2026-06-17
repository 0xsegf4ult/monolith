#pragma once

#include <lib/types.hpp>

using uid_t = int32_t;
using gid_t = int32_t;

struct cred_t
{
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
};

constexpr uid_t cred_superuser = 0;

