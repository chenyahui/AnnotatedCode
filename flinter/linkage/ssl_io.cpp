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

#include "flinter/linkage/ssl_io.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <stdexcept>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "flinter/linkage/interface.h"
#include "flinter/linkage/ssl_context.h"
#include "flinter/linkage/ssl_peer.h"
#include "flinter/logger.h"
#include "flinter/safeio.h"

namespace flinter {

SslIo::SslIo(Interface *i,
             bool client_or_server,
             bool socket_connecting,
             SslContext *context)
        : _ssl(SSL_new(context->context()))
        , _client_mode(client_or_server)
        , _i(i)
        , _connecting(socket_connecting)
        , _peer(NULL)
{
    assert(i);
    assert(i->fd() >= 0);
    assert(context);

    if (!SSL_set_fd(_ssl, i->fd())) {
        throw std::runtime_error("SslIo: setting fd");
    }
}

SslIo::~SslIo()
{
    delete _i;
    delete _peer;
    SSL_free(_ssl);
}

const char *SslIo::GetActionString(const Action &in_progress)
{
    switch (in_progress) {
    case kActionRead:
        return "reading";

    case kActionWrite:
        return "writing";

    case kActionAccept:
        return "accepting";

    case kActionConnect:
        return "connecting";

    case kActionShutdown:
        return "closing";

    default:
        assert(false);
        return NULL;
    };
}

AbstractIo::Status SslIo::HandleError(const Action &in_progress, int ret)
{
    int fd = _i->fd();
    const char *actstr = GetActionString(in_progress);
    if (!actstr) {
        return kStatusError;
    }

    int r = SSL_get_error(_ssl, ret);
    if (r == SSL_ERROR_WANT_READ) {
        CLOG.Verbose("Linkage: %s but want to read for fd = %d", actstr, fd);
        return kStatusWannaRead;

    } else if (r == SSL_ERROR_WANT_WRITE) {
        CLOG.Verbose("Linkage: %s but want to write for fd = %d", actstr, fd);
        return kStatusWannaWrite;

    } else if (r == SSL_ERROR_ZERO_RETURN) {
        CLOG.Verbose("Linkage: %s but connection closed for fd = %d", actstr, fd);
        return kStatusClosed;

    } else if (r == SSL_ERROR_SYSCALL) {
        if (errno == 0) {
            CLOG.Verbose("Linkage: %s but peer hung for fd = %d", actstr, fd);
            return kStatusClosed;
        }

        CLOG.Verbose("Linkage: %s but system error for fd = %d: %d: %s",
                     actstr, fd, errno, strerror(errno));

        return kStatusError;

    } else if (r == SSL_ERROR_WANT_X509_LOOKUP) {
        CLOG.Verbose("Linkage: %s but failed to verify peer certificate for fd = %d",
                     actstr, fd);

        errno = EACCES;
        return kStatusError;

    } else if (r == SSL_ERROR_SSL) {
        unsigned long err = ERR_get_error();
        CLOG.Verbose("Linkage: %s but error occurred for fd = %d: [%d:%d:%d] %s:%s:%s ",
                     actstr, fd,
                     ERR_GET_LIB(err), ERR_GET_FUNC(err), ERR_GET_REASON(err),
                     ERR_lib_error_string(err), ERR_func_error_string(err),
                     ERR_reason_error_string(err));

        errno = EPERM;
        return kStatusError;

    } else {
        CLOG.Verbose("Linkage: %s but unknown ret for fd = %d: %d: %d",
                     actstr, fd, ret, r);

        errno = EBADMSG;
        return kStatusError;
    }
}

AbstractIo::Status SslIo::Write(const void *buffer, size_t length, size_t *retlen)
{
    if (length > INT_MAX) {
        return kStatusBug;
    }

    int fd = _i->fd();
    int ret = SSL_write(_ssl, buffer, static_cast<int>(length));
    if (ret > 0) {
        if (retlen) {
            *retlen = static_cast<size_t>(ret);
        }

        CLOG.Verbose("Linkage: sending [%lu] bytes and sent [%d] "
                     "bytes for fd = %d", length, ret, fd);

        return kStatusOk;
    }

    return HandleError(kActionWrite, ret);
}

AbstractIo::Status SslIo::Read(void *buffer,
                               size_t length,
                               size_t *retlen,
                               bool *more)
{
    if (length > INT_MAX) {
        return kStatusBug;
    }

    int fd = _i->fd();
    int ret = SSL_read(_ssl, buffer, static_cast<int>(length));
    if (ret > 0) {
        if (retlen) {
            *retlen = static_cast<size_t>(ret);
        }

        CLOG.Verbose("Linkage: read [%d] bytes for fd = %d", ret, fd);
        *more = !!SSL_pending(_ssl);
        return kStatusOk;
    }

    return HandleError(kActionRead, ret);
}

bool SslIo::OnHandshaked()
{
    int fd = _i->fd();
    std::string version = SSL_get_version(_ssl);
    std::string cipher = SSL_get_cipher_name(_ssl);
    CLOG.Verbose("Linkage: SSL handshaked with [%s/%s] for fd = %d",
                 version.c_str(), cipher.c_str(), fd);

    X509 *x509 = SSL_get_peer_certificate(_ssl);
    if (!x509) {
        CLOG.Verbose("Linkage: no peer certificate for fd = %d", fd);
        _peer = new SslPeer(version, cipher);
        return true;
    }

    char buffer[8192];
    CLOG.Verbose("Linkage: peer certificate detected for fd = %d", fd);

    if (!X509_NAME_oneline(X509_get_subject_name(x509), buffer, sizeof(buffer))) {
        X509_free(x509);
        return false;
    }

    std::string subject_name = buffer;
    if (!X509_NAME_oneline(X509_get_issuer_name(x509), buffer, sizeof(buffer))) {
        X509_free(x509);
        return false;
    }

    std::string issuer_name = buffer;
    ASN1_INTEGER *asn1 = X509_get_serialNumber(x509);
    if (!asn1) {
        X509_free(x509);
        return false;
    }

    BIGNUM *bn = ASN1_INTEGER_to_BN(asn1, NULL);
    if (!bn) {
        X509_free(x509);
        return false;
    }

    char *serial = BN_bn2hex(bn);
    if (!serial) {
        BN_free(bn);
        X509_free(x509);
        return false;
    }

    std::string serial_number = serial;
    OPENSSL_free(serial);
    BN_free(bn);
    X509_free(x509);

    _peer = new SslPeer(version, cipher,
                        subject_name, issuer_name, serial_number);

    return true;
}

AbstractIo::Status SslIo::Connect()
{
    int fd = _i->fd();
    if (_connecting) {
        if (safe_test_if_connected(fd)) {
            return kStatusError;
        }
    }

    _connecting = false;
    CLOG.Verbose("Linkage: SSL initiating for fd = %d", fd);
    int ret = SSL_connect(_ssl);
    if (ret == 1) { // Handshake completed.
        if (!OnHandshaked()) {
            return kStatusError;
        }

        return kStatusOk;

    } else if (ret == 0) {
        CLOG.Verbose("Linkage: SSL initiating failed and closed for fd = %d", fd);
        return kStatusClosed;
    }

    return HandleError(kActionConnect, ret);
}

AbstractIo::Status SslIo::Accept()
{
    int fd = _i->fd();
    if (_connecting) {
        if (safe_test_if_connected(fd)) {
            return kStatusError;
        }
    }

    _connecting = false;
    CLOG.Verbose("Linkage: SSL handshaking for fd = %d", fd);
    int ret = SSL_accept(_ssl);
    if (ret == 1) { // Handshake completed.
        if (!OnHandshaked()) {
            return kStatusError;
        }

        return kStatusOk;
    }

    return HandleError(kActionAccept, ret);
}

AbstractIo::Status SslIo::Shutdown()
{
    int fd = _i->fd();
    CLOG.Verbose("Linkage: SSL shutdown for fd = %d", fd);
    int ret = SSL_shutdown(_ssl);
    if (ret == 0) {
        CLOG.Verbose("Linkage: SSL unidirectional shutdown for fd = %d", fd);
        ret = SSL_shutdown(_ssl);
    }

    if (ret == 1) {
        CLOG.Verbose("Linkage: SSL bidirectional shutdown for fd = %d", fd);
        if (!_i->Shutdown()) {
            return kStatusError;
        }

        return kStatusClosed;
    }

    return HandleError(kActionShutdown, ret);
}

bool SslIo::Initialize(Action *action,
                       Action *next_action,
                       bool *wanna_read,
                       bool *wanna_write)
{
    if (_connecting) {
        *action      = kActionNone;
        *next_action = _client_mode ? kActionConnect : kActionAccept;
        *wanna_read  = false;
        *wanna_write = true;

    } else {
        *action      = _client_mode ? kActionConnect : kActionAccept;
        *next_action = kActionNone;
        *wanna_read  = false;
        *wanna_write = false;
    }

    return true;
}

} // namespace flinter
