#include <types.h>

enum SPAWN_FLAGS
{
	SPAWN_SETPGID = 1
};

pid_t sys_spawn(const char** argv, const char** envp, uint64_t flags);
void sys_exit(int status);
pid_t sys_waitpid(pid_t pid, int* status, int options);
pid_t sys_getpid();
int sys_setsid();
pid_t sys_getpgid(pid_t pid);
int sys_setpgid(pid_t pgid);
int sys_chdir(const char* path);
int sys_getcwd(char* buffer, size_t length);
uid_t sys_getuid();
int sys_setuid(uid_t uid);
gid_t sys_getgid();
int sys_setgid(gid_t gid);
