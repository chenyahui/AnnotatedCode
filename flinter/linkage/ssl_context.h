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

#ifndef FLINTER_LINKAGE_SSL_CONTEXT_H
#define FLINTER_LINKAGE_SSL_CONTEXT_H

#include <string>

struct evp_cipher_ctx_st;
struct hmac_ctx_st;
struct ssl_ctx_st;
struct ssl_st;

namespace flinter {

// Warning: make sure you have sufficient knowledge of TLS security, especially
//          perfect forward secrecy which is much much more important than key
//          types, key length or ciphers, before you turn on session resumption.
class SslContext {
public:
    explicit SslContext(bool enhanced_security = true);
    ~SslContext();

    // Enable session resumption with session tickets.
    //
    // Tickets are instance locked, that is, different instances of the same
    // application cannot share tickets, or even restarting the same instance
    // will invalidates all previous tickets.
    //
    // Session tickets don't require internal session cache but it's still
    // possible to break PFS, since session ticket keys are still cached within
    // instance memory, long term or short term. When clients try to resume
    // sessions after session ticket key has been compromised, malicious users
    // can obtain all session keys and decrypt all traffic.
    bool SetAllowTlsTicket(bool allow);

    // Only necessary for session resumptions (ID or ticket based).
    bool SetSessionTimeout(int seconds);

    // Always load external DH parameters.
    //
    // Although built-in DH parameters are generally secure, it's a good
    // practice to always subsitute with one that matches your private key
    // length and cipher choices.
    bool LoadDHParam(const std::string &filename);

    // This method will enable internal session cache.
    //
    // Session cache can be dangerous since it saves session keys inside memory
    // temporarily. Malicious codes can extract keys from memory if they have
    // sufficient rights to read virtual/physical memory.
    bool SetSessionIdContext(const std::string &context);

    // To enable verifying, it's necessary to enable internal session cache by
    // calling SetSessionIdContext(). This requirement is a limitation of the
    // underlying cryptographic implementation (OpenSSL) and might be removed
    // if I switch to some other implementations later.
    bool SetVerifyPeer(bool verify, bool required = true);

    // Necessary for verifying peers.
    bool AddTrustedCACertificate(const std::string &filename);

    // Strongly recommended to verify private keys after loading.
    bool VerifyPrivateKey();
    bool LoadCertificate(const std::string &filename);
    bool LoadCertificateChain(const std::string &filename);
    bool LoadPrivateKey(const std::string &filename, const std::string &passphrase);

    // Using this method is strongly OPPOSED, again, DO NOT USE THIS UNLESS
    // YOU KNOW WHAT YOU'RE DOING. Using static session ticket key invalidates
    // perfect forward secrecy since all PFS negotiated keys are ultimately
    // encrypted by static keys, which are saved in files and distributed to
    // multiple places.
    //
    // Key format and rotation are compatible with nginx: first loaded file
    // will be used for signing new tickets, while others can be used to
    // decrypt tickets.
    //
    // Must protect with a global mutex since it's using static data.
    // Usually this method is called at beginning of application so it's fine.
    //
    // To generate a key file:
    // openssl rand 80 > ticket.key
    bool LoadTlsTicketKey(const std::string &filename);

    struct ssl_ctx_st *context()
    {
        return _context;
    }

private:
    static int TlsTicketKeyCallback(
            struct ssl_st *s,
            unsigned char *key_name,
            unsigned char *iv,
            evp_cipher_ctx_st *ctx,
            hmac_ctx_st *hctx,
            int enc);

    static ssize_t LoadFile(const std::string &filename, void *buffer, size_t length);
    static int PasswordCallback(char *buf, int size, int rwflag, void *userdata);
    bool Initialize();

    static int _tls_ticket_key_index;
    struct ssl_ctx_st *_context;
    bool _enhanced_security;

}; // class SslContext

} // namespace flinter

#endif // FLINTER_LINKAGE_SSL_CONTEXT_H
