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

#include <stdio.h>

#include <ClearSilver/ClearSilver.h>
#include <google/protobuf/message_lite.h>
#include <fcgiapp.h>
#include <fcgios.h>

#include "flinter/fastcgi/main/fastcgi_main.h"
#include "flinter/fastcgi/dispatcher.h"
#include "flinter/cmdline.h"
#include "flinter/openssl.h"
#include "flinter/utility.h"
#include "flinter/xml.h"

#include "config.h"

#if HAVE_CURL_CURL_H
#include <curl/curl.h>
#endif

int main(int argc, char *argv[])
{
    argv = cmdline_setup(argc, argv);
    if (!argv) {
        fprintf(stderr, "Failed to setup cmdline library.\n");
        return EXIT_FAILURE;
    }

    // Initialize timezone before going multi-threaded.
    tzset();

    // Initialize weak (but fast) PRG rand(3).
    randomize();

    flinter::OpenSSLInitializer openssl_initializer;

#if HAVE_CURL_CURL_H_
    // Initialize libcurl.
    if (curl_global_init(CURL_GLOBAL_ALL)) {
        fprintf(stderr, "Failed to initialize libcurl.\n");
        return EXIT_FAILURE;
    }
#endif

#if HAVE_LIBXML_XMLVERSION_H
    // Initialize XML.
    if (!flinter::Xml::Initialize()) {
        fprintf(stderr, "Failed to initialize libxml2.\n");
        return false;
    }
#endif

    // Initialize FastCGI.
    if (FCGX_Init()) {
        fprintf(stderr, "Failed to initialize FastCGI.\n");
        return false;
    }

    // Initialize ClearSilver.
    // Nothing to do.

    int ret = fastcgi_main(argc, argv);

#if HAVE_NERR_SHUTDOWN
    // ClearSilver leaks.
    NEOERR *err = nerr_shutdown();
    if (err != STATUS_OK) {
        nerr_ignore(&err);
    }
#endif

    // FCGX leaks.
    OS_LibShutdown();

#if HAVE_LIBXML_XMLVERSION_H
    // Shutdown libxml2.
    flinter::Xml::Shutdown();
#endif

#if HAVE_CURL_CURL_H
    // Shutdown libcurl.
    curl_global_cleanup();
#endif

    openssl_initializer.Shutdown();
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}
