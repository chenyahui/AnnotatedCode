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

#ifndef FLINTER_RMDIRS_H
#define FLINTER_RMDIRS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Same function as "rmdir -fr pathname" */
extern int rmdirs(const char *pathname);

/** Same function as "rmdir -fr pathname", but leave pathname itself along */
/** If pathname is not a directory, then pathname is left along. */
extern int rmdirs_inside(const char *pathname);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FLINTER_RMDIRS_H */
