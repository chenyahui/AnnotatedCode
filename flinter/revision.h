/* Copyright 2015 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#ifndef FLINTER_REVISION_H
#define FLINTER_REVISION_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Version control system name, might be NULL. */
extern const char *const VCS_NAME;

/** Project URL, might be NULL. */
extern const char *const VCS_PROJECT_URL;

/** Project repository URL, might be NULL. */
extern const char *const VCS_PROJECT_ROOT;

/** Project revision, might be -1. */
extern const         int VCS_PROJECT_REVISION;

/** Project last changed timestamp, might be -1. */
extern const      time_t VCS_PROJECT_TIMESTAMP;

/** Project version, might be NULL. */
extern const char *const VCS_PROJECT_VERSION;

/** Build hostname. */
extern const char *const VCS_BUILD_HOSTNAME;

/** Build timestamp. */
extern const      time_t VCS_BUILD_TIMESTAMP;

/** Print information to STDOUT, useful in main(). */
extern void revision_print(void);

/** Try to retrieve version number from project URL. */
extern int revision_get_project_version(char *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_REVISION_H */

