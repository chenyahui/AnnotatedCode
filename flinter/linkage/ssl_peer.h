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

#ifndef FLINTER_LINKAGE_SSL_PEER_H
#define FLINTER_LINKAGE_SSL_PEER_H

#include <stdint.h>

#include <string>

namespace flinter {

class SslPeer {
public:
    SslPeer(const std::string &version,
            const std::string &cipher)
            : _version(version)
            , _cipher(cipher)
            , _certificate(false) {}

    SslPeer(const std::string &version,
            const std::string &cipher,
            const std::string &subject_name,
            const std::string &issuer_name,
            const std::string &serial_number)
            : _serial_number(serial_number)
            , _subject_name(subject_name)
            , _issuer_name(issuer_name)
            , _version(version)
            , _cipher(cipher)
            , _certificate(true) {}

    const std::string &subject_name() const
    {
        return _subject_name;
    }

    const std::string &issuer_name() const
    {
        return _issuer_name;
    }

    const std::string &serial_number() const
    {
        return _serial_number;
    }

    const std::string &version() const
    {
        return _version;
    }

    const std::string &cipher() const
    {
        return _cipher;
    }

    bool certificate() const
    {
        return _certificate;
    }

private:
    std::string _serial_number;
    std::string _subject_name;
    std::string _issuer_name;
    std::string _version;
    std::string _cipher;
    bool _certificate;

}; // class SslPeer

} // namespace flinter

#endif // FLINTER_LINKAGE_SSL_PEER_H
