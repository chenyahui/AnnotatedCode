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

#ifndef FLINTER_MSLEEP_H
#define FLINTER_MSLEEP_H

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pause the calling thread (no sooner than) given milliseconds.
 * Will not be interrupted by signals.
 *
 * @param milliseconds safe to use in signal handler if milliseconds > 0
 */
extern void msleep(int milliseconds);

/**
 * Pause the calling thread (no sooner than) given milliseconds.
 * Interruptible by signals if alertable, but return remaining milliseconds.
 *
 * @param milliseconds safe to use in signal handler if milliseconds > 0
 * @param alertable return on receiving signals
 */
extern int msleep_ex(int milliseconds, int alertable);

/**
 * Pause the calling thread (no sooner than) given milliseconds.
 * Interruptible by signals, but return remaining milliseconds.
 *
 * @param milliseconds safe to use in signal handler if milliseconds > 0
 * @param alertable return on receiving signals
 * @param sigset signal mask when sleeping
 */
extern int msleep_signal(int milliseconds,
                         int alertable,
                         const sigset_t *sigset);

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_MSLEEP_H */
