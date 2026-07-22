#include <types.h>

struct stat;

int sys_open(const char* path, int flags);
int sys_openat(int fd, const char* path, int flags);
int sys_close(int fd);
ssize_t sys_read(int fd, byte* buffer, size_t length);
ssize_t sys_write(int fd, const byte* buffer, size_t length);
off_t sys_seek(int fd, off_t offset, int flags);
int sys_dup(int fd);
int sys_ioctl(int fd, uint64_t op, uint64_t arg);
int sys_stat(const char* path, struct stat* buffer);
int sys_fstat(int fd, struct stat* buffer);
ssize_t sys_getdents(int fd, byte* buffer, size_t length);
int sys_mkdir(const char* name, mode_t mode);
int sys_mount(const char* source, const char* target, const char* fsname);
int sys_umount(const char* target);
