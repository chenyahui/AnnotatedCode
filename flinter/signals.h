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

#ifndef FLINTER_SIGNALS_H
#define FLINTER_SIGNALS_H

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int signals_block  (int signum);
extern int signals_ignore (int signum);
extern int signals_default(int signum);
extern int signals_unblock(int signum);

extern int signals_block_all  (void);
extern int signals_ignore_all (void);
extern int signals_default_all(void);
extern int signals_unblock_all(void);

/*
 * 0 terminated.
 * Like this: signals_block_some(SIG_HUP, SIG_TERM, 0);
 */
extern int signals_block_some  (int signum, ...);
extern int signals_ignore_some (int signum, ...);
extern int signals_default_some(int signum, ...);
extern int signals_unblock_some(int signum, ...);

extern int signals_set_handler(int signum, void (*handler)(int));
extern int signals_set_action(int signum, void (*action)(int, siginfo_t *, void *));

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_SIGNALS_H */
