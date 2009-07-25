#define _GNU_SOURCE
#include "tup/access_event.h"
#include "tup/compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>

static void handle_file(const char *file, const char *file2, int at);
static int ignore_file(const char *file);
static int sd;

static int (*s_open)(const char *, int, ...);
static int (*s_open64)(const char *, int, ...);
static FILE *(*s_fopen)(const char *, const char *);
static FILE *(*s_fopen64)(const char *, const char *);
static FILE *(*s_freopen)(const char *, const char *, FILE *);
static int (*s_creat)(const char *, mode_t);
static int (*s_symlink)(const char *, const char *);
static int (*s_rename)(const char*, const char*);
static int (*s_unlink)(const char*);
static int (*s_unlinkat)(int, const char*, int);
static int (*s_execve)(const char *filename, char *const argv[],
		       char *const envp[]);
static int (*s_xstat)(int vers, const char *name, struct stat *buf);
static int (*s_xstat64)(int vers, const char *name, struct stat64 *buf);

#define WRAP(ptr, name) \
	if(!ptr) { \
		ptr = dlsym(RTLD_NEXT, name); \
		if(!ptr) { \
			fprintf(stderr, "tup.ldpreload: Unable to wrap '%s'\n", \
				name); \
			exit(1); \
		} \
	}

int open(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;

	WRAP(s_open, "open");
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	/* O_ACCMODE is 0x3, which covers O_WRONLY and O_RDWR */
	rc = s_open(pathname, flags, mode);
	if(rc >= 0) {
		handle_file(pathname, "", flags&O_ACCMODE);
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(pathname, "", ACCESS_GHOST);
	}
	return rc;
}

int open64(const char *pathname, int flags, ...)
{
	int rc;
	mode_t mode = 0;

	WRAP(s_open64, "open64");
	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	rc = s_open64(pathname, flags, mode);
	if(rc >= 0) {
		handle_file(pathname, "", flags&O_ACCMODE);
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(pathname, "", ACCESS_GHOST);
	}
	return rc;
}

FILE *fopen(const char *path, const char *mode)
{
	FILE *f;

	WRAP(s_fopen, "fopen");
	f = s_fopen(path, mode);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

FILE *fopen64(const char *path, const char *mode)
{
	FILE *f;

	WRAP(s_fopen64, "fopen64");
	f = s_fopen64(path, mode);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
	FILE *f;

	WRAP(s_freopen, "freopen");
	f = s_freopen(path, mode, stream);
	if(f) {
		handle_file(path, "", !(mode[0] == 'r'));
	} else {
		if(errno == ENOENT || errno == ENOTDIR)
			handle_file(path, "", ACCESS_GHOST);
	}
	return f;
}

int creat(const char *pathname, mode_t mode)
{
	int rc;

	WRAP(s_creat, "creat");
	rc = s_creat(pathname, mode);
	if(rc >= 0)
		handle_file(pathname, "", ACCESS_WRITE);
	return rc;
}

int symlink(const char *oldpath, const char *newpath)
{
	int rc;
	WRAP(s_symlink, "symlink");
	rc = s_symlink(oldpath, newpath);
	if(rc == 0)
		handle_file(oldpath, newpath, ACCESS_SYMLINK);
	return rc;
}

int rename(const char *old, const char *new)
{
	int rc;

	WRAP(s_rename, "rename");
	rc = s_rename(old, new);
	if(rc == 0) {
		if(!ignore_file(old) && !ignore_file(new)) {
			handle_file(old, new, ACCESS_RENAME);
		}
	}
	return rc;
}

int unlink(const char *pathname)
{
	int rc;

	WRAP(s_unlink, "unlink");
	rc = s_unlink(pathname);
	if(rc == 0)
		handle_file(pathname, "", ACCESS_UNLINK);
	return rc;
}

int unlinkat(int dirfd, const char *pathname, int flags)
{
	int rc;

	WRAP(s_unlinkat, "unlinkat");
	rc = s_unlinkat(dirfd, pathname, flags);
	if(rc == 0) {
		if(dirfd == AT_FDCWD) {
			handle_file(pathname, "", ACCESS_UNLINK);
		} else {
			fprintf(stderr, "tup.ldpreload: Error - unlinkat() not supported unless dirfd == AT_FDCWD\n");
			return -1;
		}
	}
	return rc;
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
	int rc;

	WRAP(s_execve, "execve");
	handle_file(filename, "", ACCESS_READ);
	rc = s_execve(filename, argv, envp);
	return rc;
}

int __xstat(int vers, const char *name, struct stat *buf)
{
	int rc;
	WRAP(s_xstat, "__xstat");
	rc = s_xstat(vers, name, buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(name, "", ACCESS_GHOST);
		}
	}
	return rc;
}

int __xstat64 (int __ver, __const char *__filename,
	       struct stat64 *__stat_buf)
{
	int rc;
	WRAP(s_xstat64, "__xstat64");
	rc = s_xstat64(__ver, __filename, __stat_buf);
	if(rc < 0) {
		if(errno == ENOENT || errno == ENOTDIR) {
			handle_file(__filename, "", ACCESS_GHOST);
		}
	}
	return rc;
}

static void handle_file(const char *file, const char *file2, int at)
{
	struct access_event *event;
	static char msgbuf[sizeof(*event) + PATH_MAX*2];
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int rc;

	if(ignore_file(file))
		return;

	pthread_mutex_lock(&mutex);
	if(!sd) {
		char *path;

		path = getenv(TUP_SERVER_NAME);
		if(!path) {
			fprintf(stderr, "tup.ldpreload: Unable to get '%s' "
				"path from the environment.\n", TUP_SERVER_NAME);
			exit(1);
		}
		sd = strtol(path, NULL, 0);
		if(sd <= 0) {
			fprintf(stderr, "tup.ldpreload: Unable to get valid socket descriptor.\n");
			exit(1);
		}
	}
	event = (struct access_event*)msgbuf;
	event->at = at;
	event->len = strlen(file) + 1;
	event->len2 = strlen(file2) + 1;
	if(event->len + event->len2 >= PATH_MAX*2) {
		fprintf(stderr, "tup.ldpreload error: Path too long (%i + %i bytes)\n", event->len, event->len2);
		goto out_unlock;
	}
	memcpy(msgbuf + sizeof(*event), file, event->len);
	memcpy(msgbuf + sizeof(*event) + event->len, file2, event->len2);
	rc = send(sd, msgbuf, sizeof(*event) + event->len + event->len2, 0);
	if(rc < 0) {
		perror("tup.ldpreload send");
	}
out_unlock:
	pthread_mutex_unlock(&mutex);
}

static int ignore_file(const char *file)
{
	if(strncmp(file, "/tmp/", 5) == 0)
		return 1;
	if(strncmp(file, "/dev/", 5) == 0)
		return 1;
	return 0;
}
