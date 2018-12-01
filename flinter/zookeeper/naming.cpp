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

#include "flinter/zookeeper/naming.h"

#include <iomanip>
#include <sstream>

#include "flinter/thread/mutex_locker.h"
#include "flinter/logger.h"
#include "flinter/utility.h"

namespace flinter {

Naming::Naming() : _cb(NULL)
                 , _zk(NULL)
                 , _zkt(new ZooKeeperTracker)
{
    // Intended left blank.
}

Naming::~Naming()
{
    Shutdown();
    delete _zkt;
}

bool Naming::RemoveNode()
{
    if (_actual_path.empty()) {
        return true;
    }

    int ret = _zk->Erase(_actual_path.c_str());
    if (ret != ZOK) {
        CLOG.Warn("Naming: failed to remove node [%s]: %d: %s",
                  _actual_path.c_str(), ret, ZooKeeper::strerror(ret));

        return false;
    }

    LOG(TRACE) << "Naming: removed node [" << _actual_path << "]";
    _actual_path.clear();
    return true;
}

bool Naming::CreateNode(ZooKeeper *zk,
                        const std::string &naming_path,
                        const std::string &identity)
{
    if (!_actual_path.empty()) {
        return true;
    }

    std::ostringstream s;
    s << naming_path << "/" << identity << "_";

    std::string actual;
    std::string path = s.str();
    int ret = zk->Create(path.c_str(), "", &actual,
                         true, true, ZooKeeper::ACL_ANYONE_READ);

    if (ret != ZOK) {
        CLOG.Warn("Naming: failed to create node [%s]: %d: %s",
                  _actual_path.c_str(), ret, ZooKeeper::strerror(ret));

        return false;
    }

    _actual_path = actual;
    LOG(TRACE) << "Naming: created node [" << _actual_path << "]";
    return true;
}

bool Naming::Get(std::map<int32_t, std::string> *naming)
{
    if (!naming) {
        return false;
    }

    MutexLocker locker(&_mutex);
    *naming = _naming;
    return true;
}

bool Naming::Shutdown()
{
    MutexLocker locker(&_mutex);
    if (!_zk) {
        return true;
    }

    _zkt->Shutdown();
    RemoveNode();

    _actual_path.clear();
    _path.clear();
    _zk = NULL;
    _cb = NULL;
    return true;
}

void Naming::OnError(int /*error*/, const char *path)
{
    MutexLocker locker(&_mutex);
    RemoveNode();

    if (_cb) {
        _cb->OnError(path);
    }
}

void Naming::OnChildren(const char *path, const std::list<std::string> *children)
{
    if (!path || path[0] != '/') {
        return;
    }

    MutexLocker locker(&_mutex);
    _naming.clear();
    if (children) {
        for (std::list<std::string>::const_iterator p = children->begin();
             p != children->end(); ++p) {

            size_t offset = p->find_last_of('_');
            if (offset == std::string::npos ||
                offset == 0                 ||
                offset + 11 != p->length()  ){

                continue;
            }

            int32_t id = atoi32(p->c_str() + offset + 1);
            if (id < 0) {
                continue;
            }

            _naming.insert(std::make_pair(id, p->substr(0, offset)));
        }
    }

    if (_cb) {
        _cb->OnChanged(_naming);
    }
}

bool Naming::Watch(ZooKeeper *zk,
                   const std::string &naming_path,
                   Callback *cb)
{
    if (!zk                                          ||
        naming_path.length() < 2                     ||
        naming_path[0] != '/'                        ||
        naming_path[naming_path.length() - 1] == '/' ){

        return false;
    }

    MutexLocker locker(&_mutex);
    if (_zk) {
        return false;
    }

    return DoWatch(zk, naming_path, cb);
}

bool Naming::DoWatch(ZooKeeper *zk,
                     const std::string &naming_path,
                     Callback *cb)
{
    if (!_zkt->Initialize(zk)) {
        LOG(WARN) << "Naming: failed to initialize tracker.";
        return false;
    }

    if (!_zkt->Track(naming_path.c_str(), false, true, false, this)) {
        LOG(WARN) << "Naming: failed to initiate tracking.";
        _zkt->Shutdown();
        return false;
    }

    _path = naming_path;
    _cb = cb;
    _zk = zk;
    return true;
}

bool Naming::Attend(ZooKeeper *zk,
                    const std::string &naming_path,
                    const std::string &identity,
                    Callback *cb)
{
    if (!zk                                          ||
        naming_path.length() < 2                     ||
        naming_path[0] != '/'                        ||
        naming_path[naming_path.length() - 1] == '/' ||
        identity.empty()                             ){

        return false;
    }

    MutexLocker locker(&_mutex);
    if (_zk) {
        return false;
    }

    if (!CreateNode(zk, naming_path, identity)) {
        return false;
    }

    return DoWatch(zk, naming_path, cb);
}

} // namespace flinter
