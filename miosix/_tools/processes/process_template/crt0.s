/***************************************************************************
 *   Copyright (C) 2012-2024 by Terraneo Federico                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

.syntax unified
.arch armv7-m
.thumb

/**
 * call global constructors and destructors
 * uses non-standard calling convention, as it is called only from _start
 * expects the pointer to the start of the function pointer area in r4 and
 * the pointer to the end of it in r5
 */
.section .text
.type	call, %function
call:
	push {lr}
	cmp  r4, r5
	beq  .L100
.L101:
	ldr  r3, [r4], #4
	blx  r3
	cmp  r5, r4
	bne  .L101
.L100:
	pop  {pc}

/**
 * _start, program entry point
 */
.section .text
.global _start
.type _start, %function
_start:
	/* save argc, argv, envp for when we call main */
	push {r0,r1,r2}
	/* call C++ global constructors */
	ldr  r0, .L200
	ldr  r1, .L200+4
	ldr  r4, [r9, r0]
	ldr  r5, [r9, r1]
	bl   call
	ldr  r0, .L200+8
	ldr  r1, .L200+12
	ldr  r4, [r9, r0]
	ldr  r5, [r9, r1]
	bl   call
	/* get back argc,argv,envp and store envp in the environ variable */
	pop  {r0,r1,r2}
	ldr  r3, .L200+16
	ldr  r3, [r9, r3]
	str  r2, [r3]
	/* call main */
	bl   main
	mov  r6, r0
	/* atexit */
	bl   __call_exitprocs
	/* for now we don't call function pointers in __fini_array as nobody seems
	   to use these. Even C++ global destructors are registered through atexit
	   and the implementation of exit() in newlib doesn't touch __fini_array
	   either, so doing it here would even be inconsistent with exit() */
	/* terminate the program */
	mov  r0, r6
	b    _exit
.L200:
	.word	__preinit_array_end(GOT)
	.word	__preinit_array_start(GOT)
	.word	__init_array_start(GOT)
	.word	__init_array_end(GOT)
	.word	environ(GOT)

/**
 * open, open a file
 * \param path file name
 * \param file access mode
 * \param mode access permisions
 * \return file descriptor or -1 if errors
 */
.section .text.open
.global open
.type open, %function
open:
	movs r3, #2
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * close, close a file
 * \param fd file descriptor
 * \return 0 on success, -1 on failure
 */
.section .text.close
.global close
.type close, %function
close:
	movs r3, #3
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * read, read from file
 * \param fd file descriptor
 * \param buf data to be read
 * \param size buffer length
 * \return number of read bytes or -1 if errors
 */
.section .text.read
.global	read
.type	read, %function
read:
	movs r3, #4
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * write, write to file
 * \param fd file descriptor
 * \param buf data to be written
 * \param size buffer length
 * \return number of written bytes or -1 if errors
 */
.section .text.write
.global	write
.type	write, %function
write:
	movs r3, #5
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * lseek
 * \param fd file descriptor, passed in r0
 * \param pos moving offset, passed in r3,r2 as it is a long long
 * \param whence, SEEK_SET, SEEK_CUR or SEEK_END, passed in the stack
 * \return absolute position after seek on success, -1LL on failure
 */
.section .text.lseek
.global lseek
.type lseek, %function
lseek:
	ldr  r12, [sp] /* Whence moved to 4th syscall parameter (r12) */
	movs r1, r3    /* Upper 32bit of pos moved to 2nd syscall parameter (r1) */
	movs r3, #6
	svc  0
	cmp  r0, #0
	sbcs r3, r1, #0 /* 64 bit negative check */
	blt  syscallfailed64
	bx   lr

/**
 * stat
 * \param path path to file or directory
 * \param pstat pointer to struct stat
 * \return 0 on success, -1 on failure
 */
.section .text.stat
.global stat
.type stat, %function
stat:
	movs r3, #7
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * lstat
 * \param path path to file or directory
 * \param pstat pointer to struct stat
 * \return 0 on success, -1 on failure
 */
.section .text.lstat
.global lstat
.type lstat, %function
lstat:
	movs r3, #8
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * fstat
 * \param fd file descriptor
 * \param pstat pointer to struct stat
 * \return 0 on success, -1 on failure
 */
.section .text.fstat
.global fstat
.type fstat, %function
fstat:
	movs r3, #9
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * fcntl
 * \param fd file descriptor
 * \param cmd operation to perform
 * \param opt optional third parameter
 * \return -1 on failure, an operation dependent return value otherwise
 */
.section .text.fcntl
.global fcntl
.type fcntl, %function
fcntl:
	movs r3, #10
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * ioctl
 * \param fd file descriptor
 * \param cmd operation to perform
 * \param arg optional third parameter
 * \return -1 on failure, an operation dependent return value otherwise
 */
.section .text.ioctl
.global ioctl
.type ioctl, %function
ioctl:
	movs r3, #11
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * isatty
 * \param fd file descriptor
 * \return 1 if fd is associated with a terminal, 0 if not, -1 on failure
 */
.section .text.isatty
.global isatty
.type isatty, %function
isatty:
	movs r3, #12
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * getcwd
 * \param fd file descriptor
 * \param buf pointer to buffer where current directory will be written
 * \param size buffer size
 * \return pointer on success, NULL on failure
 */
.section .text.getcwd
.global getcwd
.type getcwd, %function
getcwd:
	movs r3, #13
	svc  0
	cmp  r0, #0
	blt  .L300
	bx   lr
.L300:
	/* tail call */
	b    __getcwdfailed

/**
 * chdir
 * \param path pointer to path where to change directory
 * \return 0 on success, -1 on failure
 */
.section .text.chdir
.global chdir
.type chdir, %function
chdir:
	movs r3, #14
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * getdents
 * \param fd file descriptor
 * \param buf pointer to buffer where directory entries will be written
 * \param size buffer size
 * \return number of bytes wrtten, 0 on end of directory, -1 on failure
 */
.section .text.getdents
.global getdents
.type getdents, %function
getdents:
	movs r3, #15
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * mkdir
 * \param path pointer to path of directory to create
 * \param mode directory mode
 * \return 0 on success, -1 on failure
 */
.section .text.mkdir
.global mkdir
.type mkdir, %function
mkdir:
	movs r3, #16
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * rmdir
 * \param path pointer to path of directory to remove
 * \return 0 on success, -1 on failure
 */
.section .text.rmdir
.global rmdir
.type rmdir, %function
rmdir:
	movs r3, #17
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * link
 * \param oldpath existing file path
 * \param newpath new file path
 * \return 0 on success, -1 on failure
 */
.section .text.link
.global link
.type link, %function
link:
	movs r3, #18
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * unlink
 * \param path path of file/directory to remove
 * \return 0 on success, -1 on failure
 */
.section .text.unlink
.global unlink
.type unlink, %function
unlink:
	movs r3, #19
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * symlink
 * \param target symlink destination
 * \param linkpath file name of symlink to be created
 * \return 0 on success, -1 on failure
 */
.section .text.symlink
.global symlink
.type symlink, %function
symlink:
	movs r3, #20
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * readlink
 * \param path path to the symlink
 * \param buf pointer where the symlink target will be stored
 * \param size buffer size
 * \return 0 on success, -1 on failure
 */
.section .text.readlink
.global readlink
.type readlink, %function
readlink:
	movs r3, #21
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * rename
 * \param oldpath existing file path
 * \param newpath new file path
 * \return 0 on success, -1 on failure
 */
.section .text.rename
.global rename
.type rename, %function
rename:
	movs r3, #22
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/* TODO: missing syscalls: chmod, fchmod, chown, fchown, lchown */

/**
 * dup
 * \param fd file descriptor to duplicate
 * \return the new file descriptor on success, -1 on failure
 */
.section .text.dup
.global dup
.type dup, %function
dup:
	movs r3, #28
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * dup2
 * \param oldfd file descriptor to duplicate
 * \param newfd old file descriptor will be duplicated to this file descriptor
 * \return the new file descriptor on success, -1 on failure
 */
.section .text.dup2
.global dup2
.type dup2, %function
dup2:
	movs r3, #29
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * pipe
 * \param fds[2] file descriptors
 * \return 0 on success, -1 on failure
 */
.section .text.pipe
.global pipe
.type pipe, %function
pipe:
	movs r3, #30
	svc  0
	cmp  r1, #0
	blt  .L400
	str  r2, [r0]
	str  r12, [r0, #4]
	bx   lr
.L400:
	movs r0, r1
	b    syscallfailed32

/* TODO: missing syscalls: access */

/**
 * miosix::getTime, nonstandard syscall
 * \return long long time in nanoseconds, relative to clock monotonic
 */
.section .text._ZN6miosix7getTimeEv
.global _ZN6miosix7getTimeEv
.type _ZN6miosix7getTimeEv, %function
_ZN6miosix7getTimeEv:
	movs r0, #4
	movs r3, #32
	svc  0
	bx   lr

/**
 * clock_gettime
 * \param clockid which clock
 * \param tp struct timespec*
 * \return in Miosix this syscall always returns 0, if the clockid is wrong
 * the default clock is returned
 */
.section .text.clock_gettime
.global clock_gettime
.type clock_gettime, %function
clock_gettime:
	push {r4, lr}
	movs r4, r1
	movs r3, #32
	svc  0
	adr  r3, .L500
	ldrd r2, [r3]
	bl   __aeabi_ldivmod
	strd r0, [r4]
	str  r2, [r4, #8]
	movs r0, #0
	pop  {r4, pc}
	.align 2
.L500:
	.word 1000000000
	.word 0

/**
 * clock_settime
 * \param clockid which clock
 * \param tp struct timespec*
 * \return 0 on success or a positive error code
 */
.section .text.clock_settime
.global clock_settime
.type clock_settime, %function
clock_settime:
	push {r4}
	ldr  r4, .L600
	ldrd r2, [r1]
	umull r2, r12, r2, r4
	ldr  r1, [r1, #8]
	mla  r3, r3, r4, r12
	adds r2, r2, r1
	adc  r1, r3, r1, asr #31
	movs r3, #33
	svc  0
	pop  {r4}
	bx   lr
	.align 2
.L600:
	.word 1000000000

/**
 * miosix::nanoSleepUntil, nonstandard syscall
 * \param absolute sleep time in nanoseconds, relative to clock monotonic
 */
.section .text._ZN6miosix14nanoSleepUntilEx
.global _ZN6miosix14nanoSleepUntilEx
.type _ZN6miosix14nanoSleepUntilEx, %function
_ZN6miosix14nanoSleepUntilEx:
	mov  r12, #260
	movs r3, #34
	svc  0
	bx   lr

/**
 * clock_nanosleep
 * \param clockid which clock
 * \param flags absolute or relative
 * \param req struct timespec* with sleep time
 * \param rem ignored
 * \return 0 on success or a positive error code
 */
.section .text.clock_nanosleep
.global clock_nanosleep
.type clock_nanosleep, %function
clock_nanosleep:
	push {r4}
	orr  r12, r0, r1, lsl #6
	ldr  r4, .L700
	ldrd r0, [r2]
	umull r0, r3, r0, r4
	ldr  r2, [r2, #8]
	mla  r1, r1, r4, r3
	adds r0, r0, r2
	adc  r1, r1, r2, asr #31
	movs r3, #34
	svc  0
	pop  {r4}
	bx   lr
	.align 2
.L700:
	.word 1000000000

/**
 * clock_getres
 * \param clockid which clock
 * \param req struct timespec* resolution
 * \return 0 on success or a positive error code
 */
.section .text.clock_getres
.global clock_getres
.type clock_getres, %function
clock_getres:
	movs r3, #35
	svc  0
	str  r2, [r1, #8]
	movs r2, #0
	movs r3, #0
	strd r2, [r1]
	bx   lr

/* TODO: missing syscalls: clock_adjtime */

/**
 * _exit, terminate process
 * \param v exit value
 * This syscall does not return
 */
.section .text._exit
.global _exit
.type _exit, %function
_exit:
	movs r3, #37
	svc  0

/**
 * execve, run a different program
 * \param path program to run
 * \param argv program arguments
 * \param envp program environment variables
 * \return -1 on failure. Does not return on success
 */
.section .text.execve
.global execve
.type execve, %function
execve:
	movs r3, #38
	svc  0
	/* if execve returns, then it failed */
	b    __seterrno32

/**
 * posix_spawn, start a new process running the given program
 * \param pid pid of running program will be stored here
 * \param path program to run
 * \param file_actions optional actions (not supported, must be nullptr)
 * \param attrp more options            (not supported, must be nullptr)
 * \param argv program arguments
 * \param envp program environment variables
 * \return an error code on failure, 0 on success
 */
.section .text.posix_spawn
.global posix_spawn
.type posix_spawn, %function
posix_spawn:
	cbnz r2, .L800
	cbnz r3, .L800
	ldrd r2, r3, [sp]
	mov  r12, r3
	movs r3, #39
	svc  0
	bx   lr
.L800:
	movs r0, #14 /* EFAULT */
	bx   lr

/* TODO: missing syscalls */

/**
 * waitpid, wait for process termination
 * \param pid pid to wait for
 * \param wstatus pointer to return code
 * \param options wait options
 * \return 0 on success, -1 on failure
 */
.section .text.waitpid
.global waitpid
.type waitpid, %function
waitpid:
	movs r3, #41
	svc  0
	cmp  r0, #0
	blt  syscallfailed32
	bx   lr

/**
 * getpid
 * \return the pid of the current process
 */
.section .text.getpid
.global getpid
.type getpid, %function
getpid:
	movs r3, #42
	svc  0
	bx   lr

/**
 * getppid
 * \return the pid of the parent process
 */
.section .text.getppid
.global getppid
.type getppid, %function
getppid:
	movs r3, #43
	svc  0
	bx   lr

/* TODO: missing syscalls: getuid, getgid, geteuid, getegid, setuid, setgid */

/* common jump target for all failing syscalls with 32 bit return value */
.section .text.__seterrno32
syscallfailed32:
	/* tail call */
	b    __seterrno32

/* common jump target for all failing syscalls with 64 bit return value */
.section .text.__seterrno64
syscallfailed64:
	/* tail call */
	b    __seterrno64

.end
