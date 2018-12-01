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

#include "flinter/logger.h"

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>
#include <string>

#include "flinter/thread/read_write_lock.h"
#include "flinter/thread/write_locker.h"
#include "flinter/thread/read_locker.h"
#include "flinter/cmdline.h"
#include "flinter/mkdirs.h"
#include "flinter/safeio.h"
#include "flinter/utility.h"

namespace flinter {
namespace {

static const size_t kMaximumLineLength = 2048;
static const size_t kMaximumFileLength = 256;

// 6 might not really be correct since PID range vary.
#define PID_LENGTH 6
#define PID_LENGTH_STRING "6"

#ifdef NDEBUG
static Logger::Level g_filter = Logger::kLevelTrace;
#else
static Logger::Level g_filter = Logger::kLevelDebug;
#endif

static bool g_with_filename = true;
static std::string g_filename;
static bool g_colorful = true;
static ReadWriteLock g_mutex;
static struct stat g_fstat;
static time_t g_stated;
static int g_fd = -1;

static void DoProcessDetach()
{
    safe_close(g_fd);
    g_fd = -1;
}

static bool DoProcessAttach(const std::string &filename)
{
    std::string file(filename);
    char *fn = cmdline_get_absolute_path(filename.c_str(), 0);
    if (fn) {
        file = fn;
        free(fn);
    }

    size_t pos = file.rfind('/');
    if (pos != std::string::npos) {
        std::string path = file.substr(0, pos);
        if (mkdirs(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)) {
            return false;
        }
    }

    // Create file with permission 0666, umask applies.
    int fd = open(filename.c_str(),
                  O_CREAT | O_APPEND | O_WRONLY | O_NOFOLLOW | O_NOCTTY,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if (fd < 0) {
        return false;
    }

    if (set_close_on_exec(fd)) {
        safe_close(fd);
        return false;
    }

    if (g_fd >= 0) {
        safe_close(g_fd);
    }

    fstat(fd, &g_fstat);
    time(&g_stated);
    g_fd = fd;

    return true;
}

static bool Write(time_t now, const void *buffer, size_t length)
{
    ReadLocker rlocker(&g_mutex);

    bool reopen = false;
    if (g_filename.empty()) {
        // Nothing

    } else if (g_fd < 0) {
        reopen = true;

    } else if (now < g_stated || now - g_stated > 5) {
        g_stated = now;
        struct stat st;
        if (stat(g_filename.c_str(), &st) ||
            st.st_dev != g_fstat.st_dev   ||
            st.st_ino != g_fstat.st_ino   ){

            reopen = true;
        }
    }

    if (reopen) {
        time_t stated = g_stated;
        rlocker.Unlock();
        WriteLocker wlocker(&g_mutex);

        // Multiple threads can enter here one by one.
        if (stated == g_stated) {
            DoProcessDetach();
            if (!DoProcessAttach(g_filename)) {
                return false;
            }
        }

        wlocker.Unlock();
        rlocker.Relock();
    }

    int fd = g_fd < 0 ? STDERR_FILENO : g_fd;
    ssize_t sret = safe_write(fd, buffer, length);
    return static_cast<size_t>(sret) == length;
}

static bool Log(const Logger::Level &level,
                const char *file, int line,
                const char *format, va_list va)
{
    if (level > g_filter) {
        return true;
    }

    struct tm tm;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    char buffer[48 + kMaximumLineLength + kMaximumFileLength + 20];
    bool color = false;
    char *next;

    // Not that good, but separated logic can save my brain.
    if (g_colorful) {
        switch (level) {
        case Logger::kLevelFatal   : memcpy(buffer, "FATAL ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[1;37;41m", 10);
                                     next = buffer + 39 + PID_LENGTH;
                                     color = true;
                                     break;
        case Logger::kLevelError   : memcpy(buffer, "ERROR ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[1;31m", 7);
                                     next = buffer + 36 + PID_LENGTH;
                                     color = true;
                                     break;
        case Logger::kLevelWarn    : memcpy(buffer, "WARN  ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[1;33m", 7);
                                     next = buffer + 36 + PID_LENGTH;
                                     color = true;
                                     break;
        case Logger::kLevelInfo    : memcpy(buffer, "INFO  ", 6);
                                     next = buffer + 29 + PID_LENGTH;
                                     break;
        case Logger::kLevelTrace   : memcpy(buffer, "TRACE ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[0;36m", 7);
                                     next = buffer + 36 + PID_LENGTH;
                                     color = true;
                                     break;
        case Logger::kLevelDebug   : memcpy(buffer, "DEBUG ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[0;32m", 7);
                                     next = buffer + 36 + PID_LENGTH;
                                     color = true;
                                     break;
        case Logger::kLevelVerbose : memcpy(buffer, "MINOR ", 6);
                                     memcpy(buffer + 29 + PID_LENGTH, "\033[0;34m", 7);
                                     next = buffer + 36 + PID_LENGTH;
                                     color = true;
                                     break;
        default                    : memcpy(buffer, "OTHER ", 6);
                                     next = buffer + 29 + PID_LENGTH;
                                     break;
        };

    } else {
        next = buffer + 29 + PID_LENGTH;
        switch (level) {
        case Logger::kLevelFatal   : memcpy(buffer, "FATAL ", 6); break;
        case Logger::kLevelError   : memcpy(buffer, "ERROR ", 6); break;
        case Logger::kLevelWarn    : memcpy(buffer, "WARN  ", 6); break;
        case Logger::kLevelInfo    : memcpy(buffer, "INFO  ", 6); break;
        case Logger::kLevelTrace   : memcpy(buffer, "TRACE ", 6); break;
        case Logger::kLevelDebug   : memcpy(buffer, "DEBUG ", 6); break;
        case Logger::kLevelVerbose : memcpy(buffer, "MINOR ", 6); break;
        default                    : memcpy(buffer, "OTHER ", 6); break;
        };
    }

#ifdef __linux__
    long tid = static_cast<long>(get_current_thread_id());
#else
    // For UNIX/MacOSX, native thread id is quite unfamiliar.
    long tid = getpid();
#endif

    ssize_t sret = sprintf(buffer + 6,
                   "%02d-%02d %02d:%02d:%02d.%06ld %" PID_LENGTH_STRING "ld",
                   tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec,
                   static_cast<long>(tv.tv_usec),
                   tid);

    if (sret != 22 + PID_LENGTH) {
        return false;
    }

    // Fix the '\0'.
    buffer[28 + PID_LENGTH] = ' ';

    // Don't occupy the last byte with '\0'.
    sret = vsnprintf(next, kMaximumLineLength + 1, format, va);
    if (sret < 0) {
        return false;
    } else if (static_cast<size_t>(sret) > kMaximumLineLength) {
        memset(next + kMaximumLineLength - 3, '.', 3);
        sret = kMaximumLineLength;
    }

    size_t ret = static_cast<size_t>(sret);
    next += ret;

    if (g_with_filename) {
        memcpy(next, " [", 2);
        next += 2;

        ret = strlen(file);
        if (ret > kMaximumFileLength) {
            file += ret - kMaximumFileLength;
            ret = kMaximumFileLength;
        }

        memcpy(next, file, ret);
        next += ret;
        *next++ = ':';
        sret = sprintf(next, "%d", line);
        if (sret < 0) {
            return false;
        }

        next += sret;
        *next++ = ']';
    }

    if (color) {
        memcpy(next, "\033[0m", 4);
        next += 4;
    }

    *next++ = '\n';
    ret = static_cast<size_t>(next - buffer);
    return Write(tv.tv_sec, buffer, ret);
}

static bool Log(const Logger::Level &level,
                const char *file, int line,
                const char *format, ...)
{
    va_list va;
    va_start(va, format);
    bool ret = Log(level, file, line, format, va);
    va_end(va);
    return ret;
}

} // anonymous namespace

void CLogger::SetWithFilename(bool with_filename)
{
    g_with_filename = with_filename;
}

void CLogger::SetColorful(bool colorful)
{
    g_colorful = colorful;
}

void CLogger::SetFilter(int filter_level)
{
    g_filter = static_cast<Level>(filter_level);
}

bool CLogger::IsFiltered(int level)
{
    return level > g_filter;
}

bool CLogger::ProcessAttach(const std::string &filename, int filter_level)
{
    SetFilter(filter_level);

    WriteLocker locker(&g_mutex);
    g_filename.clear();
    DoProcessDetach();

    if (filename.empty()) {
        return true;
    }

    if (!DoProcessAttach(filename)) {
        return false;
    }

    g_filename = filename;
    return true;
}

void CLogger::ProcessDetach()
{
    WriteLocker locker(&g_mutex);
    g_filename.clear();
    DoProcessDetach();
}

bool CLogger::ThreadAttach()
{
    return true;
}

void CLogger::ThreadDetach()
{
    // Intended left blank.
}

#define LOGGER(name, level) \
bool CLogger::name(const char *format, ...) { \
    va_list va; \
    va_start(va, format); \
    bool ret = Log(kLevel##level, _file, _line, format, va); \
    va_end(va); \
    return ret; \
}

LOGGER(Fatal,   Fatal);
LOGGER(Error,   Error);
LOGGER(Warn,    Warn);
LOGGER(Info,    Info);
LOGGER(Trace,   Trace);
LOGGER(Debug,   Debug);
LOGGER(Warning, Warn);
LOGGER(Verbose, Verbose);

Logger::~Logger()
{
    std::string s = _buffer.str();
    Log(_level, _file, _line, "%s", s.c_str());
}

} // namespace flinter
