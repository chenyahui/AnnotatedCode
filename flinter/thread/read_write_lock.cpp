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

#include "flinter/thread/read_write_lock.h"

#include <assert.h>
#include <errno.h>

#include <stdexcept>

#include "config.h"
#if defined(WIN32)
# include <Windows.h>
#elif HAVE_PTHREAD_H
# include <pthread.h>
#else
# error Unsupported: ReadWriteLock
#endif

namespace flinter {

#ifdef WIN32
# define _FIX(x) { if ((x) == INVALID_HANDLE_VALUE) (x) = NULL; }
# define _CLOSE(x) { if ((x)) CloseHandle((x)); }
class ReadWriteLock::Context {
public:
    Context() : _access(0), _reader_queue(0), _writer_queue(0)
    {
              _mutex = CreateMutex    (NULL,       FALSE, NULL);
        _reader_wait = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
        _writer_wait = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
        _FIX(_mutex); _FIX(_reader_wait); _FIX(_writer_wait);
        if (!_mutex || !_reader_wait || !_writer_wait) {
            _CLOSE(_mutex); _CLOSE(_reader_wait); _CLOSE(_writer_wait);
            throw std::runtime_error("ReadWriteLock::ReadWriteLock()");
        }
    }

    ~Context()
    {
        CloseHandle(_writer_wait);
        CloseHandle(_reader_wait);
        CloseHandle(_mutex);
    }

    int _access;
    int _reader_queue;
    int _writer_queue;
    HANDLE _mutex;
    HANDLE _reader_wait;
    HANDLE _writer_wait;

}; // class ReadWriteLock::Context

ReadWriteLock::ReadWriteLock() : _context(new Context)
{
    // Intended left blank.
}

ReadWriteLock::~ReadWriteLock()
{
    delete _context;
}

void ReadWriteLock::ReaderLock()
{
    if (WaitForSingleObject(_context->_mutex, INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("ReadWriteLock::ReaderLock()");
    }

    bool access_pending = (_context->_access < 0 || _context->_writer_queue > 0);
    if (access_pending) {
        ++_context->_reader_queue;
    } else {
        ++_context->_access;
    }

    if (!ReleaseMutex(_context->_mutex)) {
        throw std::runtime_error("ReadWriteLock::ReaderLock()");
    }

    if (access_pending) {
        if (WaitForSingleObject(_context->_reader_wait, INFINITE) != WAIT_OBJECT_0) {
            throw std::runtime_error("ReadWriteLock::ReaderLock()");
        }
    }
}

void ReadWriteLock::WriterLock()
{
    if (WaitForSingleObject(_context->_mutex, INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("ReadWriteLock::WriterLock()");
    }

    bool access_pending = (_context->_access != 0);
    if (access_pending) {
        ++_context->_writer_queue;
    } else {
        --_context->_access;
    }

    if (!ReleaseMutex(_context->_mutex)) {
        throw std::runtime_error("ReadWriteLock::WriterLock()");
    }

    if (access_pending) {
        if (WaitForSingleObject(_context->_writer_wait, INFINITE) != WAIT_OBJECT_0) {
            throw std::runtime_error("ReadWriteLock::WriterLock()");
        }
    }
}

bool ReadWriteLock::TryReaderLock()
{
    bool access_granted = false;
    if (WaitForSingleObject(_context->_mutex, INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("ReadWriteLock::TryReaderLock()");
    }

    if (_context->_access >= 0 && _context->_writer_queue == 0) {
        ++_context->_access;
        access_granted = true;
    }

    if (!ReleaseMutex(_context->_mutex)) {
        throw std::runtime_error("ReadWriteLock::TryReaderLock()");
    }

    return access_granted;
}

bool ReadWriteLock::TryWriterLock()
{
    bool access_granted = false;
    if (WaitForSingleObject(_context->_mutex, INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("ReadWriteLock::TryWriterLock()");
    }

    if (_context->_access == 0) {
        --_context->_access;
        access_granted = true;
    }

    if (!ReleaseMutex(_context->_mutex)) {
        throw std::runtime_error("ReadWriteLock::TryWriterLock()");
    }

    return access_granted;
}

void ReadWriteLock::Unlock()
{
    if (WaitForSingleObject(_context->_mutex, INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("ReadWriteLock::Unlock()");
    }

    if (_context->_access > 0) {
        --_context->_access;
    } else {
        ++_context->_access;
    }

    HANDLE fortune = NULL;
    LONG cookies = 0;

    if (_context->_access == 0) {
        if (_context->_writer_queue > 0) {
            --_context->_access;
            --_context->_writer_queue;
            fortune = _context->_writer_wait;
            cookies = 1;
        } else if (_context->_reader_queue > 0) {
            _context->_access = _context->_reader_queue;
            _context->_reader_queue = 0;
            fortune = _context->_reader_wait;
            cookies = _context->_access;
        }
    }

    if (fortune != NULL) {
        if (!ReleaseSemaphore(fortune, cookies, NULL)) {
            throw std::runtime_error("ReadWriteLock::Unlock()");
        }
    }

    if (!ReleaseMutex(_context->_mutex)) {
        throw std::runtime_error("ReadWriteLock::Unlock()");
    }
}
#else
ReadWriteLock::ReadWriteLock() : _context(NULL)
{
    pthread_rwlock_t *rwlock = new pthread_rwlock_t;

#if HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP
    pthread_rwlockattr_t attr;
    if (pthread_rwlockattr_init(&attr)) {
        delete rwlock;
        throw std::runtime_error("ReadWriteLock::ReadWriteLock()");
    }

    if (pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) ||
        pthread_rwlock_init(rwlock, &attr)) {

        pthread_rwlockattr_destroy(&attr);
        delete rwlock;
        throw std::runtime_error("ReadWriteLock::ReadWriteLock()");
    }

    pthread_rwlockattr_destroy(&attr);
#else
    if (pthread_rwlock_init(rwlock, NULL)) {
        delete rwlock;
        throw std::runtime_error("ReadWriteLock::ReadWriteLock()");
    }
#endif

    _context = reinterpret_cast<Context *>(rwlock);
}

ReadWriteLock::~ReadWriteLock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    pthread_rwlock_destroy(rwlock);
    delete rwlock;
}

void ReadWriteLock::ReaderLock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    if (pthread_rwlock_rdlock(rwlock)) {
        throw std::runtime_error("ReadWriteLock::ReaderLock()");
    }
}

void ReadWriteLock::WriterLock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    if (pthread_rwlock_wrlock(rwlock)) {
        throw std::runtime_error("ReadWriteLock::WriterLock()");
    }
}

bool ReadWriteLock::TryReaderLock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    if (pthread_rwlock_tryrdlock(rwlock)) {
        if (errno == EBUSY) {
            return false;
        }

        throw std::runtime_error("ReadWriteLock::TryReaderLock()");
    }

    return true;
}

bool ReadWriteLock::TryWriterLock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    if (pthread_rwlock_trywrlock(rwlock)) {
        if (errno == EBUSY) {
            return false;
        }

        throw std::runtime_error("ReadWriteLock::TryWriterLock()");
    }

    return true;
}

void ReadWriteLock::Unlock()
{
    assert(_context);
    pthread_rwlock_t *rwlock = reinterpret_cast<pthread_rwlock_t *>(_context);
    if (pthread_rwlock_unlock(rwlock)) {
        throw std::runtime_error("ReadWriteLock::Unlock()");
    }
}
#endif

} // namespace flinter
