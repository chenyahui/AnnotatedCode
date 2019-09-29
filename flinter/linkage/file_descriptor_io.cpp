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

#include "flinter/linkage/file_descriptor_io.h"

#include <assert.h>
#include <errno.h>

#include "flinter/linkage/interface.h"
#include "flinter/safeio.h"

namespace flinter {

FileDescriptorIo::FileDescriptorIo(Interface *i, bool connecting)
        : _i(i), _connecting(connecting)
{
    assert(i);
    assert(i->fd() >= 0);
}

FileDescriptorIo::~FileDescriptorIo()
{
    delete _i;
}

AbstractIo::Status FileDescriptorIo::Write(const void *buffer,
                                           size_t length,
                                           size_t *retlen)
{
    int fd = _i->fd();
    if (fd < 0) {
        return kStatusBug;
    }

    ssize_t ret = safe_write(fd, buffer, length);
    if (ret >= 0) {
        if (retlen) {
            *retlen = static_cast<size_t>(ret);
        }

        return kStatusOk;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return kStatusJammed;
    } else if (errno == EPIPE) {
        return kStatusClosed;
    } else {
        return kStatusError;
    }
}

AbstractIo::Status FileDescriptorIo::Read(void *buffer,
                                          size_t length,
                                          size_t *retlen,
                                          bool * /*more*/)
{
    int fd = _i->fd();
    if (fd < 0) {
        return kStatusBug;
    }

    ssize_t ret = safe_read(fd, buffer, length);
    if (ret > 0) {
        if (retlen) {
            *retlen = static_cast<size_t>(ret);
        }

        return kStatusOk;

    } else if (ret == 0) {
        return kStatusClosed;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return kStatusJammed;
    } else {
        return kStatusError;
    }
}

AbstractIo::Status FileDescriptorIo::Shutdown()
{
    int fd = _i->fd();
    if (fd < 0) {
        return kStatusOk;
    }

    if (!_i->Shutdown()) {
        return kStatusError;
    }

    return kStatusClosed;
}

AbstractIo::Status FileDescriptorIo::Connect()
{
    int fd = _i->fd();
    if (fd < 0) {
        return kStatusBug;
    }

    int ret = safe_test_if_connected(fd);
    if (ret != 0) {
        return kStatusError;
    }

    _connecting = false;
    return kStatusOk;
}

AbstractIo::Status FileDescriptorIo::Accept()
{
    return kStatusOk;
}

bool FileDescriptorIo::Initialize(Action *action,
                                  Action *next_action,
                                  bool *wanna_read,
                                  bool *wanna_write)
{
    if (_connecting) {
        *action      = kActionNone;
        *next_action = kActionConnect;
        *wanna_read  = false;
        *wanna_write = true;

    } else {
        *action      = kActionNone;
        *next_action = kActionNone;
        *wanna_read  = false;
        *wanna_write = false;
    }

    return true;
}

} // namespace flinter
