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

#ifndef FLINTER_PKCS7_H
#define FLINTER_PKCS7_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKCS7_INPUT_FORMAT_SMIME    1
#define PKCS7_INPUT_FORMAT_PEM      2
#define PKCS7_INPUT_FORMAT_DER      3

/**
 * To verify certain algorithms, caller should OpenSSL_add_all_algorithms() beforehand.
 * Callback:
 *     int callback(const void *buffer, size_t buflen, void *parameter);
 *
 * Calling this method might require caller to do the following cleanup:
 *     EVP_cleanup();
 *     CRYPTO_cleanup_all_ex_data();
 *     ERR_remove_thread_state(NULL);
 *
 * @param cacert    CA certificate to load, can be NULL
 * @param filename  source file to read, either S/MIME or attached PEM/DER.
 * @param format    1 for S/MIME, 2 for PEM, 3 for DER
 * @param handler   method to process the resulting buffer
 * @param parameter passed to handler untouched
 */
extern int pkcs7_verify_into_memory(const char *cacert,
                                    const char *filename,
                                    int format,
                                    int (*handler)(const void *, size_t, void *),
                                    void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* FLINTER_PKCS7_H */
