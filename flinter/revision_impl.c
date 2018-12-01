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

#include "flinter/revision.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int revision_get_timezone(void)
{
    time_t now;
    time_t ref;
    struct tm tm;

    /* Millennium */
    now = 946684800;
    if (!gmtime_r(&now, &tm)) {
        return 0;
    }

    ref = mktime(&tm);
    if (ref < 0) {
        return 0;
    }

    return (int)(ref - now);
}

static void revision_print_timestamp(time_t timestamp)
{
    struct tm buffer;
    struct tm *tm = localtime_r(&timestamp, &buffer);
    if (tm) {
        int tz = revision_get_timezone();
        printf("%04d-%02d-%02d %02d:%02d:%02d ",
               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (tz == 0) {
            printf("UTC");
        } else {
            printf("%+03d:%02d", -tz / 3600, -tz % 3600 / 60);
        }

    } else {
        printf("%ld", VCS_PROJECT_TIMESTAMP);
    }
}

/* Building environment detection. */
static void revision_print_build_environment(void)
{
#if defined( __GNUC__)
# if defined(__APPLE_CC__)
    printf("Compiler          : Apple GCC\n");
# elif defined(__MINGW32__)
    printf("Compiler          : MinGW GCC\n");
# elif defined(__CYGWIN__)
    printf("Compiler          : Cygwin GCC\n");
# else
    printf("Compiler          : GCC\n");
# endif

# ifdef __VERSION__
    printf("Compiler Version  : %s\n", __VERSION__);
# endif
#elif defined(_MSC_VER)
    printf("Compiler          : Microsoft Visual C++ "
# if _MSC_VER == 1600
           "2010\n"
# elif _MSC_VER == 1500
           "2008\n"
# elif _MSC_VER == 1400
           "2005\n"
# elif _MSC_VER == 1310
           "2003\n"
# elif _MSC_VER == 1300
           "2002\n"
# elif _MSC_VER == 1200
           "6.0\n"
# elif _MSC_VER == 1100
           "5.0\n"
# else
           "%d\n", _MSC_VER
# endif
            );

    printf("Compiler Version  : %d.%d.%d.%d\n", _MSC_FULL_VER / 10000000,
                                                _MSC_FULL_VER / 100000 % 100,
                                                _MSC_FULL_VER % 100000,
                                                _MSC_BUILD);
#else
    printf("Compiler          : N/A\n");
#endif

    /* It's impossible to enumerate them all, just put in something that I know about. */
#if defined(WIN32) || defined(__CYGWIN__)
    printf("Host Platform     : Windows\n");
#elif defined(__APPLE__)
    printf("Host Platform     : Darwin\n");
#elif defined(__FreeBSD__)
    printf("Host Platform     : FreeBSD\n");
#elif defined(__OpenBSD__)
    printf("Host Platform     : OpenBSD\n");
#elif defined(__NetBSD__)
    printf("Host Platform     : NetBSD\n");
#elif defined(__svr4__) || defined(__SVR4)
    printf("Host Platform     : Solaris 2\n");
#elif defined(__VXWORKS__)
    printf("Host Platform     : VxWorks\n");
#elif defined(__hpux__)
    printf("Host Build        : HP-UX\n");
#elif defined(_AIX)
    printf("Host Build        : IBM AIX\n");
#elif defined(__ANDROID__)
    printf("Host Platform     : Android\n");
#elif defined(__linux__) && defined(__GNU__) || defined(__gnu_linux__)
    printf("Host Platform     : GNU/Linux\n");
#elif defined(__linux__)
    printf("Host Platform     : Generic Linux\n");
#elif defined(__unix__)
    printf("Host Platform     : Generic Unix\n");
#else
    printf("Host Platform     : N/A\n");
#endif

#if defined(__i386__) || defined(__i386) || defined(_M_IX86)
    printf("Host Build        : x32\n");
#elif defined(__x86_64__) || defined(_M_X64)
    printf("Host Build        : x64\n");
#elif defined(__sparc__) || defined(__sparc64__)
    printf("Host Build        : SPARC\n");
#elif defined(__ia64__) || defined(__ia64) || defined(_M_IA64)
    printf("Host Build        : Itanium\n");
#elif defined(__mips__) || defined(__mips)
    printf("Host Build        : MIPS\n");
#elif defined(__arm__) || defined(__ARM)
    printf("Host Build        : ARM\n");
#else
    printf("Host Build        : N/A\n");
#endif

    printf("Build hostname    : %s\n", VCS_BUILD_HOSTNAME);
    printf("Build timestamp   : ");
    revision_print_timestamp(VCS_BUILD_TIMESTAMP);
    printf("\n");
}

int revision_get_project_version(char *buffer, size_t len)
{
    int ret;

    if (VCS_PROJECT_VERSION) {
        ret = snprintf(buffer, len, "%s", VCS_PROJECT_VERSION);
        if (ret > 0 && (size_t)ret < len) {
            return 0;
        }
    }

    if (VCS_PROJECT_REVISION >= 0) {
        ret = snprintf(buffer, len, "r%d", VCS_PROJECT_REVISION);
        if (ret > 0 && (size_t)ret < len) {
            return 0;
        }
    }

    return -1;
}

void revision_print(void)
{
    char buffer[256];
    if (revision_get_project_version(buffer, sizeof(buffer))) {
        strcpy(buffer, "N/A");
    }

    if (VCS_NAME) {
        printf("Version Control   : %s\n", VCS_NAME);
    } else {
        printf("Version Control   : N/A\n");
    }

    if (VCS_PROJECT_URL) {
        printf("Project URL       : %s\n", VCS_PROJECT_URL);
    }

    if (VCS_PROJECT_ROOT) {
        printf("Project Root      : %s\n", VCS_PROJECT_ROOT);
    }

    printf("Project Version   : %s\n", buffer);

    if (VCS_PROJECT_REVISION >= 0) {
        printf("Project Revision  : %d\n", VCS_PROJECT_REVISION);
    }

    if (VCS_PROJECT_TIMESTAMP >= 0) {
        printf("Project Timestamp : ");
        revision_print_timestamp(VCS_PROJECT_TIMESTAMP);
        printf("\n");
    }

    printf("\n");
    revision_print_build_environment();
}
