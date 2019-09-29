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

#include "flinter/types/uuid.h"

#ifdef WIN32
# include <Rpc.h>
# pragma comment (lib, "Rpcrt4.lib")
# pragma comment (lib, "Ws2_32.lib")
# define uuid_copy(dst,src) {(dst)=(src);}
# define uuid_clear(uu) UuidCreateNil(&(uu))
# define uuid_generate(uu) UuidCreate(&(uu))
# define uuid_parse(in,uu) UuidFromStringA((RPC_CSTR)(in),&(uu))
#else
#include <uuid/uuid.h>
#endif

#include <assert.h>
#include <string.h>

#include <stdexcept>

namespace flinter {

class Uuid::Context {
public:
    uuid_t _uuid;
};

Uuid::Uuid() : _context(new Context)
{
    Clear();
}

Uuid::Uuid(const char *value) : _context(new Context)
{
    if (!Parse(value)) {
        throw std::invalid_argument("invalid UUID string");
    }
}

Uuid::Uuid(const std::string &value) : _context(new Context)
{
    if (!Parse(value)) {
        throw std::invalid_argument("invalid UUID string");
    }
}

Uuid::~Uuid()
{
    delete _context;
}

Uuid::Uuid(const Uuid &other) : _context(new Context)
{
    uuid_copy(_context->_uuid, other._context->_uuid);
}

Uuid::operator bool() const
{
    return !IsNull();
}

bool Uuid::IsNull() const
{
#ifdef WIN32
    RPC_STATUS status;
    return !!UuidIsNil(&_context->_uuid, &status);
#else
    return !!uuid_is_null(_context->_uuid);
#endif
}

void Uuid::Generate()
{
    uuid_generate(_context->_uuid);
}

void Uuid::Clear()
{
    uuid_clear(_context->_uuid);
}

bool Uuid::Parse(const char *value)
{
    if (!value || !*value) {
        return false;
    }

    uuid_t uuid;
    if (uuid_parse(value, uuid) < 0) {
        uuid_clear(_context->_uuid);
        return false;
    }

    uuid_copy(_context->_uuid, uuid);
    return true;
}

bool Uuid::Parse(const std::string &value)
{
    return Parse(value.c_str());
}

std::string Uuid::str() const
{
#ifdef WIN32
    RPC_CSTR buffer;
    if (UuidToStringA(&_context->_uuid, &buffer) != RPC_S_OK) {
        return std::string();
    }

    std::string result = reinterpret_cast<const char *>(buffer);
    RpcStringFreeA(&buffer);
    return result;
#else
    char buffer[40];
    uuid_unparse(_context->_uuid, buffer);
    return buffer;
#endif
}

int Uuid::Compare(const Uuid &other) const
{
    if (this == &other) {
        return true;
    }

#ifdef WIN32
    RPC_STATUS status;
    return UuidCompare(&_context->_uuid, &other._context->_uuid, &status);
#else
    return uuid_compare(_context->_uuid, other._context->_uuid);
#endif
}

Uuid &Uuid::operator = (const Uuid &other)
{
    if (this == &other) {
        return *this;
    }

    uuid_copy(_context->_uuid, other._context->_uuid);
    return *this;
}

void Uuid::Save(void *buffer) const
{
    if (!buffer) {
        return;
    }

#ifdef WIN32
    // Padding can cause breaking down. Little endian.
    unsigned char *ptr = reinterpret_cast<unsigned char *>(buffer);
    *(u_long  *)(ptr + 0) = htonl(_context->_uuid.Data1);
    *(u_short *)(ptr + 4) = htons(_context->_uuid.Data2);
    *(u_short *)(ptr + 6) = htons(_context->_uuid.Data3);
    memcpy(ptr + 8, &_context->_uuid.Data4, 8);
#else
    memcpy(buffer, _context->_uuid, sizeof(uuid_t));
#endif
}

void Uuid::Load(const void *buffer)
{
    if (!buffer) {
        return;
    }

#ifdef WIN32
    // Padding can cause breaking down. Little endian.
    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(buffer);
    _context->_uuid.Data1 = ntohl(*(u_long  *)(ptr + 0));
    _context->_uuid.Data2 = ntohs(*(u_short *)(ptr + 4));
    _context->_uuid.Data3 = ntohs(*(u_short *)(ptr + 6));
    memcpy(&_context->_uuid.Data4, ptr + 8, 8);
#else
    memcpy(_context->_uuid, buffer, sizeof(uuid_t));
#endif
}

bool Uuid::IsValidString(const char *str)
{
    Uuid u;
    return u.Parse(str);
}

bool Uuid::IsValidString(const std::string &str)
{
    Uuid u;
    return u.Parse(str);
}

} // namespace flinter
