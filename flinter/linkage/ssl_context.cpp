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

#include "flinter/linkage/ssl_context.h"

#include <vector>

#include "flinter/logger.h"

#include "config.h"
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/ssl23.h>

namespace flinter {

int SslContext::_tls_ticket_key_index = -1;
typedef struct {
    unsigned char hmac[32];
    unsigned char ekey[32];
    unsigned char name[16];
    size_t bits;
} key_t;

SslContext::SslContext(bool enhanced_security)
        : _context(NULL)
        , _enhanced_security(enhanced_security)
{
    Initialize();
}

SslContext::~SslContext()
{
    if (!_context) {
        return;
    }

    std::vector<key_t> *const keys =
            reinterpret_cast<std::vector<key_t> *>(
                    SSL_CTX_get_ex_data(_context, _tls_ticket_key_index));

    if (keys) {
        delete keys;
        SSL_CTX_set_ex_data(_context, _tls_ticket_key_index, NULL);
    }

    SSL_CTX_free(_context);
}

bool SslContext::Initialize()
{
    static const unsigned char kDefaultDHParamG[] = { 0x02 };
    static const unsigned char kDefaultDHParamP[] = {
        0xD8, 0x7F, 0x7C, 0xE6, 0xBD, 0x7A, 0xD0, 0x30, 0x84, 0xDA, 0x77, 0x5B,
        0x44, 0xB2, 0xA0, 0xDE, 0x01, 0x26, 0x9F, 0xB7, 0xC9, 0xF3, 0xBF, 0x33,
        0x5B, 0x36, 0x47, 0x66, 0xC1, 0xCC, 0x4C, 0x53, 0x4B, 0x20, 0xC2, 0x57,
        0xBC, 0x5B, 0x7E, 0x10, 0x56, 0x77, 0xF3, 0xFF, 0x6A, 0x19, 0x0D, 0x36,
        0xE0, 0x12, 0xE7, 0x77, 0x5E, 0x6A, 0xFE, 0x7B, 0x0A, 0x9F, 0x80, 0x7F,
        0xE3, 0xC0, 0xC8, 0xEE, 0x08, 0x5D, 0x5A, 0xC9, 0x61, 0xFC, 0x1C, 0x08,
        0x81, 0xDF, 0x4D, 0x10, 0xA1, 0x8C, 0x7D, 0x21, 0xED, 0x48, 0x83, 0xA0,
        0x25, 0x07, 0x66, 0x24, 0x07, 0x17, 0x6F, 0x62, 0xAB, 0x98, 0x67, 0x27,
        0xD8, 0xB9, 0x54, 0x2E, 0x1E, 0x6B, 0xA7, 0x44, 0x31, 0xAE, 0x8D, 0x5C,
        0xED, 0x0B, 0xCC, 0xF2, 0x6B, 0x2F, 0xA2, 0xF4, 0x07, 0x38, 0x3F, 0x37,
        0x37, 0xAB, 0x2E, 0x06, 0xCB, 0x6B, 0x79, 0xFF, 0xF2, 0x98, 0xF2, 0x80,
        0xE3, 0x31, 0xA7, 0x5D, 0x82, 0xD1, 0xF9, 0xA8, 0x1A, 0x05, 0x02, 0x34,
        0x3D, 0x59, 0xB6, 0x28, 0x53, 0xA4, 0xB9, 0xEF, 0xD8, 0xCA, 0xA3, 0xE8,
        0x22, 0xBD, 0x6D, 0xA9, 0x28, 0x35, 0x4E, 0x1C, 0x25, 0xD2, 0x4A, 0xAD,
        0xAB, 0x85, 0x5B, 0x44, 0xA9, 0xDC, 0x4C, 0x74, 0xCE, 0xEA, 0xA9, 0xFD,
        0xDD, 0x35, 0x29, 0xC3, 0x83, 0xC1, 0x0D, 0x3A, 0x05, 0x44, 0x7F, 0x0F,
        0x44, 0x69, 0x81, 0x67, 0x82, 0x3E, 0x64, 0xF9, 0x16, 0x45, 0xCE, 0x8B,
        0x0C, 0x61, 0x3E, 0x35, 0x8B, 0x06, 0xE3, 0x80, 0x17, 0x27, 0x1C, 0x82,
        0xA6, 0xC2, 0x9F, 0x26, 0x42, 0x51, 0xB3, 0x7E, 0xBF, 0xA2, 0xF6, 0xBF,
        0x6E, 0xB3, 0xB3, 0xB7, 0xCE, 0x48, 0x0C, 0x43, 0x2D, 0x2E, 0x16, 0xCF,
        0x0B, 0xBE, 0x05, 0xDA, 0xE9, 0x39, 0xFD, 0x73, 0x74, 0x38, 0x8D, 0x48,
        0x07, 0x06, 0xFB, 0x53,
    };

    if (_context) {
        return true;
    }

    const SSL_METHOD *method;
    const char *ciphers = NULL;
    if (_enhanced_security) {
        method = TLSv1_2_method();
        ciphers = "ECDH+AESGCM:DH+AESGCM:"
                  "ECDH+AES:DH+AES:"
                  "!AES256:!SHA:!MD5:!DSS:!aNULL:!eNULL";

    } else {
        method = SSLv23_method();
        ciphers = "ECDH+AESGCM:DH+AESGCM:"
                  "ECDH+AES:DH+AES:"
                  "ECDH+3DES:DH+3DES:"
                  "kRSA+AESGCM:kRSA+AES:kRSA+3DES:"
                  "!AES256:!MD5:!DSS:!aNULL:!eNULL";
    }

    DH *dh = DH_new();
    if (!dh) {
        return false;
    }

    dh->p = BN_bin2bn(kDefaultDHParamP, sizeof(kDefaultDHParamP), NULL);
    dh->g = BN_bin2bn(kDefaultDHParamG, sizeof(kDefaultDHParamG), NULL);
    if (!dh->p || !dh->g) {
        DH_free(dh);
        return false;
    }

    EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ecdh) {
        DH_free(dh);
        return false;
    }

    _context = SSL_CTX_new(method);
    if (!_context) {
        EC_KEY_free(ecdh);
        DH_free(dh);
        return false;
    }

    if (SSL_CTX_set_tmp_ecdh(_context, ecdh) != 1 ||
        SSL_CTX_set_tmp_dh(_context, dh) != 1     ){

        SSL_CTX_free(_context);
        _context = NULL;
        EC_KEY_free(ecdh);
        DH_free(dh);
        return false;
    }

    EC_KEY_free(ecdh);
    DH_free(dh);

    if (SSL_CTX_set_cipher_list(_context, ciphers) != 1) {
        SSL_CTX_free(_context);
        _context = NULL;
        return false;
    }

    // Disable session resumptions by default to provide maximum security.
    SSL_CTX_set_session_cache_mode(_context, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_options(_context, SSL_OP_NO_TICKET);

    // It's known vulnerable.
    SSL_CTX_set_options(_context, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(_context, SSL_OP_NO_SSLv3);
    SSL_CTX_set_options(_context, SSL_OP_NO_COMPRESSION);

    // Always use ephemeral keys.
    SSL_CTX_set_options(_context, SSL_OP_SINGLE_DH_USE);
    SSL_CTX_set_options(_context, SSL_OP_SINGLE_ECDH_USE);

    // Listen to me.
    SSL_CTX_set_options(_context, SSL_OP_CIPHER_SERVER_PREFERENCE);

    // Extra settings.
    SSL_CTX_set_mode(_context, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_CTX_set_default_passwd_cb(_context, PasswordCallback);
    SSL_CTX_set_default_passwd_cb_userdata(_context, NULL);
    return true;
}

bool SslContext::LoadCertificateChain(const std::string &filename)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    if (!SSL_CTX_use_certificate_chain_file(_context, filename.c_str())) {
        return false;
    }

    return true;
}

bool SslContext::LoadDHParam(const std::string &filename)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) {
        return false;
    }

    DH *dh = PEM_read_DHparams(file, NULL, NULL, NULL);
    fclose(file);

    if (!dh) {
        return false;
    }

    if (SSL_CTX_set_tmp_dh(_context, dh) != 1) {
        DH_free(dh);
        return false;
    }

    DH_free(dh);
    return true;
}

bool SslContext::LoadCertificate(const std::string &filename)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    if (!SSL_CTX_use_certificate_file(_context,
                                      filename.c_str(),
                                      SSL_FILETYPE_PEM)) {

        return false;
    }

    return true;
}

bool SslContext::LoadPrivateKey(const std::string &filename,
                                const std::string &passphrase)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    SSL_CTX_set_default_passwd_cb_userdata(_context,
                                           const_cast<std::string *>(&passphrase));

    int ret = SSL_CTX_use_PrivateKey_file(_context,
                                          filename.c_str(),
                                          SSL_FILETYPE_PEM);

    SSL_CTX_set_default_passwd_cb_userdata(_context, NULL);
    if (!ret) {
        return false;
    }

    return true;
}

int SslContext::PasswordCallback(char *buf, int size, int /*rwflag*/, void *userdata)
{
    const std::string *s = reinterpret_cast<const std::string *>(userdata);
    if (!s) {
        return -1;
    }

    int ret = snprintf(buf, static_cast<size_t>(size), "%s", s->c_str());
    if (ret < 0 || ret >= size) {
        return -1;
    }

    return ret;
}

bool SslContext::VerifyPrivateKey()
{
    if (!Initialize()) {
        return false;
    }

    if (!SSL_CTX_check_private_key(_context)) {
        return false;
    }

    return true;
}

bool SslContext::SetVerifyPeer(bool verify, bool required)
{
    if (!Initialize()) {
        return false;
    }

    int mode = verify ? SSL_VERIFY_CLIENT_ONCE
                      | SSL_VERIFY_PEER
                      : SSL_VERIFY_NONE;

    if (required) {
        mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }

    SSL_CTX_set_verify(_context, mode, NULL);
    return true;
}

bool SslContext::AddTrustedCACertificate(const std::string &filename)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    BIO *bp = BIO_new_file(filename.c_str(), "rb");
    if (!bp) {
        return false;
    }

    X509 *x509 = PEM_read_bio_X509(bp, NULL, NULL, (void *)"");
    BIO_free(bp);

    if (!x509) {
        return false;
    }

    X509_STORE *store = SSL_CTX_get_cert_store(_context);
    if (!store) {
        X509_free(x509);
        return false;
    }

    if (X509_STORE_add_cert(store, x509) != 1       ||
        SSL_CTX_add_client_CA(_context, x509) != 1  ){

        X509_free(x509);
        return false;
    }

    X509_free(x509);
    return true;
}

bool SslContext::SetSessionIdContext(const std::string &context)
{
    if (context.empty() || context.length() > SSL_MAX_SSL_SESSION_ID_LENGTH) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    SSL_CTX_set_session_cache_mode(_context, SSL_SESS_CACHE_SERVER);
    if (SSL_CTX_set_session_id_context(_context,
            reinterpret_cast<const unsigned char *>(context.data()),
            static_cast<unsigned int>(context.length())) != 1) {

        return false;
    }

    return true;
}

bool SslContext::SetSessionTimeout(int seconds)
{
    if (!seconds) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    SSL_CTX_set_timeout(_context, seconds);
    return true;
}

bool SslContext::SetAllowTlsTicket(bool allow)
{
    if (!Initialize()) {
        return false;
    }

    if (allow) {
        SSL_CTX_clear_options(_context, SSL_OP_NO_TICKET);
    } else {
        SSL_CTX_set_options(_context, SSL_OP_NO_TICKET);
    }

    return true;
}

ssize_t SslContext::LoadFile(
        const std::string &filename,
        void *buffer, size_t length)
{
    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END)) {
        fclose(fp);
        return -1;
    }

    long size = ftell(fp);
    if (size < 0 || static_cast<size_t>(size) > length) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET)) {
        fclose(fp);
        return -1;
    }

    size_t ret = fread(buffer, 1, static_cast<size_t>(size), fp);
    if (ret != static_cast<size_t>(size)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return size;
}

bool SslContext::LoadTlsTicketKey(const std::string &filename)
{
    if (filename.empty()) {
        return false;
    } else if (!Initialize()) {
        return false;
    }

    char buffer[128];
    ssize_t ret = LoadFile(filename, buffer, sizeof(buffer));
    if (ret != 48 && ret != 80) {
        return false;
    }

    // Initializing static data, a mutex shall be implemented but it's usually
    // not necessary and will slow down every session establishing.
    if (_tls_ticket_key_index < 0) {
        _tls_ticket_key_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
        if (_tls_ticket_key_index < 0) {
            return false;
        }
    }
    // End of initializing static data

    std::vector<key_t> *keys =
            reinterpret_cast<std::vector<key_t> *>(
                    SSL_CTX_get_ex_data(_context, _tls_ticket_key_index));

    if (!keys) {
        keys = new std::vector<key_t>;
        if (SSL_CTX_set_ex_data(_context, _tls_ticket_key_index, keys) != 1) {
            delete keys;
            return false;
        }

        // keys are kept in context and destroyed in dtor.
        if (SSL_CTX_set_tlsext_ticket_key_cb(_context, TlsTicketKeyCallback) != 1) {
            return false;
        }
    }

    key_t key;
    if (ret == 48) {
        key.bits = 128;
        memcpy(key.name, buffer +  0, 16);
        memcpy(key.hmac, buffer + 16, 16);
        memcpy(key.ekey, buffer + 32, 16);
    } else {
        key.bits = 256;
        memcpy(key.name, buffer +  0, 16);
        memcpy(key.hmac, buffer + 16, 32);
        memcpy(key.ekey, buffer + 48, 32);
    }

    keys->push_back(key);
    return true;
}

int SslContext::TlsTicketKeyCallback(
        SSL *s,
        unsigned char *key_name,
        unsigned char *iv,
        EVP_CIPHER_CTX *ctx,
        HMAC_CTX *hctx,
        int enc)
{
#ifdef OPENSSL_NO_SHA256
    const EVP_MD *const digest = EVP_sha1();
#else
    const EVP_MD *const digest = EVP_sha256();
#endif

    SSL_CTX *const context = SSL_get_SSL_CTX(s);
    const std::vector<key_t> *const keys =
            reinterpret_cast<std::vector<key_t> *>(
                    SSL_CTX_get_ex_data(context, _tls_ticket_key_index));

    if (!keys || keys->empty()) {
        CLOG.Error("SSL: BUG session ticket keys are not set");
        return -1;
    }

    if (enc) {
        const key_t *const key = &keys->at(0);
        const EVP_CIPHER *cipher;
        int size;

        if (key->bits == 128) {
            cipher = EVP_aes_128_cbc();
            size = 16;
        } else {
            cipher = EVP_aes_256_cbc();
            size = 32;
        }

        if (RAND_bytes(iv, EVP_CIPHER_iv_length(cipher))         != 1 ||
            EVP_EncryptInit_ex(ctx, cipher, NULL, key->ekey, iv) != 1 ||
            HMAC_Init_ex(hctx, key->hmac, size, digest, NULL)    != 1 ){

            return -1;
        }

        memcpy(key_name, key->name, 16);
        CLOG.Verbose("SSL: new session ticket generated");
        return 1;

    } else {
        const key_t *key;
        bool found = false;
        bool renew = false;
        for (std::vector<key_t>::const_iterator p = keys->begin(); p != keys->end(); ++p) {
            if (memcmp(key_name, p->name, 16) == 0) {
                found = true;
                key = &*p;
                break;
            }

            renew = true;
        }

        if (!found) {
            CLOG.Verbose("SSL: session ticket key name is not found");
            return 0;
        }

        const EVP_CIPHER *cipher;
        int size;

        if (key->bits == 128) {
            cipher = EVP_aes_128_cbc();
            size = 16;
        } else {
            cipher = EVP_aes_256_cbc();
            size = 32;
        }

        if (HMAC_Init_ex(hctx, key->hmac, size, digest, NULL)    != 1 ||
            EVP_DecryptInit_ex(ctx, cipher, NULL, key->ekey, iv) != 1 ){

            return -1;
        }

        CLOG.Verbose("SSL: session ticket %s", renew ? "needs renewal" : "accepted");
        return renew ? 2 : 1;
    }
}

} // namespace flinter

#endif // HAVE_OPENSSL_OPENSSLV_H
