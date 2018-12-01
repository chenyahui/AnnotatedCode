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

#ifndef FLINTER_BABYSITTER_H
#define FLINTER_BABYSITTER_H

#if defined(__unix__) || defined(__MACH__)

#ifdef __cplusplus
extern "C" {
#endif

/* Everything below should be non-negative, or unexpected behavior happens. */
typedef struct _babysitter_configure_t {
    int unconditional;       /* If non-zero, restart even if exited with 0. */
    int restart_times;       /* If non-zero, only restart up to this times. */
    int normal_delay;        /* Milliseconds before restarting.             */
    int coredump_delay;      /* Milliseconds before restarting if dumped.   */
    int watchdog_timer;      /* If non-zero, feed me within milliseconds.   */
    int watchdog_signal;     /* SIGKILL if zero, or signal number to kill.  */

    /* Called in parent process, return 0 to spawn again. */
    int (*spawn)(int /*status*/, void *);
    void *sparam;            /* Used in respawn().                          */

    void (*cleanup)(void *); /* Called before babysitter process exits.     */
    void *cparam;            /* Used in cleanup().                          */

} babysitter_configure_t;

extern int babysitter_spawn(const babysitter_configure_t *configure);
extern void babysitter_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* defined(__unix__) || defined(__MACH__) */

#endif /* FLINTER_BABYSITTER_H */
