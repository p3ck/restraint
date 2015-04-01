#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>

/* work out what fcntl flag to use for non-blocking */
#ifdef O_NONBLOCK
# define NONBLOCK_FLAG O_NONBLOCK
#elif defined SYSV
# define NONBLOCK_FLAG O_NDELAY
#else
# define NONBLOCK_FLAG FNDELAY
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

pid_t piped_child(char **command, int *f_in, int *f_out);
pid_t local_child(int argc, char **argv, int *f_in, int *f_out, int (*child_main)(int, char*[]));

int fd_pair(int fd[2]);
pid_t do_fork(void);
void set_nonblocking(int fd);
void set_blocking(int fd);
void logfile_close(void);
