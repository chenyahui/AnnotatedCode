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

#include "flinter/encode.h"

#include <errno.h>
#include <string.h>

#include "config.h"
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

namespace flinter {

int EncodeBase64(const std::string &input, std::string *output)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    if (!output) {
        errno = EINVAL;
        return -1;
    }

    if (input.empty()) {
        output->clear();
        return 0;
    }

    b64 = BIO_new(BIO_f_base64());
    if (!b64) {
        errno = ENOMEM;
        return -1;
    }

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    if (!bmem) {
        BIO_free_all(b64);
        errno = ENOMEM;
        return -1;
    }

    b64 = BIO_push(b64, bmem);
    if (BIO_write(b64,
                  reinterpret_cast<const unsigned char *>(input.data()),
                  static_cast<int>(input.length())) <= 0) {

        BIO_free_all(b64);
        errno = EACCES;
        return -1;
    }

    if (BIO_flush(b64) != 1) {
        BIO_free_all(b64);
        errno = EACCES;
        return -1;
    }

    BIO_get_mem_ptr(b64, &bptr);
    output->resize(static_cast<size_t>(bptr->length));
    memcpy(&output->at(0), bptr->data, static_cast<size_t>(bptr->length));
    BIO_free_all(b64);
    return 0;
}

int DecodeBase64(const std::string &input, std::string *output)
{
    BIO *b64, *bmem;
    size_t buflen;
    size_t size;
    int ret;

    if ((input.length() % 4 != 0) || !output) {
        errno = EINVAL;
        return -1;
    }

    if (input.empty()) {
        output->clear();
        return 0;
    }

    buflen = input.length() / 4 * 3;
    output->resize(buflen);

    b64 = BIO_new(BIO_f_base64());
    if (!b64) {
        errno = ENOMEM;
        return -1;
    }

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    unsigned char *inbuf = reinterpret_cast<unsigned char *>(const_cast<char *>(input.data()));
    bmem = BIO_new_mem_buf(inbuf, static_cast<int>(input.length()));
    if (!bmem) {
        BIO_free_all(b64);
        errno = ENOMEM;
        return -1;
    }

    bmem = BIO_push(b64, bmem);
    ret = BIO_read(bmem, &output->at(0), static_cast<int>(buflen));
    size = static_cast<size_t>(ret);
    if (size < buflen - 2 || size > buflen) {
        BIO_free_all(bmem);
        errno = EACCES;
        return -1;
    }

    output->resize(size);
    BIO_free_all(bmem);
    return 0;
}

} // namespace flinter
#endif // HAVE_OPENSSL_OPENSSLV_H
