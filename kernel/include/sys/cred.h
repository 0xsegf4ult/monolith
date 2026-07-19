#pragma once

#include <types.h>

typedef struct cred
{
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
} cred_t;

constexpr uid_t cred_superuser = 0;
