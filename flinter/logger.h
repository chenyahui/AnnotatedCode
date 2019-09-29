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

#ifndef FLINTER_LOGGER_H
#define FLINTER_LOGGER_H

#include <stdarg.h>

#include <sstream>
#include <string>

namespace flinter {

class CLogger {
public:
    enum Level {
        kLevelFatal   = 0, // The application is crashing.
        kLevelError   = 1, // The current operation is to be aborted.
        kLevelWarn    = 2, // Something wrong but the current operation can still go on.
        kLevelInfo    = 3, // It's about business, something to be archived.
        kLevelTrace   = 4, // It's trival but might provide some useful information.
        kLevelDebug   = 5, // Only when you're developing.
        kLevelVerbose = 6, // Turn on detailed library operating logs.

    }; // enum Level

    bool Fatal(const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Error(const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Warn (const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Info (const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Trace(const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Debug(const char *format, ...) __attribute__ ((format (printf, 2, 3)));

    bool Warning(const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    bool Verbose(const char *format, ...) __attribute__ ((format (printf, 2, 3)));

    static bool IsFiltered(int level);
    static void SetFilter(int filter_level);

    static bool ProcessAttach(const std::string &filename,
                              int filter_level = kLevelTrace);

    static void ProcessDetach();

    static bool ThreadAttach();
    static void ThreadDetach();

    static void SetColorful(bool colorful);
    static void SetWithFilename(bool filename);

    // Don't call this thing explicitly.
    CLogger(const char *file, int line) : _file(file), _line(line) {}

protected:
    const char *_file;
    int _line;

}; // class Logger

class Logger : public CLogger {
public:
    Logger(const char *file, int line, const Level &level)
            : CLogger(file, line), _level(level) {}

    virtual ~Logger();

    template <class T>
    Logger &operator <<(const T &t)
    {
        _buffer << t;
        return *this;
    }

private:
    std::ostringstream _buffer;
    Level _level;

}; // class Logger

} // namespace flinter

#define _FLINTER_LOG(x)         (::flinter::Logger(__FILE__, __LINE__, (x)))
#define _FLINTER_LOG_FATAL      _FLINTER_LOG(::flinter::Logger::kLevelFatal)
#define _FLINTER_LOG_ERROR      _FLINTER_LOG(::flinter::Logger::kLevelError)
#define _FLINTER_LOG_WARNING    _FLINTER_LOG(::flinter::Logger::kLevelWarn)
#define _FLINTER_LOG_WARN       _FLINTER_LOG(::flinter::Logger::kLevelWarn)
#define _FLINTER_LOG_INFO       _FLINTER_LOG(::flinter::Logger::kLevelInfo)
#define _FLINTER_LOG_TRACE      _FLINTER_LOG(::flinter::Logger::kLevelTrace)
#define _FLINTER_LOG_DEBUG      _FLINTER_LOG(::flinter::Logger::kLevelDebug)
#define _FLINTER_LOG_VERBOSE    _FLINTER_LOG(::flinter::Logger::kLevelVerbose)

#define CLOG (::flinter::CLogger(__FILE__, __LINE__))
#define LOG(x) _FLINTER_LOG_##x

#endif // FLINTER_LOGGER_H
