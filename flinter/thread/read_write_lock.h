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

/**
 * @file
 * @brief
 * Read-write lock is a "share reading and exclusive writing" lock. It allows
 * multiple readers to access data simultaneously, or only one writer to access
 * data exclusively, but not both.
 *
 * That is to say the writer can safely modify data before it unlocks the
 * read-write lock, until then can other accessors get unblocked. If there are
 * already readers accessing data, all writers should block.
 *
 * Non recursive, writer first.
 **/

#ifndef FLINTER_THREAD_READ_WRITE_LOCK_H
#define FLINTER_THREAD_READ_WRITE_LOCK_H

#include <flinter/common.h>

namespace flinter {

/// POSIX rw lock.
class ReadWriteLock {
public:
    ReadWriteLock();                            ///< Constructor.
    ~ReadWriteLock();                           ///< Destructor.

    void ReaderLock();                         ///< Reader lock.
    void WriterLock();                         ///< Writer lock.
    bool TryReaderLock();                     ///< Immediately returns.
    bool TryWriterLock();                     ///< Immediately returns.
    void Unlock();                              ///< Unlocks.

private:
    class Context;                              ///< Internally used.
    Context *_context;                          ///< Internally used.

    NON_COPYABLE(ReadWriteLock);                ///< Don't copy me.

}; // class ReadWriteLock

} // namespace flinter

#endif // FLINTER_THREAD_READ_WRITE_LOCK_H
