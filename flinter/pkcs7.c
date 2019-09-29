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

#include "flinter/pkcs7.h"

#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

static X509 *pkcs7_get_ca_certificate(const char *cacert)
{
    X509 *x509;
    BIO *bp;

    bp = BIO_new_file(cacert, "rb");
    if (!bp) {
        return NULL;
    }

    x509 = PEM_read_bio_X509(bp, NULL, NULL, "");
    BIO_free(bp);
    return x509;
}

static X509_STORE *pkcs7_get_ca_store(const char *cacert)
{
    X509_STORE *store;
    X509 *ca;

    ca = pkcs7_get_ca_certificate(cacert);
    if (!ca) {
        return NULL;
    }

    store = X509_STORE_new();
    if (!store) {
        X509_free(ca);
        return NULL;
    }

    if (X509_STORE_add_cert(store, ca) <= 0) {
        X509_STORE_free(store);
        X509_free(ca);
        return NULL;
    }

    X509_free(ca);
    return store;
}

static PKCS7 *pkcs7_get_signed_file(const char *filename, BIO **content, int format)
{
    PKCS7 *pkcs7;
    BIO *input;

    input = BIO_new_file(filename, "rb");
    if (!input) {
        return NULL;
    }

    *content = NULL;
    switch (format) {
    case PKCS7_INPUT_FORMAT_SMIME:
        pkcs7 = SMIME_read_PKCS7(input, content);
        break;

    case PKCS7_INPUT_FORMAT_PEM:
        pkcs7 = PEM_read_bio_PKCS7(input, NULL, NULL, "");
        break;

    case PKCS7_INPUT_FORMAT_DER:
        pkcs7 = d2i_PKCS7_bio(input, NULL);
        break;

    default:
        abort(); /* Should never reach here. */
        return NULL;
    };

    if (!pkcs7) {
        BIO_free(input);
        return NULL;
    }

    BIO_free(input);
    return pkcs7;
}

int pkcs7_verify_into_memory(const char *cacert,
                             const char *filename,
                             int format,
                             int (*handler)(const void *, size_t, void *),
                             void *parameter)
{
    char *memory;
    long length;

    X509_STORE *store;
    PKCS7 *pkcs7;
    BIO *content;
    BIO *output;

    if (format != PKCS7_INPUT_FORMAT_SMIME  &&
        format != PKCS7_INPUT_FORMAT_PEM    &&
        format != PKCS7_INPUT_FORMAT_DER    ){

        return -1;
    }

    pkcs7 = pkcs7_get_signed_file(filename, &content, format);
    if (!pkcs7) {
        return -2;
    }

    store = pkcs7_get_ca_store(cacert);
    if (!store) {
        PKCS7_free(pkcs7);

        if (content) {
            BIO_free(content);
        }

        return -3;
    }

    output = BIO_new(BIO_s_mem());
    if (!output) {
        X509_STORE_free(store);
        PKCS7_free(pkcs7);

        if (content) {
            BIO_free(content);
        }

        return -4;
    }

    if (PKCS7_verify(pkcs7, NULL, store, content, output, 0) <= 0) {
        BIO_free(output);
        X509_STORE_free(store);
        PKCS7_free(pkcs7);

        if (content) {
            BIO_free(content);
        }

        return -5;
    }

    X509_STORE_free(store);
    PKCS7_free(pkcs7);

    if (content) {
        BIO_free(content);
    }

    length = BIO_get_mem_data(output, &memory);
    if (length < 0 || !memory) {
        BIO_free(output);
        return -6;
    }

    if (handler(memory, (size_t)length, parameter)) {
        BIO_free(output);
        return -7;
    }

    BIO_free(output);
    return 0;
}
