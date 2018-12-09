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

#ifndef FLINTER_TYPES_AUTO_BUFFER_H
#define FLINTER_TYPES_AUTO_BUFFER_H

#include <stdlib.h>

#include <stdexcept>

namespace flinter {

template <class T = unsigned char>
class AutoBuffer {
public:
    AutoBuffer() : _size(0), _buffer(NULL) {}
    explicit AutoBuffer(size_t size) : _size(size)
    {
        _buffer = reinterpret_cast<T *>(malloc(size * sizeof(T)));
        if (!_buffer) {
            throw std::bad_alloc();
        }
    }

    ~AutoBuffer()
    {
        free(_buffer);
    }

    void resize(size_t size)
    {
        void *tmp = realloc(_buffer, size * sizeof(T));
        if (!tmp) {
            throw std::bad_alloc();
        }

        _buffer = reinterpret_cast<T *>(tmp);
        _size = size;
    }

    T *get()
    {
        return _buffer;
    }

    const T *get() const
    {
        return _buffer;
    }

    operator T *()
    {
        return _buffer;
    }

    operator const T *() const
    {
        return _buffer;
    }

    T &operator [] (int i)
    {
        if (i < 0 || static_cast<size_t>(i) >= _size) {
            throw std::out_of_range("out of range");
        }

        return _buffer[i];
    }

    const T &operator [] (int i) const
    {
        if (i < 0 || static_cast<size_t>(i) >= _size) {
            throw std::out_of_range("out of range");
        }

        return _buffer[i];
    }

    size_t size() const
    {
        return _size;
    }

private:
    explicit AutoBuffer(const AutoBuffer &);
    AutoBuffer &operator = (const AutoBuffer &);

    size_t _size;
    T *_buffer;

}; // class AutoBuffer

} // namespace flinter

#endif // FLINTER_TYPES_AUTO_BUFFER_H
