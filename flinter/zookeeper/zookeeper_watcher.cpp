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

#include "flinter/zookeeper/zookeeper_watcher.h"

#include <assert.h>

#include <zookeeper/zookeeper.h>

#include "flinter/logger.h"

namespace flinter {

ZooKeeperWatcher::~ZooKeeperWatcher()
{
    // This is a pure virtual destructor.
}

void ZooKeeperWatcher::OnEvent(int type, int state, const char *path)
{
    if (type == ZOO_CREATED_EVENT) {
        OnCreated(state, path);
    } else if (type == ZOO_DELETED_EVENT) {
        OnErased(state, path);
    } else if (type == ZOO_CHANGED_EVENT) {
        OnChanged(state, path);
    } else if (type == ZOO_CHILD_EVENT) {
        OnChild(state, path);
    } else if (type == ZOO_SESSION_EVENT) {
        assert(path && !*path); // Empty string.
        OnSession(state);
    } else if (type == ZOO_NOTWATCHING_EVENT) {
        OnNotWatching(state, path);
    } else { // WTF?
        CLOG.Warn("ZooKeeperWatcher: unknown type (this=%p, state=%d, path=%s)",
                  this, state, path);
    }
}

void ZooKeeperWatcher::OnCreated(int state, const char *path)
{
    CLOG.Warn("ZooKeeperWatcher: unhandled CREATED (this=%p, state=%d, path=%s)",
              this, state, path);
}

void ZooKeeperWatcher::OnErased(int state, const char *path)
{
    CLOG.Warn("ZooKeeperWatcher: unhandled DELETED (this=%p, state=%d, path=%s)",
              this, state, path);
}

void ZooKeeperWatcher::OnChanged(int state, const char *path)
{
    CLOG.Warn("ZooKeeperWatcher: unhandled CHANGED (this=%p, state=%d, path=%s)",
              this, state, path);
}

void ZooKeeperWatcher::OnChild(int state, const char *path)
{
    CLOG.Warn("ZooKeeperWatcher: unhandled  CHILD  (this=%p, state=%d, path=%s)",
              this, state, path);
}

void ZooKeeperWatcher::OnSession(int state)
{
    CLOG.Debug("ZooKeeperWatcher: unhandled SESSION (this=%p, state=%d), it's OK.",
               this, state);
}

void ZooKeeperWatcher::OnNotWatching(int state, const char *path)
{
    CLOG.Warn("ZooKeeperWatcher: unhandled NOWATCH (this=%p, state=%d, path=%s)",
              this, state, path);
}

} // namespace flinter
