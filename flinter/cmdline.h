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

#ifndef FLINTER_CMDLINE_H
#define FLINTER_CMDLINE_H

#include <sys/types.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__MACH__)
# include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initialize internal memory layouts to accept process name changing.
 * Make sure you call this method as early as you get ARGV from main() since other
 * modules might change the memory layout thus reduce the capability that long names
 * can be used. Might cause tiny but unavoidable memory leaks to accept long names.
 *
 * @return  The newly allocated argv array, use it instead of the original one.
 * @retval  NULL if the operation was a failure.
 *
 * @warning If your main() accepts char **env, point it to environ after the call.
 * @warning Memory leaks might occur, typically several KBs once and for all.
 */
extern char **cmdline_setup(int argc, char *argv[]);

#if HAVE_SETPROCTITLE
# define cmdline_set_process_name(...) (setproctitle(__VA_ARGS__),0)
#else
/**
 * @retval 0 if succeeded, -1 on error.
 * @warning Calling this method might overwrite the original ARGV.
 * @param fmt can be NULL so that the original process name is restored, however arguments
 *            are not restored, only argv[0].
 * @sa cmdline_setup()
 */
extern int cmdline_set_process_name(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#endif

#if HAVE_SETPROCTITLE
# define cmdline_unset_process_name()
#else
/**
 * Restore last set process name after using syslog(3).
 * Can only unset once.
 */
extern void cmdline_unset_process_name(void);
#endif

/**
 * Get the absolute path of the given path.
 * Path is treated as an absolute path if it begins with '/' or force_relative is non-zero.
 * Relative path is calculated according to the executable, originally started. If the
 * executable is moved or deleted - that's beyond this method's power.
 *
 * If the executable is /some/where/bin/exe:
 * ../abc, 0 or 1   -> /some/where/abc
 * /abc, 0          -> /abc
 * /abc, 1          -> /some/where/bin/abc
 *
 * @return NULL if failed, errno is set accordingly.
 * @warning Caller free the returned buffer via free(3).
 *
 * TODO(yiyuanzhong): not (yet) working on Windows due to path rules.
 */
extern char *cmdline_get_absolute_path(const char *path, int force_relative);

extern const char *cmdline_arg_name();
extern const char *cmdline_module_path();
extern const char *cmdline_module_dirname();
extern const char *cmdline_module_basename();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FLINTER_CMDLINE_H */
