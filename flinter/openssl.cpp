/* Copyright 2016 yiyuanzhong@gmail.com (Yiyuan Zhong)
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

#include "flinter/openssl.h"

#include <stdexcept>

#include "flinter/thread/mutex.h"
#include "flinter/utility.h"

#include "config.h"
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#endif // HAVE_OPENSSL_OPENSSLV_H

namespace flinter {

static Mutex *g_mutex = NULL;

static void Locking(int mode, int n, const char *, int)
{
    if ((mode & CRYPTO_LOCK)) {
        g_mutex[n].Lock();
    } else {
        g_mutex[n].Unlock();
    }
}

OpenSSLInitializer::OpenSSLInitializer() : _initialized(false)
{
#if HAVE_OPENSSL_OPENSSLV_H
    OpenSSL_add_all_algorithms();
    int locks = CRYPTO_num_locks();
    if (locks > 0) {
        g_mutex = new Mutex[locks];
        CRYPTO_set_locking_callback(Locking);
    }

    if (!SSL_library_init()) {
        throw std::runtime_error("SSL_library_init()");
    }

    SSL_load_error_strings();
    _initialized = true;
#endif // HAVE_OPENSSL_OPENSSLV_H
}

OpenSSLInitializer::~OpenSSLInitializer()
{
    Shutdown();
}

void OpenSSLInitializer::Shutdown()
{
    if (!_initialized) {
        return;
    }

#if HAVE_OPENSSL_OPENSSLV_H
    CRYPTO_set_locking_callback(NULL);
#if HAVE_SSL_LIBRARY_CLEANUP
    SSL_library_cleanup();
#endif
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    ERR_remove_thread_state(NULL);
    ERR_free_strings();

    delete [] g_mutex;
    g_mutex = NULL;

    _initialized = false;
#endif // HAVE_OPENSSL_OPENSSLV_H
}

} // namespace flinter
