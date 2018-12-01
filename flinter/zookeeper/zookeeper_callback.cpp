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

#include "flinter/zookeeper/zookeeper_callback.h"

#include "flinter/logger.h"

namespace flinter {

ZooKeeperCallback::~ZooKeeperCallback()
{
    // This is a pure virtual destructor.
}

void ZooKeeperCallback::OnErase(int rc, const char *path)
{
    CLOG.Warn("ZooKeeperCallback: unhandled DELETE (this=%p, rc=%d, path=%s)",
              this, rc, path);
}

void ZooKeeperCallback::OnSetAcl(int rc, const char *path)
{
    CLOG.Warn("ZooKeeperCallback: unhandled SETACL (this=%p, rc=%d, path=%s)",
              this, rc, path);
}

void ZooKeeperCallback::OnCreate(int rc, const char *path, const char *callback_path)
{
    CLOG.Warn("ZooKeeperCallback: unhandled CREATE (this=%p, rc=%d, path=%s, callback_path=%s)",
              this, rc, path, callback_path);
}

void ZooKeeperCallback::OnSync(int rc, const char *path, const char *callback_path)
{
    CLOG.Warn("ZooKeeperCallback: unhandled  SYNC  (this=%p, rc=%d, path=%s, callback_path=%s)",
              this, rc, path, callback_path);
}

void ZooKeeperCallback::OnExists(int rc, const char *path, const struct Stat * /*stat*/)
{
    CLOG.Warn("ZooKeeperCallback: unhandled EXISTS (this=%p, rc=%d, path=%s, stat=<optimized>)",
              this, rc, path);
}

void ZooKeeperCallback::OnSet(int rc, const char *path, const struct Stat * /*stat*/)
{
    CLOG.Warn("ZooKeeperCallback: unhandled  SET   (this=%p, rc=%d, path=%s, stat=<optimized>)",
              this, rc, path);
}

void ZooKeeperCallback::OnGetChildren(int rc,
                                      const char *path,
                                      const std::list<std::string> &children,
                                      const struct Stat * /*stat*/)
{
    CLOG.Warn("ZooKeeperCallback: unhandled  GETC  "
              "(this=%p, rc=%d, path=%s, children={%lu}, stat=<optimized>)",
              this, rc, path, children.size());
}

void ZooKeeperCallback::OnGet(int rc,
                              const char *path,
                              const std::string &data,
                              const struct Stat * /*stat*/)
{
    CLOG.Warn("ZooKeeperCallback: unhandled  GET   "
              "(this=%p, rc=%d, path=%s, data={%lu}, stat=<optimized>)",
              this, rc, path, data.size());
}

void ZooKeeperCallback::OnGetAcl(int rc,
                                 const char *path,
                                 const ZooKeeper::Acl & /*acl*/,
                                 const struct Stat * /*stat*/)
{
    CLOG.Warn("ZooKeeperCallback: unhandled  GETA  "
              "(this=%p, rc=%d, path=%s, acl=<optimized>, stat=<optimized>)",
              this, rc, path);
}

} // namespace flinter
