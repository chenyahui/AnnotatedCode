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

#include "flinter/zookeeper/council.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <algorithm>
#include <iomanip>
#include <list>
#include <set>
#include <sstream>
#include <string>

#include "flinter/thread/mutex_locker.h"
#include "flinter/logger.h"
#include "flinter/common.h"
#include "flinter/msleep.h"
#include "flinter/utility.h"

#include "flinter/zookeeper/zookeeper.h"

#include "config.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

namespace flinter {

Council::Callback::~Callback()
{
    // This is a pure virtual destructor.
}

Council::Council() : _zk(NULL)
                   , _zkt(new ZooKeeperTracker)
                   , _actual_id(-1)
                   , _cb(NULL)
                   , _id(-1)
                   , _size(-1)
{
    // Intended left blank.
}

Council::~Council()
{
    Shutdown();
    delete _zkt;
}

long Council::id() const
{
    MutexLocker locker(&_mutex);
    return _id;
}

long Council::size() const
{
    MutexLocker locker(&_mutex);
    return _size;
}

int Council::IsElected() const
{
    MutexLocker locker(&_mutex);
    if (!_zk || _id < -1) {
        return -1;
    }

    if (_id >= 0) {
        return 0;
    }

    return 1;
}

bool Council::WaitUntilAttended() const
{
    MutexLocker locker(&_mutex);
    if (!_zk) {
        return false;
    }

    while (_id < 0) {
        if (_id < -1) {
            return false;
        }

        _condition.Wait(&_mutex);
    }

    return true;
}

void Council::OnError(int error, const char *path)
{
    if (!path || !*path) {
        path = "<NULL>";
    }

    MutexLocker locker(&_mutex);
    if (error == ZSESSIONEXPIRED) {
        CLOG.Trace("Council: session expired, trying to resume...");
        OnLost();

    } else {
        CLOG.Warn("Council: got a ZooKeeper error on [%s]: %d: %s",
                  path, error, ZooKeeper::strerror(error));

        OnFired(false);
    }

    // Please, if there's another error tell me again.
    _zkt->ClearErrorState();
}

void Council::OnLost()
{
    if (_id >= 0) {
        CLOG.Warn("Council: got lost as councilman %d", _id);
    }

    DoLostClean(true);

    // This one will not be reset.
    _actual_id = -1;
    _size = -1;
}

void Council::OnQuit()
{
    if (_id >= 0) {
        CLOG.Info("Council: quit as councilman %d", _id);
    }

    DoQuitClean(true);
}

void Council::OnFired(bool recoverable)
{
    if (_id >= 0) {
        CLOG.Warn("Council: got fired as councilman %d", _id);
    }

    DoQuitClean(recoverable);
}

void Council::OnShrinked(bool recoverable)
{
    if (_id >= 0) {
        CLOG.Warn("Council: got fired as councilman %d", _id);
    }

    DoLostClean(recoverable);
}

void Council::DoQuitClean(bool recoverable)
{
    assert(_election_path.length());

    if (_actual_id >= 0) {
        std::ostringstream s;
        s << _election_path << "/" << _node_name << "_"
          << std::setfill('0') << std::setw(10) << _actual_id;

        std::string path = s.str();

        // Even if the deletion was a failure...
        int ret = _zk->Erase(path.c_str());
        if (ret == ZOK) {
            CLOG.Trace("Council: removed [%s].", path.c_str());

        } else {
            // Oh no...
            recoverable = false;
            CLOG.Warn("Council: failed to remove [%s]: %d: %s",
                      path.c_str(), ret, ZooKeeper::strerror(ret));
        }


        _actual_id = -1;
    }

    DoLostClean(recoverable);
}

void Council::DoLostClean(bool recoverable)
{
    bool notify = false;

    // If we've notified the clients about the successful election, or the error is
    // unrecoverable, then we must tell them.
    if (_id >= 0 || !recoverable) {
        notify = true;
    }

    ResetState(recoverable);
    _condition.WakeAll();

    if (notify && _cb) {
        _cb->OnFired(_election_path, recoverable);
    }
}

void Council::ResetState(bool recoverable)
{
    if (recoverable) {
        _id = -1;

    } else {
        _id = -2;
        _size = -1;
        _actual_id = -1;
    }
}

void Council::OnData(const char *path, const std::string *data)
{
    MutexLocker locker(&_mutex);
    size_t len = _election_path.length();
    if (_election_path.compare(0, len, path, len)) { // Weird.
        return;
    }

    if (_election_path.compare(path) == 0) { // Election node itself.
        if (!data) {
            OnFired(true);
            return;
        }

        int32_t size = atoi32(data->c_str());
        if (size <= 0) {
            CLOG.Warn("Council: invalid council size: %s", data->c_str());

            _size = -1;
            OnShrinked(true);
            return;
        }

        // Nice work, we've got the maximum number.
        CLOG.Trace("Council: got council size: %d", size);
        _size = size;
        Attend();

    } else {
        if (data) { // Not interested.
            return;
        }

        if (_id >= 0) { // Not interested.
            return;
        }

        CLOG.Trace("Council: tracked node %s is missing.", path);
        _zkt->Cancel(path, this);
        Attend();
    }
}

bool Council::Initialize(ZooKeeper *zk, const std::string &election_path, Callback *cb)
{
    MutexLocker locker(&_mutex);
    if (_zk || !zk || election_path.length() < 2 || election_path[0] != '/' ||
        election_path[election_path.length() - 1] == '/') {

        return false;
    }

    if (!_zkt->Initialize(zk)) {
        return false;
    }

    if (!_zkt->Track(election_path.c_str(), true, false, true, this)) {
        _zkt->Shutdown();
        return false;
    }

    _zk = zk;
    _cb = cb;
    _election_path = election_path;

    ResetState(false);
    _id = -1;
    return true;
}

bool Council::UpdateName()
{
    if (!_name.empty()) {
        _node_name = _name;
        return true;
    }

    char buffer[HOST_NAME_MAX + 1];
    if (gethostname(buffer, sizeof(buffer))) {
        return false;
    }

    _node_name = buffer;
    return true;
}

bool Council::CreateNodeAsNeeded()
{
    if (_actual_id >= 0) {
        return true;
    }

    if (!UpdateName()) {
        OnFired(false);
        return false;
    }

    std::string actual;
    std::string path = _election_path;
    path.append("/").append(_node_name).append("_");
    int error = _zk->Create(path.c_str(), "", &actual, true, true,
                            ZooKeeper::ACL_ANYONE_READ);

    if (error != ZOK) {
        _size = -1;
        if (error == ZNONODE) { // Parent got deleted?
            CLOG.Warn("Council: council node deleted, retry...");

        } else { // All other errors are not acceptable.
            CLOG.Warn("Council: failed to create councilman node: %d: %s",
                      error, ZooKeeper::strerror(error));
        }

        return false;
    }

    size_t offset = _election_path.length() + 1 + _node_name.length() + 1;
    if (actual.length() != offset + 10                     ||
        (_actual_id = atoi32(actual.c_str() + offset)) < 0 ){

        CLOG.Warn("Council: invalid resulting councilman node: %s", actual.c_str());
        _zk->Erase(actual.c_str());
        return false;
    }

    CLOG.Trace("Council: created councilman node: %d", _actual_id);
    return true;
}

bool Council::GetCandidates(std::map<int32_t, std::string> *candidates)
{
    std::list<std::string> children;
    int error = _zk->GetChildren(_election_path.c_str(), &children);
    if (error != ZOK) {
        _size = -1;
        if (error == ZNONODE) {
            CLOG.Warn("Council: council node deleted, retry...");

        } else {
            CLOG.Warn("Council: failed to get councilman list: %d: %s",
                      error, ZooKeeper::strerror(error));
        }

        return false;
    }

    CLOG.Debug("Council: got %lu children.", children.size());

    candidates->clear();
    for (std::list<std::string>::const_iterator p = children.begin(); p != children.end(); ++p) {
        const std::string &s = *p;
        size_t pos = s.find_last_of("_");
        if (pos == std::string::npos || pos == 0 || pos + 11 != s.length()) {
            continue;
        }

        int32_t i = atoi32(p->c_str() + pos + 1);
        if (i >= 0) {
            candidates->insert(std::make_pair(i, s.substr(0, pos)));
        }
    }

    CLOG.Debug("Council: got %lu valid candidates.", candidates->size());
    return true;
}

void Council::Attend()
{
    if (!CreateNodeAsNeeded()) {
        OnFired(false);
        return;
    }

    assert(_actual_id >= 0);
    std::map<int32_t, std::string> candidates;
    if (!GetCandidates(&candidates)) {
        return;
    }

    if (candidates.find(_actual_id) == candidates.end()) {
        CLOG.Error("Council: my actual node %d is missing.", _actual_id);
        OnFired(false);
        return;
    }

    int32_t pos = 0;
    for (std::map<int32_t, std::string>::const_iterator
         p = candidates.begin(); p != candidates.end(); ++p) {

        if (p->first <= _actual_id) {
            ++pos;
            continue;
        }

        break;
    }

    if (pos <= _size) { // Elected.
        if (_id >= 0) { // Status not changed.
            return;
        }

        _id = _actual_id;
        _condition.WakeAll();
        CLOG.Info("Council: elected as councilman: %d", _id);

        if (_cb) {
            _cb->OnElected(_election_path, _id);
        }

    } else {
        CLOG.Trace("Council: council needs %d councilmen but is now full, keep track.", _size);
        std::map<int32_t, std::string>::const_iterator p = candidates.find(_actual_id);
        assert(p != candidates.begin());
        assert(p != candidates.end());

        bool recoverable = true;
        for (int32_t i = 0; i < _size; ++i) {
            --p;

            std::ostringstream s;
            s << _election_path << "/" << p->second << "_"
              << std::setfill('0') << std::setw(10) << p->first;

            std::string path = s.str();
            if (!_zkt->TrackExisting(path.c_str(), true, false, this)) {
                CLOG.Warn("Council: failed track node %s", path.c_str());
                recoverable = false;
                break;
            }

            CLOG.Trace("Council: tracking %s...", path.c_str());
        }

        OnShrinked(recoverable);
    }
}

bool Council::Shutdown()
{
    MutexLocker locker(&_mutex);

    if (!_zk) {
        return true;
    }

    // Critical, you've been warned.
    _zkt->Shutdown();
    OnQuit();

    _zk = NULL;
    _cb = NULL;
    _election_path.clear();
    return true;
}

} // namespace flinter
