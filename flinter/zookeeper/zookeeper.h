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

#ifndef FLINTER_ZOOKEEPER_ZOOKEEPER_H
#define FLINTER_ZOOKEEPER_ZOOKEEPER_H

#include <stdint.h>
#include <stdio.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include <zookeeper/zookeeper.h>

#include <flinter/thread/mutex.h>

namespace flinter {

class ZooKeeperCallback;
class ZooKeeperWatcher;

/// ZooKeeper is a low level wrapper around ZooKeeper API.
class ZooKeeper {
public:
    typedef std::pair<std::string, std::string> Id;
    typedef std::map<Id, int32_t> Acl;

    static const Acl ACL_ANYONE_OPEN;
    static const Acl ACL_ANYONE_READ;

    ZooKeeper();            ///< Constructor
    virtual ~ZooKeeper();   ///< Destructor

    /**
     * @param hosts in the form of host:port,host:port...
     * @return ZOK if good.
     */
    int Initialize(const std::string &hosts, int64_t timeout = kDefaultTimeout);

    /// Might block.
    int Shutdown();

    /// No longer interested in events.
    void Detach(ZooKeeperWatcher *zkw, ZooKeeperCallback *zkc);

    int state() const;

    /// Might be unreliable if connected then immediately disconnected.
    /// @param timeout to wait, <0 for infinity.
    bool WaitUntilConnected(int64_t timeout = -1) const;

    /// Might be unreliable if connected then immediately disconnected.
    /// @retval <0 for error, 0 for connected, >0 for pending.
    int is_connected() const;

    /// Set ZooKeeper log stream.
    static void SetLog(FILE *stream, const ZooLogLevel &level);

    // If any one of the methods fails, check this one.
    static const char *strerror(int error);

    // Synchronized operations.

    /**
     * Get data and/or stat.
     * @param path ZooKeeper node path
     * @param data must not be NULL but can be empty.
     * @param stat return stat, can be NULL.
     * @param version -1 to ignore, see ZooKeeper documents.
     */
    int Set(const char *path,
            const std::string &data,
            struct Stat *stat = NULL,
            int version = -1);

    /**
     * Get data and/or stat.
     * @param path ZooKeeper node path
     * @param data can be NULL, or should resize() ahead to make a large enough buffer.
     * @param stat return stat, can be NULL.
     * @param watcher set a watcher if not NULL
     */
    int Get(const char *path,
            std::string *data,
            struct Stat *stat = NULL,
            ZooKeeperWatcher *watcher = NULL);

    /**
     * Check if a node exists.
     * @param path ZooKeeper node path
     * @param stat must not be NULL.
     * @param watcher set a watcher if not NULL
     */
    int Exists(const char *path, struct Stat *stat, ZooKeeperWatcher *watcher = NULL);

    /**
     * @param path ZooKeeper node path
     * @param children return children list, can be NULL
     * @param stat return stat, can be NULL
     * @param watcher set a watcher if not NULL
     */
    int GetChildren(const char *path,
                    std::list<std::string> *children,
                    struct Stat *stat = NULL,
                    ZooKeeperWatcher *watcher = NULL);

    /**
     * ACL is set to anyone any access.
     * @param path ZooKeeper node path
     * @param data must not be NULL but can be empty
     * @param actual_path can be NULL
     * @param ephemeral node got automatically deleted when client disconnects
     * @param sequence a %10d sequence number is appended to node path.
     * @param acl set the ACL of the created node.
     * @warning Sequence number is an int32 and will overflow to negative value.
     */
    int Create(const char *path,
               const char *data,
               std::string *actual_path = NULL,
               bool ephemeral = false,
               bool sequence = false,
               const Acl &acl = ACL_ANYONE_OPEN);

    /**
     * If there're multiple entries with same id, the resulting ACL entry will combine all
     *     the permissions into a single entry.
     *
     * @param path ZooKeeper node path.
     * @param acl output acl list, must not be null.
     * @param stat stat of the node, might be null.
     */
    int GetAcl(const char *path, Acl *acl, struct Stat *stat = NULL);

    /**
     * @param path ZooKeeper node path.
     * @param acl input acl list.
     * @param version -1 to ignore, see ZooKeeper documents.
     */
    int SetAcl(const char *path, const Acl &acl, int version = -1);

    /**
     * @param path ZooKeeper node path.
     * @param version must match the version on server or the method fails. -1 for not
     *        checking version.
     */
    int Erase(const char *path, int version = -1);

    /// @sa Get()
    int AsyncGet(const char *path,
                 ZooKeeperCallback *zkc,
                 ZooKeeperWatcher *watcher = NULL);

    /// @sa Exists()
    int AsyncExists(const char *path,
                    ZooKeeperCallback *zkc,
                    ZooKeeperWatcher *watcher = NULL);

    /// @sa GetChildren()
    int AsyncGetChildren(const char *path,
                         ZooKeeperCallback *zkc,
                         ZooKeeperWatcher *watcher = NULL);

    static const int64_t kDefaultTimeout = 10000000000LL; ///< 10s

private:
    enum Operation {
        OP_SESSION, ///< Session

        OP_CREATE,  ///< Create
        OP_DELETE,  ///< Delete
        OP_EXISTS,  ///< Exists
        OP_SYNC,    ///< Synchronize
        OP_GET,     ///< Get data
        OP_SET,     ///< Set data
        OP_GETA,    ///< Get ACL
        OP_SETA,    ///< Set ACL
        OP_GETC,    ///< Get children

    }; // enum Operation

    // Asynchronized operations related.
    class Pending {
    public:
        /// Session pending constructor.
        Pending(ZooKeeper *zk) : _zk(zk), _op(OP_SESSION), _zkw(NULL)
                               , _zkc(NULL), _zkw_detached(false) {}

        /// Ordinary pending constructor.
        Pending(ZooKeeper *zk,
                Operation op,
                const char *path,
                ZooKeeperWatcher *zkw,
                ZooKeeperCallback *zkc) : _zk(zk), _op(op), _path(path), _zkw(zkw)
                                        , _zkc(zkc), _zkw_detached(false) {}

        /// Getter.
        ZooKeeper *zk() const               { return _zk; }

        /// Getter.
        Operation op() const                { return _op; }

        /// Getter.
        ZooKeeperWatcher *zkw() const       { return _zkw; }

        /// Getter.
        ZooKeeperCallback *zkc() const      { return _zkc; }

        /// Getter.
        bool zkw_detached() const           { return _zkw_detached; }

        /// Getter.
        const char *path() const            { return _path.c_str(); }

        /// Setter.
        void clear_callback()               { _zkc = NULL; }

        /// Setter.
        void clear_watcher()                { _zkw = NULL; _zkw_detached = false; }

        /// Setter.
        void detach_watcher()               { _zkw = NULL; _zkw_detached = true; }

    private:
        ZooKeeper *_zk;

        Operation _op;
        std::string _path;
        ZooKeeperWatcher *_zkw;
        ZooKeeperCallback *_zkc;
        bool _zkw_detached;

    }; // class Pending

    /// Global callback entry.
    static void GlobalWatcher(zhandle_t *zh,
                              int type,
                              int state,
                              const char *path,
                              void *watcherCtx);

    /// Global callback entry.
    static void GlobalVoidCompletion(int rc, const void *data);

    /// Global callback entry.
    static void GlobalStatCompletion(int rc, const struct Stat *stat, const void *data);

    /// Global callback entry.
    static void GlobalDataCompletion(int rc,
                                     const char *value,
                                     int value_len,
                                     const struct Stat *stat,
                                     const void *data);

    /// Global callback entry.
    static void GlobalStringsStatCompletion(int rc,
                                            const struct String_vector *strings,
                                            const struct Stat *stat,
                                            const void *data);

    /// Global callback entry.
    static void GlobalStringCompletion(int rc, const char *value, const void *data);

    /// Global callback entry.
    static void GlobalAclCompletion(int rc,
                                    struct ACL_vector *acl,
                                    struct Stat *stat,
                                    const void *data);

    /// ACL translator.
    static void Translate(const Acl &acl, struct ACL_vector *result);

    /// ACL translator.
    static void Translate(const struct ACL_vector *acl, Acl *result);

    /// A pending has returned.
    void ErasePending(Pending *pending);

    /// Global session event handler.
    void OnSession(int state);

    /// Global session event handler for connected state.
    void OnConnected();

    /// Got a new session, try to resume.
    /// @warning Call while unlocked.
    void ResumeSession(const std::set<Pending *> &pending);

    /// Establish a new connection. Call while unlocked.
    /// Only used when session expires.
    /// @sa OnSession()
    int Reconnect();

    /// Establish a new connection. Call while locked.
    int ReconnectNolock();

    /// @warning always call this method while unlocked, since zookeeper_close() is called
    ///          within which will block and wait for all workers, while workers might
    ///          acquire mutexes again.
    int Disconnect(zhandle_t *handle);

    std::set<Pending *> _pending;   ///< Pending watches.
    bool _resuming;                 ///< Are we resuming? Async callbacks need this.

    Pending _session_pending;       ///< Session watcher.
    clientid_t _client_id;          ///< Show client id.

    std::string _hosts;             ///< ZooKeeper servers.
    zhandle_t *_handle;             ///< ZooKeeper handle.
    bool _connecting;               ///< If true, don't try to alter connection handle.
    int64_t _timeout;               ///< ZooKeeper default timeout.

    mutable Mutex _mutex;           ///< Global lock.

}; // class ZooKeeper

} // namespace flinter

#endif // FLINTER_ZOOKEEPER_ZOOKEEPER_H
