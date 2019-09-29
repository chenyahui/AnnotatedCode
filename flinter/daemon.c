/* Copyright 2014 yiyuanzhong@gmail.com (Yiyuan Zhong)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __unix__

#include "flinter/daemon.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "flinter/babysitter.h"
#include "flinter/cmdline.h"
#include "flinter/mkdirs.h"
#include "flinter/msleep.h"
#include "flinter/pidfile.h"
#include "flinter/revision.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

#define TEST(f,s) (((f) & (s)) == (f))

static int daemon_close_files(int fd_to_start_closing)
{
    /* We need /proc to work here. */
    struct rlimit rlimit;
    struct dirent *ent;
    int dir_fd;
    DIR *dir;
    int max;
    int fd;

    size_t size;
    size_t pos;
    int *nfds;
    int *fds;
    size_t i;

    if (fd_to_start_closing < 0) {
        fd_to_start_closing = 0;
    }

    dir = opendir("/proc/self/fd");
    dir_fd = -1;
    if (dir) {
        dir_fd = dirfd(dir);
        if (dir_fd < 0) {
            closedir(dir);
            dir = NULL;
        }
    }

    if (!dir) {
        /*
         * Unable to read /proc, then we take a costly backup route.
         * And this method is not secure if there's a FD larger than 4096.
         */
        max = 4096;
        if (getrlimit(RLIMIT_NOFILE, &rlimit) == 0  &&
            rlimit.rlim_cur != RLIM_INFINITY        &&
            rlimit.rlim_cur > 0                     &&
            rlimit.rlim_cur <= 4096                 ){

            max = (int)rlimit.rlim_cur;
        }

        for (fd = fd_to_start_closing; fd < max; ++fd) {
            safe_close(fd);
        }

    } else {
        pos = 0;
        size = 128;
        fds = NULL;

        do {
            nfds = realloc(fds, size * sizeof(int));
            if (!nfds) {
                free(fds);
                closedir(dir);
                errno = ENOMEM;
                return -1;
            }
            fds = nfds;

            do {
                ent = readdir(dir);
                if (!ent) {
                    break;
                }

                fd = atoi32(ent->d_name);
                if (fd != dir_fd && fd >= fd_to_start_closing) {
                    fds[pos++] = fd;
                }

            } while (pos < size);

            size <<= 1;
        } while (pos == size);

        closedir(dir);
        for (i = 0; i < pos; ++i) {
            safe_close(fds[i]);
        }
        free(fds);
    }

    return 0;
}

static int daemon_nullify_streams(void)
{
    struct stat st;
    int null;

    if (stat("/dev/null", &st) || !S_ISCHR(st.st_mode)) {
        /* Hell, what kind of system is this? */
        return -1;
    }

    null = open("/dev/null", O_RDONLY | O_NOCTTY);
    if (null < 0) {
        return -1;
    }

    if (null != STDIN_FILENO) {
        if (dup2(null, STDIN_FILENO) < 0) {
            return -1;
        }
        safe_close(null);
    }

    null = open("/dev/null", O_WRONLY | O_NOCTTY);
    if (null < 0) {
        return -1;
    }

    if (null != STDOUT_FILENO) {
        if (dup2(null, STDOUT_FILENO) < 0) {
            return -1;
        }
    }

    if (null != STDERR_FILENO) {
        if (dup2(null, STDERR_FILENO) < 0) {
            return -1;
        }
    }

    if (null != STDOUT_FILENO && null != STDERR_FILENO) {
        safe_close(null);
    }

    return 0;
}

static int daemon_close_files_and_nullify_streams(int fd_to_start_closing)
{
    if (daemon_close_files(fd_to_start_closing)) {
        return -1;
    }

    if (daemon_nullify_streams()) {
        return -1;
    }

    return 0;
}

static int daemon_daemonize(int fd_to_start_closing, int detach_session, int cd_to_root)
{
    pid_t pid;

    if (detach_session) {
        /* Create new session and process group. */
        if (setsid() < 0) {
            _exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid < 0) {
            _exit(EXIT_FAILURE);
        }

        if (pid) { /* Goodbye, temporary process, the child process is now alive. */
            _exit(EXIT_SUCCESS);
        }
    }

    if (daemon_close_files_and_nullify_streams(fd_to_start_closing)) {
        _exit(EXIT_FAILURE);
    }

    /* Daemons are privileged, deny group and other accesses. */
    umask(S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

    if (cd_to_root) {
        /* Leave current directory to allow future dismounts. */
        if (chdir("/")) {
            _exit(EXIT_FAILURE);
        }
    }

    /* Now the child process returns. */
    return 0;
}

int daemon_spawn(int fd_to_start_closing, int flags)
{
    void (*handler)(int);
    int fast_return;
    int status;
    pid_t pid;

    if ((flags | DAEMON_FLAGS_MASK) != DAEMON_FLAGS_MASK ||
        fd_to_start_closing < 0                          ){

        errno = EINVAL;
        return -1;
    }

    fast_return = TEST(DAEMON_FAST_RETURN, flags) ||
                  TEST(DAEMON_ATTACHED_SESSION, flags);

    handler = NULL;
    if (!fast_return) {
        handler = signal(SIGCHLD, SIG_DFL);
    }

    pid = fork();
    if (pid < 0) {
        if (!fast_return) {
            signal(SIGCHLD, handler);
        }
        return -1;
    }

    if (pid) { /* We're in the calling process context. */
        if (fast_return) {
            return 1;
        }

        if (waitpid(pid, &status, 0) < 0) {
            signal(SIGCHLD, handler);
            return -1;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            signal(SIGCHLD, handler);
            return -1;
        }

        signal(SIGCHLD, handler);
        return 1;
    }

    /* We're in the temporary process context. */
    signal(SIGCHLD, SIG_DFL);
    if (daemon_daemonize(fd_to_start_closing,
                         !TEST(DAEMON_ATTACHED_SESSION, flags),
                         !TEST(DAEMON_NO_CD_TO_ROOT, flags))) {

        _exit(EXIT_FAILURE);
    }

    /* Kick off the daemon process. */
    return 0;
}

static void daemon_print_version(const daemon_configure_t *configure, const char *argv0)
{
    char buffer[256];

    if (revision_get_project_version(buffer, sizeof(buffer))) {
        printf("%s (%s) %s\n", argv0, configure->name, configure->version);
        printf("Written by %s (%s).\n", configure->mail, configure->author);

    } else {
        printf("%s (%s) %s\n", argv0, configure->name, buffer);
        printf("Written by %s (%s).\n", configure->mail, configure->author);
        printf("\n");
        revision_print();
    }
}

static void daemon_print_license(const daemon_configure_t *configure)
{
    printf("Copyright 2014 %s (%s)\n", configure->mail, configure->author);
    printf("\n");
    printf("Licensed under the Apache License, Version 2.0 (the \"License\");\n");
    printf("you may not use this file except in compliance with the License.\n");
    printf("You may obtain a copy of the License at\n");
    printf("\n");
    printf("    http://www.apache.org/licenses/LICENSE-2.0\n");
    printf("\n");
    printf("Unless required by applicable law or agreed to in writing, software\n");
    printf("distributed under the License is distributed on an \"AS IS\" BASIS,\n");
    printf("WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n");
    printf("See the License for the specific language governing permissions and\n");
    printf("limitations under the License.\n");
    printf("\n");

    printf("This product includes software developed by the OpenSSL Project\n");
    printf("for use in the OpenSSL Toolkit. (http://www.openssl.org/)\n");
    printf("\n");

    printf("This product includes cryptographic software written by\n");
    printf("Eric Young (eay@cryptsoft.com)\n");
}

static void daemon_print_help(const daemon_configure_t *configure, const char *argv0)
{
    printf("Usage: %s -k <control> [OPTION]...\n", argv0);
    printf("       %s -c [OPTION]...\n", argv0);
    printf("       %s <-h|-l|-v>\n", argv0);
    printf("\n");
    printf("%s\n", configure->full_name);
    printf("\n");
    printf("  -k, --control     <start|stop|reload|status>\n");
    printf("  -c, --console     do not daemonize\n");
    printf("\n");
    printf("  -v, --version     output version information and exit\n");
    printf("  -l, --license     output license information and exit\n");
    printf("  -h, --help        display this help and exit\n");
    printf("\n");
    printf("Without any OPTION, show this help.\n");
}

static void daemon_print_unknown(const char *argv0)
{
    fprintf(stderr, "Try `%s --help' for more information.\n", argv0);
}

static int daemon_prepare_directory(const char *relative)
{
    char *dir;
    int ret;

    dir = cmdline_get_absolute_path(relative, 0);
    if (!dir || !*dir) {
        fprintf(stderr, "Failed to prepare directory %s: %d: %s",
                relative, ENOMEM, strerror(ENOMEM));

        free(dir);
        return -1;
    }

    ret = mkdirs(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    free(dir);

    if (ret) {
        fprintf(stderr, "Failed to prepare directory %s: %d: %s",
                relative, ENOMEM, strerror(ENOMEM));

        return -1;
    }

    return 0;
}

static int daemon_prepare_directories(void)
{
    if (daemon_prepare_directory("../log") ||
        daemon_prepare_directory("../run") ||
        daemon_prepare_directory("../tmp") ){

        return -1;
    }

    return 0;
}

static char *daemon_get_pidfile(const char *name)
{
    char buffer[PATH_MAX];
    int ret;

    ret = snprintf(buffer, sizeof(buffer), "../run/%s.pid", name);
    if (ret < 0 || (size_t)ret >= sizeof(buffer)) {
        return NULL;
    }

    return cmdline_get_absolute_path(buffer, 0);
}

static pid_t daemon_do_control_status(const char *pidfile)
{
    pid_t pid;

    pid = pidfile_check(pidfile);
    if (pid > 0) {
        printf("PID = %d active lock detected.\n", pid);

    } else if (pid == 0) {
        printf("The daemon is gone but PID file is left behind.\n");
        unlink(pidfile);

    } else if (errno == ENOENT) {
        printf("No PID file is found, daemon is not running.\n");
        pid = 0;

    } else {
        pid = -1;
        fprintf(stderr, "Can't reliably read PID file, try do it manually: %d: %s\n",
                errno, strerror(errno));
    }

    return pid;
}

static int daemon_control_status(const char *name)
{
    char *pidfile;
    pid_t pid;

    pidfile = daemon_get_pidfile(name);
    if (!pidfile) {
        return -1;
    }

    pid = daemon_do_control_status(pidfile);
    free(pidfile);
    if (pid <= 0) {
        return -1;
    }

    return 0;
}

static int daemon_control_daemonize(const daemon_configure_t *configure,
                                    const char *pidfile)
{
    pid_t pid;
    int count;
    int ret;
    int i;

    ret = daemon_spawn(configure->fd_to_start_closing, configure->flags);
    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    count = 0;
    for (i = 0; ; ++i) {
        msleep(100);
        pid = pidfile_check(pidfile);
        if (pid >= 0) {
            ++count;
        } else {
            count = 0;
        }

        if (count >= 5) {
            return 1;
        } else if (i == 50) {
            return -1;
        }

    }

    return 1;
}

static int daemon_control_start(const daemon_configure_t *configure,
                                int argc, char *argv[],
                                int console, int *status)
{
    char *pidfile;
    int ret;
    int fd;
    int i;

    *status = EXIT_FAILURE;
    if (daemon_prepare_directories()) {
        return -1;
    }

    pidfile = daemon_get_pidfile(configure->name);
    if (!pidfile) {
        return -1;
    }

    if (!console) {
        ret = daemon_control_daemonize(configure, pidfile);
        if (ret < 0) {
            free(pidfile);
            return -1;
        } else if (ret > 0) {
            *status = EXIT_SUCCESS;
            free(pidfile);
            return 1;
        }

        if (configure->babysitter) {
            if (babysitter_spawn(configure->babysitter)) {
                free(pidfile);
                return 2;
            }
        }
    }

    for (i = 0; ; ++i) {
        fd = pidfile_open(pidfile);
        if (fd >= 0 || fd == -2 || i == 2) {
            break;
        }

        msleep(100);
    }

    if (fd >= 0) {
        ret = configure->callback(argc, argv);
        pidfile_close(fd, pidfile);
        *status = ret;

    } else if (fd == -2) { /* Another instance detected. */
        if (console) {
            fprintf(stderr, "[FAIL] duplicated instance detected.\n");
        } else {
            *status = EXIT_SUCCESS;
        }
    }

    free(pidfile);
    return 2;
}

static int daemon_control_stop(const char *name, int signum, int wait)
{
    char *pidfile;
    pid_t pid;
    pid_t ret;
    int i;

    if (daemon_prepare_directories()) {
        return -1;
    }

    pidfile = daemon_get_pidfile(name);
    if (!pidfile) {
        return -1;
    }

    pid = daemon_do_control_status(pidfile);
    if (pid < 0) {
        free(pidfile);
        return -1;
    } else if (pid == 0) {
        free(pidfile);
        return 0;
    }

    if (kill(pid, signum)) {
        fprintf(stderr, "PID = %d failed to kill daemon: %d: %s\n",
                pid, errno, strerror(errno));

        free(pidfile);
        return -1;
    }

    printf("PID = %d signal %d sent.\n", pid, signum);
    if (!wait) {
        return 0;
    }

    for (i = 0; i < 3000; ++i) { /* 5min */
        msleep(100);
        ret = pidfile_check(pidfile);
        if (ret <= 0) {
            free(pidfile);
            printf("PID = %d stopped.\n", pid);
            return 0;
        }
    }

    free(pidfile);
    fprintf(stderr, "PID = %d failed to stop, lock is still held.\n", pid);
    return -1;
}

static int daemon_parse_arguments(const daemon_configure_t *c,
                                  int argc, char *argv[],
                                  const char **control,
                                  int *console)
{
    static const char OPTIONS[] = "vlhk:c";
    static const struct option LONG_OPTIONS[] = {
        {"console",       no_argument,  NULL,   'c'},
        {"version",       no_argument,  NULL,   'v'},
        {"license",       no_argument,  NULL,   'l'},
        {"help",          no_argument,  NULL,   'h'},
        {"control", required_argument,  NULL,   'k'},
        { NULL,                     0,  NULL,    0 },
    };

    int opt;

    *control = NULL;
    *console = 0;
    for (;;) {
        opt = getopt_long(argc, argv, OPTIONS, LONG_OPTIONS, NULL);
        if (opt < 0) {
            break;
        }

        switch (opt) {
        case 'v':
            daemon_print_version(c, argv[0]);
            return 1;

        case 'l':
            daemon_print_license(c);
            return 1;

        case 'h':
            daemon_print_help(c, argv[0]);
            return 1;

        case 'k':
            *control = optarg;
            break;

        case 'c':
            *console = 1;
            break;

        case '?':
        default:
            daemon_print_unknown(argv[0]);
            return -1;
        };
    }

    if ((*console && *control) || (!*console && !*control)) {
        daemon_print_help(c, argv[0]);
        return -1;
    }

    return 0;
}

int daemon_main(const daemon_configure_t *configure, int argc, char *argv[])
{
    daemon_configure_t c;
    const char *control;
    char **new_argv;
    int new_argc;
    int console;
    int status;
    int ret;

    if (!configure || !configure->callback) {
        fprintf(stderr, "Invalid arguments.\n");
        return EXIT_FAILURE;
    }

    tzset();
    randomize();
    argv = cmdline_setup(argc, argv);
    if (!argv) {
        fprintf(stderr, "Failed to initialize cmdline.\n");
        return EXIT_FAILURE;
    }

    memcpy(&c, configure, sizeof(c));
    if (!c.name)      { c.name      = cmdline_module_basename(); }
    if (!c.full_name) { c.full_name = c.name;                    }
    if (!c.author)    { c.author    = "Yiyuan Zhong";            }
    if (!c.version)   { c.version   = "1.0.0";                   }
    if (!c.mail)      { c.mail      = "yiyuanzhong@gmail.com";   }

    /* Arguments parsing. */
    ret = daemon_parse_arguments(&c, argc, argv, &control, &console);
    if (ret < 0) {
        return EXIT_FAILURE;
    } else if (ret > 0) {
        return EXIT_SUCCESS;
    }

    new_argc = argc - optind + 1;
    new_argv = argv + optind - 1;
    new_argv[0] = argv[0];
    optind = 1;

    status = EXIT_FAILURE;
    if (!control || strcmp(control, "start") == 0) {
        ret = daemon_control_start(&c, new_argc, new_argv, console, &status);

    } else if (strcmp(control, "stop") == 0) {
        ret = daemon_control_stop(c.name, SIGTERM, 1);

    } else if (strcmp(control, "reload") == 0) {
        ret = daemon_control_stop(c.name, SIGHUP, 0);

    } else if (strcmp(control, "status") == 0) {
        ret = daemon_control_status(c.name);

    } else {
        daemon_print_help(&c, argv[0]);
        return EXIT_FAILURE;
    }

    if (ret == 2) { /* Returned from start child. */
        return status;
    }

    if (ret == 0 || (ret == 1 && status == EXIT_SUCCESS)) {
        printf("[OK] %s\n", control);
        status = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "[FAIL] %s\n", control);
        status = EXIT_FAILURE;
    }

    if (ret == 1) {
        fflush(stdout);
        fflush(stderr);
        _exit(status);
    }

    return status;
}

#endif /* __unix__ */
