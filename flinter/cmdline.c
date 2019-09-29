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

#include "flinter/cmdline.h"

#include <assert.h>
#include <errno.h>

#include "flinter/utility.h"

#if defined(WIN32)
# include <Windows.h>
#elif __unix__
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#define MAX_PREFIX_LENGTH 20

extern char **environ;

static size_t g_max_length              = 0;
static size_t g_prefix_length           = 0;
static char   g_prefix[MAX_PREFIX_LENGTH]  ;

static       char *g_arg_name        = NULL;
#endif /* __unix__ */

#define MAX_STATIC_BUFFER 65536

static char *g_module_arg         = NULL;
static char *g_module_path        = NULL;
static char *g_module_dirname     = NULL;
static char *g_module_basename    = NULL;
static char *g_saved_program_name = NULL;

static char  g_memory_buffer[MAX_STATIC_BUFFER];
static char *g_memory_pointer = g_memory_buffer;

static void *cmdline_malloc(size_t size)
{
    size_t remain;
    char *ret;

    remain = sizeof(g_memory_buffer) - (size_t)(g_memory_pointer - g_memory_buffer);
    if (size > remain) {
        return malloc(size);
    }

    ret = g_memory_pointer;
    g_memory_pointer += size;
    return ret;
}

static void cmdline_free(char *ptr)
{
    size_t len;
    char *str;

    str = (char *)ptr;
    if (str < g_memory_buffer ||
        str >= g_memory_buffer + sizeof(g_memory_buffer)) {

        free(ptr);
        return;
    }

    len = strlen(str) + 1;
    if (str + len == g_memory_pointer) {
        g_memory_pointer -= len;
    }
}

static char *cmdline_strdup(const char *str)
{
    char *ret;
    size_t len;
    if (!str) {
        return NULL;
    }

    len = strlen(str);
    ret = cmdline_malloc(len + 1);
    if (!ret) {
        return NULL;
    }

    memcpy(ret, str, len + 1);
    return ret;
}

#if defined(WIN32)
char **cmdline_setup(int argc, char *argv[])
{
    char filename[MAX_PATH * 4];
    wchar_t buffer[MAX_PATH];
    char *pos;
    DWORD ret;
    DWORD i;
    int len;

    if (argc <= 0 || !argv || !*argv) {
        return NULL;
    }

    ret = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (ret == 0 || ret == MAX_PATH) {
        return NULL;
    }

    len = WideCharToMultiByte(CP_UTF8, 0, buffer, ret, filename, sizeof(filename), NULL, NULL);
    if (len <= 0 || len >= sizeof(filename)) {
        return NULL;
    }
    filename[len] = '\0';

    for (i = 0; i < ret; ++i) {
        if (filename[i] == '\\') {
            filename[i] = '/';
        }
    }

    g_module_arg = cmdline_strdup(argv[0]);
    g_module_path = cmdline_strdup(filename);

    pos = strrchr(filename, '/');
    if (!pos) {
        g_module_basename = cmdline_strdup(filename);
        g_module_dirname = cmdline_strdup(".");
    } else {
        g_module_basename = cmdline_strdup(pos + 1);
        *pos = '\0';
        g_module_dirname = cmdline_strdup(filename);
    }

    return argv;
}

int cmdline_set_process_name(const char *fmt, ...)
{
    /* NOP */
    return 0;
}
#elif __unix__
/* Both dirname and basename is retrieved. */
/* Never fails. */
static int cmdline_get_module_path_via_argv(const char *argv, int overwrite)
{
    char buffer[PATH_MAX];
    int ret;

    if (overwrite) {
        /* Arbitrary order. */
        cmdline_free(g_module_basename);
        cmdline_free(g_module_dirname);
        cmdline_free(g_module_basename);
        g_module_basename = NULL;
        g_module_dirname = NULL;
    }

    if (!g_module_dirname) {
        ret = snprintf(buffer, sizeof(buffer), "%s", argv);
        if (ret >= 0 && (size_t)ret < sizeof(buffer)) {
            g_module_dirname = cmdline_strdup(dirname(buffer));
        }
    }

    if (!g_module_basename) {
        ret = snprintf(buffer, sizeof(buffer), "%s", argv);
        if (ret >= 0 && (size_t)ret < sizeof(buffer)) {
            g_module_basename = cmdline_strdup(basename(buffer));
        }
    }

    return 0;
}

/* Both dirname and basename are retrieved. */
static int cmdline_get_module_path_via_exe(void)
{
    char buffer[PATH_MAX];
    char path[128];
    ssize_t ret;

    if (g_module_dirname && g_module_basename) {
        return 0;
    }

    ret = snprintf(path, sizeof(path), "/proc/%d/exe", getpid());
    if (ret < 0 || (size_t)ret >= sizeof(path)) {
        return -1;
    }

    ret = readlink(path, buffer, sizeof(buffer));
    if (ret <= 0) {
        return -1;
    }

    buffer[ret] = '\0';
    return cmdline_get_module_path_via_argv(buffer, 1);
}

/* Only basename are retrieved. */
static int cmdline_get_module_path_via_stat(const char *argv)
{
    char buffer[NAME_MAX];
    char *argv_name;
    char *second;
    char *third;
    FILE *file;

    size_t start;
    size_t end;
    size_t len;
    size_t i;

    if (g_module_basename) {
        return 0;
    }

    /* In case of ARGV[0] been modified, read module name directly from PROCFS. */
    file = fopen("/proc/self/stat", "r");
    if (!file) {
        return -1;
    }

    len = fread(buffer, (size_t)1, sizeof(buffer) - 1, file);
    fclose(file);

    /* Find 2nd token. */
    second = NULL;
    for (i = 0; i < len; ++i) {
        if (buffer[i] == '(') {
            second = buffer + i + 1;
            break;
        }
    }

    if (!second) {
        return -1;
    }

    start = (size_t)(second - buffer);
    end = len - start;
    end = end <= 20 ? end : 20;
    end += start;

    /* Find 3rd token. */
    third = NULL;
    for (i = end; i > start; --i) {
        if (buffer[i] == ')') {
            third = buffer + i;
            break;
        }
    }

    if (!third) {
        return -1;
    }

    *third = '\0';
    argv_name = basename(cmdline_strdup(argv));

    /* Looks like a truncation, proceed with argv. */
    if (strncmp(argv_name, second, 15) == 0) {
        g_module_basename = cmdline_strdup(argv_name);
    } else {
        g_module_basename = cmdline_strdup(second);
    }

    return 0;
}

/* Only dirname is retrieved. */
static int cmdline_get_module_path_via_fd(const char *argv)
{
    char buffer[PATH_MAX];
    char path[128];
    ssize_t ret;
    char *ptr;
    int fd;

    if (g_module_dirname) {
        return 0;
    }

    ptr = cmdline_strdup(argv);
    fd = open(dirname(ptr), O_RDONLY | O_NOCTTY);
    cmdline_free(ptr);
    if (fd < 0) {
        return -1;
    }

    ret = snprintf(path, sizeof(path), "/proc/%d/fd/%d", getpid(), fd);
    if (ret < 0 || (size_t)ret >= sizeof(path)) {
        return -1;
    }

    ret = readlink(path, buffer, sizeof(buffer));

    close(fd);
    if (ret <= 0) {
        return -1;
    }

    buffer[ret] = '\0';
    g_module_dirname = cmdline_strdup(buffer);
    return 0;
}

/* Only dirname is retrieved. */
static int cmdline_get_module_path_via_cwd(const char *argv)
{
    char buffer[PATH_MAX];
    char cwd[PATH_MAX];
    char *ptr;
    char *pwd;
    int ret;

    if (g_module_dirname) {
        return 0;
    }

    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }

    pwd = NULL;
    if (*argv != '/') {
        ptr = cmdline_strdup(argv);
        ret = snprintf(buffer, sizeof(buffer), "%s/%s", cwd, dirname(ptr));
        cmdline_free(ptr);

        if (ret < 0 || (size_t)ret >= sizeof(buffer)) {
            return -1;
        }

        pwd = make_clean_path(buffer, 0);
    }

    if (!pwd) {
        return -1;
    }

    g_module_dirname = cmdline_strdup(pwd);
    free(pwd);
    return 0;
}

static int cmdline_get_module_path(const char *argv)
{
    char buffer[PATH_MAX];
    int ret;

    cmdline_get_module_path_via_stat(argv);
    cmdline_get_module_path_via_fd(argv);
    cmdline_get_module_path_via_cwd(argv);

    /* Fallback, overwrite previous result. */
    cmdline_get_module_path_via_exe();

    /* Fallback, won't overwrite previous result, not absolute path! */
    cmdline_get_module_path_via_argv(argv, 0);

    if (!g_module_dirname || !g_module_basename) {
        return -1;
    }

    ret = snprintf(buffer, sizeof(buffer), "%s/%s", g_module_dirname, g_module_basename);
    if (ret < 0 || (size_t)ret >= sizeof(buffer)) {
        return -1;
    }

    g_module_path = cmdline_strdup(buffer);
    return 0;
}

static void cmdline_reserve_prefix(void)
{
    size_t len;

    assert(g_module_basename);

    len = strlen(g_module_basename);
    if (len > sizeof(g_prefix) - 2) {
        len = sizeof(g_prefix) - 2;
    }

    g_prefix_length = len + 2;
    memcpy(g_prefix, g_module_basename, len);
    g_prefix[len++] = ':';
    g_prefix[len++] = ' ';
}

char **cmdline_setup(int argc, char *argv[])
{
    char **environ_duplication;
    char **argv_duplication;
    size_t environ_size;
    char *pointer;
    int argv_size;
    size_t i;
    int j;

    if (argc <= 0 || !argv || !*argv || g_arg_name) {
        errno = EINVAL;
        return NULL;
    }

    g_arg_name = argv[0];
    g_max_length = strlen(g_arg_name);
    g_module_arg = cmdline_strdup(argv[0]);

    if (cmdline_get_module_path(g_arg_name)) {
        return NULL;
    }

    /* Now g_module_basename should be available. */
    cmdline_reserve_prefix();

    /* Try to count real ARGV length. */
    argv_size = 1;
    while (argv_size < argc && argv[argv_size]) {
        if (argv[argv_size] != argv[argv_size - 1] + strlen(argv[argv_size - 1]) + 1) {
            break;
        }
        ++argv_size;
    }

    /* Try to count available environment variables. */
    environ_size = 0;
    if (argv_size == argc) {
        if (environ && *environ) {
            pointer = argv[argc - 1];
            while (environ[environ_size]) {
                if (environ[environ_size] != pointer + strlen(pointer) + 1) {
                    break;
                }
                pointer = environ[environ_size];
                ++environ_size;
            }
        }
    }

    argv_duplication = cmdline_malloc(sizeof(char *) * (size_t)(argv_size + 1));
    if (!argv_duplication) {
        errno = ENOMEM;
        return NULL;
    }

    for (j = 0; j < argv_size; ++j) {
        argv_duplication[j] = cmdline_strdup(argv[j]);
    }
    argv_duplication[argc] = NULL;

    /* Not available, just occupy ARGV. */
    if (!environ_size) {
        g_max_length = (size_t)(argv[argv_size - 1] - argv[0])
                     + strlen(argv[argv_size - 1]);

        return argv_duplication;
    }

    environ_duplication = cmdline_malloc(sizeof(char *) * (environ_size + 1));
    if (!environ_duplication) {
        return argv_duplication;
    }

    g_max_length = (size_t)(environ[environ_size - 1] - argv[0])
                 + strlen(environ[environ_size - 1]);

    for (i = 0; i < environ_size; ++i) {
        environ_duplication[i] = cmdline_strdup(environ[i]);
    }
    environ_duplication[environ_size] = NULL;

    environ = environ_duplication;
    return argv_duplication;
}

#if !HAVE_SETPROCTITLE
int cmdline_set_process_name(const char *fmt, ...)
{
    int with_prefix;
    int eraser;
    va_list ap;
    size_t max;
    size_t off;

    if (!g_max_length || !g_arg_name) {
        errno = EINVAL;
        return -1;
    }

    if (!fmt) {
        fmt = g_module_arg;
        with_prefix = 0;

    } else if (*fmt == '-') {
        with_prefix = 0;
        ++fmt;

    } else {
        with_prefix = g_prefix_length ? 1 : 0;
    }

    if (with_prefix) {
        if (g_prefix_length <= g_max_length) {
            off = g_prefix_length;
        } else {
            off = g_max_length;
        }

        memcpy(g_arg_name, g_prefix, off);
        max = g_max_length - off;

    } else {
        max = g_max_length;
        off = 0;
    }

    cmdline_free(g_saved_program_name);
    g_saved_program_name = cmdline_strdup(g_arg_name);

    /* g_max_length doesn't include the trailing zero, so add 1 to it. */
    va_start(ap, fmt);
    eraser = vsnprintf(g_arg_name + off, max + 1, fmt, ap);
    va_end(ap);
    if (eraser < 0) {
        return -1;
    }

    /* Erase all remaining buffer, since some OS remembers the old length. */
    if (max > (size_t)eraser) {
        off += (size_t)eraser;
    } else {
        off += max;
    }

    memset(g_arg_name + off, 0, g_max_length - off);
    return 0;
}

void cmdline_unset_process_name(void)
{
    size_t len;
    if (!g_saved_program_name) {
        return;
    }

    len = strlen(g_saved_program_name);
    memcpy(g_arg_name, g_saved_program_name, len);
    memset(g_arg_name + len, 0, g_max_length - len);
    cmdline_free(g_saved_program_name);
    g_saved_program_name = NULL;
}
#endif /* HAVE_SETPROCTITLE */

#endif /* __unix__ */

char *cmdline_get_absolute_path(const char *path, int force_relative)
{
    char *p;
    char *q;
    char *r;

#ifdef WIN32
    /* TODO(yiyuanzhong): Windows is a headache, disable it for now. */
    force_relative = 1;
#endif

    if (!path) {
        errno = EINVAL;
        return NULL;
  /*} else if (!g_module_dirname || g_module_dirname[0] != '/') {*/
    } else if (!g_module_dirname || !*g_module_dirname) {
        errno = EPERM;
        return NULL;
    }

    r = make_clean_path(path, 0);
    if (!r) {
        return NULL;
    }

    if (r[0] == '/' && !force_relative) {
        q = r;

    } else {
        q = (char *)malloc(strlen(g_module_dirname) + strlen(r) + 2);
        if (!q) {
            free(r);
            errno = ENOMEM;
            return NULL;
        }

        strcpy(q, g_module_dirname);
        strcat(q, "/");
        strcat(q, r);
        free(r);
    }

    p = make_clean_path(q, 0);
    free(q);
    if (!p) {
        return NULL;
    }

    return p;
}

const char *cmdline_arg_name()
{
    return g_module_arg;
}

const char *cmdline_module_dirname()
{
    return g_module_dirname;
}

const char *cmdline_module_basename()
{
    return g_module_basename;
}

const char *cmdline_module_path()
{
    return g_module_path;
}
