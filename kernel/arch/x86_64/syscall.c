#include <arch/x86_64/syscall.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/irq.h>
#include <fs/vfs_sys.h>
#include <mm/vm_syscall.h>
#include <net/socket_sys.h>
#include <sched/task.h>
#include <sched/task_sys.h>
#include <sched/task_sleep.h>
#include <sys/clock.h>
#include <sys/smp.h>
#include <errno.h>
#include <types.h>

#include <klog.h>

int sys_arch_prctl(int op, uint64_t arg)
{
	struct task* task = smp_current_task();
	switch(op)
	{
	case ARCH_SET_FS:
		task->tls_base = arg;
		native_set_tls(arg);
		return 0;
	default:
		return -EINVAL;
	}
}

void syscall_handler(struct interrupt_frame* frame)
{
	uint64_t num = frame->rax;

	switch(num)
	{
	case SYS_OPEN:
		frame->rax = (uint64_t)sys_open((const char*)frame->rdi, (int)frame->rsi);
		break;
	case SYS_OPENAT:
		frame->rax = (uint64_t)sys_openat((int)frame->rdi, (const char*)frame->rsi, (int)frame->rdx);
		break;
	case SYS_CLOSE:
		frame->rax = (uint64_t)sys_close((int)frame->rdi);
		break;
	case SYS_READ:
		frame->rax = (uint64_t)sys_read((int)frame->rdi, (byte*)frame->rsi, (size_t)frame->rdx);
		break;
	case SYS_WRITE:
		frame->rax = (uint64_t)sys_write((int)frame->rdi, (const byte*)frame->rsi, (size_t)frame->rdx);
		break;
	case SYS_SEEK:
		frame->rax = (uint64_t)sys_seek((int)frame->rdi, (off_t)frame->rsi, (int)frame->rdx);
		break;
	case SYS_DUP:
		frame->rax = (uint64_t)sys_dup((int)frame->rdi);
		break;
	case SYS_IOCTL:
		frame->rax = (uint64_t)sys_ioctl((int)frame->rdi, frame->rsi, frame->rdx);
		break;
	case SYS_STAT:
		frame->rax = (uint64_t)sys_stat((const char*)frame->rdi, (struct stat*)frame->rsi);
		break;
	case SYS_FSTAT:
		frame->rax = (uint64_t)sys_fstat((int)frame->rdi, (struct stat*)frame->rsi);
		break;
	case SYS_GETDENTS:
		frame->rax = (uint64_t)sys_getdents((int)frame->rdi, (byte*)frame->rsi, (size_t)frame->rdx);
		break;
	case SYS_MKDIR:
		frame->rax = (uint64_t)sys_mkdir((const char*)frame->rdi, (mode_t)frame->rsi);
		break;
	case SYS_MOUNT:
		frame->rax = (uint64_t)sys_mount((const char*)frame->rdi, (const char*)frame->rsi, (const char*)frame->rdx);
		break;
	case SYS_UMOUNT:
		frame->rax = (uint64_t)sys_umount((const char*)frame->rdi);
		break;
	case SYS_MMAP:
		frame->rax = (uint64_t)sys_mmap((void*)frame->rdi, (size_t)frame->rsi, (int)frame->rdx, (int)frame->rcx, (int)frame->r8, (off_t)frame->r9);
		break;
	case SYS_MUNMAP:
		frame->rax = (uint64_t)sys_munmap((void*)frame->rdi, (size_t)frame->rsi);
		break;
	case SYS_MPROTECT:
		frame->rax = (uint64_t)sys_mprotect((void*)frame->rdi, (size_t)frame->rsi, (int)frame->rdx);
		break;
	case SYS_SPAWN:
		frame->rax = (uint64_t)sys_spawn((const char**)frame->rdi, (const char**)frame->rsi, frame->rdx);
		break;
	case SYS_EXIT:
		sys_exit((int)frame->rdi);
		break;
	case SYS_WAITPID:
		frame->rax = (uint64_t)sys_waitpid((pid_t)frame->rdi, (int*)frame->rsi, (int)frame->rdx);
		break;
	case SYS_GETPID:
		frame->rax = (uint64_t)sys_getpid();
		break;
	case SYS_SETSID:
		frame->rax = (uint64_t)sys_setsid();
		break;
	case SYS_GETPGID:
		frame->rax = (uint64_t)sys_getpgid((pid_t)frame->rdi);
		break;
	case SYS_SETPGID:
		frame->rax = (uint64_t)sys_setpgid((pid_t)frame->rdi);
		break;
	case SYS_CHDIR:
		frame->rax = (uint64_t)sys_chdir((const char*)frame->rdi);
		break;
	case SYS_GETCWD:
		frame->rax = (uint64_t)sys_getcwd((char*)frame->rdi, (size_t)frame->rsi);
		break;
	case SYS_GETUID:
		frame->rax = (uint64_t)sys_getuid();
		break;
	case SYS_SETUID:
		frame->rax = (uint64_t)sys_setuid((uid_t)frame->rdi);
		break;
	case SYS_GETGID:
		frame->rax = (uint64_t)sys_getgid();
		break;
	case SYS_SETGID:
		frame->rax = (uint64_t)sys_setgid((gid_t)frame->rdi);
		break;
	case SYS_CLOCK_GETTIME:
		frame->rax = (uint64_t)sys_clock_gettime((clockid_t)frame->rdi, (struct timespec*)frame->rsi);
		break;
	case SYS_CLOCK_NANOSLEEP:
		frame->rax = (uint64_t)sys_clock_nanosleep((clockid_t)frame->rdi, (int)frame->rsi, (const struct timespec*)frame->rdx, (struct timespec*)frame->rcx);
		break;
	case SYS_ARCH_PRCTL:
		frame->rax = (uint64_t)sys_arch_prctl((int)frame->rdi, frame->rsi);
		break;
	case SYS_SOCKET:
		frame->rax = (uint64_t)sys_socket((int)frame->rdi, (int)frame->rsi, (int)frame->rdx);
		break;
	case SYS_BIND:
		frame->rax = (uint64_t)sys_bind((int)frame->rdi, (const struct sockaddr*)frame->rsi, (socklen_t)frame->rdx);
		break;
	case SYS_RECVFROM:
		frame->rax = (uint64_t)sys_recvfrom((int)frame->rdi, (byte*)frame->rsi, (size_t)frame->rdx, (int)frame->rcx, (struct sockaddr*)frame->r8, (socklen_t*)frame->r9);
		break;
	case SYS_SENDTO:
		frame->rax = (uint64_t)sys_sendto((int)frame->rdi, (const byte*)frame->rsi, (size_t)frame->rdx, (int)frame->rcx, (const struct sockaddr*)frame->r8, (socklen_t)frame->r9);
		break;
	default:
		klog("unknown syscall: %u\n", frame->rax);
		frame->rax = (uint64_t)-ENOSYS;
		break;
	}	
}
